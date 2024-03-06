/**
    CS466 LAB 2 - Hardware, Development Tools and Blinking the LED using FreeRTOS
    Ilya Cable
    2/21/2024
 */

#include <stdio.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"

#define OUTPUT_1 22

const uint8_t LED_PIN = 25;
const uint8_t SW1_PIN = 17;
const uint8_t SW2_PIN = 16;

const uint8_t SW1_delay = 33;       //ms, 15Hz
const uint8_t SW2_delay = 38;       //ms, 13Hz
const uint8_t SW1and2_delay = 100;  //ms,  5Hz

const uint8_t guard_time = 50;  //ms, my switch has a very deep stroke and 25ms wasn't quite enough

uint32_t milliseconds;
uint32_t milliseconds_last = 0;
TickType_t ticks;

static SemaphoreHandle_t _semBtn1 = NULL;
static SemaphoreHandle_t _semBtn2 = NULL;
static SemaphoreHandle_t _bothBtn = NULL;

void gpio_int_callback(uint gpio, uint32_t events_unused) 
{
    //Ignore button presses that occur within 25ms of each other:
    ticks = xTaskGetTickCount();
    milliseconds = ticks * portTICK_PERIOD_MS;      //Get current time
    if(milliseconds-milliseconds_last < guard_time) return; //Exit if it hasn't been guard time
  
    if (gpio == SW1_PIN)
    {
        xSemaphoreGiveFromISR(_semBtn1, NULL);      //Release SW1 semaphore
    }
    if (gpio == SW2_PIN)
    {
        xSemaphoreGiveFromISR(_semBtn2, NULL);      //Release SW2 semaphore
    }
    milliseconds_last = ticks * portTICK_PERIOD_MS; //Assign time
}

void hardware_init(void)
{
    //Setup LED
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    //Setup oscilliscope output pin
    gpio_init(OUTPUT_1);
    gpio_set_dir(OUTPUT_1, GPIO_OUT);
    //Setup SW1
    gpio_init(SW1_PIN);
    gpio_pull_up(SW1_PIN);
    gpio_set_dir(SW1_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(SW1_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_int_callback);
    //Setup SW2
    gpio_init(SW2_PIN);
    gpio_pull_up(SW2_PIN);
    gpio_set_dir(SW2_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(SW2_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_int_callback);
}

//Function for controlling both LED and oscilliscope output pin:
void led_control(bool isOn)
{
    gpio_put(LED_PIN, isOn);
    gpio_put(OUTPUT_1, isOn);
}

//Task to blink LED 20 times at specificed frequency when SW1 is pressed:
void sw1_handler(void * notUsed)
{
    while (true)
    {
        xSemaphoreTake( _semBtn1, portMAX_DELAY);
        vTaskDelay(50);
        bool SW2_state = !gpio_get(SW2_PIN); //True when other button is pressed
        if(SW2_state) {xSemaphoreGive(_bothBtn);continue;}
        printf("sw1 Semaphore taken..\n");
        for(int i = 0; i < 20; i++){
            vTaskDelay(SW1_delay);
            led_control(false);
            vTaskDelay(SW1_delay);
            led_control(true);
        }
    }
}

//Task to blink LED 10 times at specificed frequency when SW2 is pressed:
void sw2_handler(void * notUsed)
{
    while (true)
    {
        xSemaphoreTake( _semBtn2, portMAX_DELAY);
        vTaskDelay(50);
        bool SW1_state = !gpio_get(SW1_PIN); //True when other button is pressed
        if(SW1_state) {xSemaphoreGive(_bothBtn);continue;}
        printf("sw2 Semaphore taken..\n");
        for(int i = 0; i < 10; i++){
            vTaskDelay(SW2_delay);
            led_control(false);
            vTaskDelay(SW2_delay);
            led_control(true);
        }
    }
}

//Task for when no buttons are pressed:
void heartbeat(void * notUsed)
{   
    while (true) {
        led_control(true);
        vTaskDelay(500);//ms
        led_control(false);
        vTaskDelay(500);
    }
}

//Task to blink for 1 second @ 20Hz if no inputs detected for 1 minute
void remind_available(void * notUsed)
{ 
    uint32_t time_since_press;
    while (true) {
        time_since_press = (xTaskGetTickCount() * portTICK_PERIOD_MS)/1000 - milliseconds_last/1000; //seconds
        //printf("%d\n",time_since_press);
        
        if(time_since_press >= 60){
            for(int i = 0; i < 20; i++){
                vTaskDelay(25);
                led_control(false);
                vTaskDelay(25);
                led_control(true);
            }
            milliseconds_last = (xTaskGetTickCount() * portTICK_PERIOD_MS); //Reset time delay
        }
        vTaskDelay(1000);
    }
}

//Task for when both buttons are pressed:
void bothBtn_handler(void * notUsed)
{
    while(true){
        xSemaphoreTake(_bothBtn,portMAX_DELAY);
        while(!gpio_get(SW1_PIN) && !gpio_get(SW2_PIN)){
            vTaskDelay(100);
            led_control(false);
            vTaskDelay(100);
            led_control(true);
        }
    }
}
int main()
{
    stdio_init_all();
    printf("lab2 Hello!\n");
    hardware_init();

    _semBtn1 = xSemaphoreCreateBinary();                //Semaphore for SW1
    _semBtn2 = xSemaphoreCreateBinary();                //Semaphore for SW2
    _bothBtn = xSemaphoreCreateBinary();                //Semaphore for when both SW1 and SW2
    
    //Start tasks
    xTaskCreate(heartbeat, "LED_Task", 256, NULL, 2, NULL);
    xTaskCreate(sw1_handler, "SW1_Task", 256, NULL, 1, NULL);
    xTaskCreate(sw2_handler, "SW2_Task", 256, NULL, 1, NULL);
    xTaskCreate(remind_available, "Remind_Available", 256, NULL, 3, NULL);
    xTaskCreate(bothBtn_handler, "Both_buttons_pressed", 256, NULL, 3, NULL);
    
    vTaskStartScheduler();

    while(1){};
}
