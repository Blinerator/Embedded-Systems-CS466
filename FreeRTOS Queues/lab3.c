/**
 * CS466 Lab 3 - Simple Queues, Producer/Consumer, Serial IO and gdb debugging
 * Ilya Cable 2/24/2024
 */

#include <stdio.h>
#include <stdlib.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
//#include <semphr.h>

#include "hardware/gpio.h"
#include "pico/stdlib.h"
#include "myAssert.h"
#define OUTPUT_1 18

const uint8_t LED_PIN = 25;
const uint8_t SW1_PIN = 17;
const uint8_t SW2_PIN = 16;
bool SW1_state = false;
bool SW2_state = false;
TickType_t ticks;

//define a producer characterization data structure:
struct producer_data{
    QueueHandle_t queue;
    char thread_name[16];
    int mean_delay;
    int priority;
    uint32_t data;
};

void hardware_init(void)
{
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    gpio_init(SW1_PIN);
    gpio_pull_up(SW1_PIN);
    gpio_set_dir(SW1_PIN, GPIO_IN);
    
    gpio_init(SW2_PIN);
    gpio_pull_up(SW2_PIN);
    gpio_set_dir(SW2_PIN, GPIO_IN);
}

//No buttons are pressed:
void heartbeat(void * notUsed)
{   int cnt = 0;
    while (true) {
        ++cnt;
        myAssert(cnt<100);                          //For testing purposes
        SW1_state = !gpio_get(SW1_PIN);             //True if button is pressed
        SW2_state = !gpio_get(SW2_PIN);
        if(!(SW1_state || SW2_state)){              //Blink if neither button is pressed
            gpio_put(LED_PIN,true);
            vTaskDelay(500);                        //ms
            gpio_put(LED_PIN,false);
            vTaskDelay(500);
        }
        //printf("Button States (SW1 | SW2): %d | %d\n",SW1_state,SW2_state);
    }
}

void producer(void *pvParameters)
{
    //int average_delay = 0; //Used for optional average delay calculation
    //int sum = 0;
    //int count = 0;
    struct producer_data *P = (struct producer_data *)pvParameters; //Get the producer struct
    QueueHandle_t queue = P->queue;                                 //Get the target queue
    int mean_delay_time = P->mean_delay;                            //Get the desired mean delay
    uint32_t data = P->data;                                        //Get the data to be posted
    
    BaseType_t send_successful;
    int delay;
    //Send data:
    while(true){
        delay = (int)(rand() % (2*mean_delay_time)); //averages mean_delay_time
        vTaskDelay(delay);
        send_successful = xQueueSend(queue, (void *) &data, 0); //Returns pdTRUE on successful send to queue
        myAssert(send_successful == pdTRUE);
        
        //Optional for average delay calculation:
        /*
        ++count;
        sum+=delay;
        average_delay = sum/count;
        printf("Average delay: %d",average_delay);*/
    }
}

void consumer(void *pvParameters)
{
    QueueHandle_t queue = (QueueHandle_t)pvParameters; //Get target queue
    uint32_t data_buffer;
    while(true){
        xQueueReceive(queue, &data_buffer, portMAX_DELAY); //Wait for item to be posted to queue
        //printf("Data recieved: %d\n", data_buffer);
        if(!gpio_get(SW1_PIN) && data_buffer == 1){ //if SW1 is pressed and data came from producer1 light LED briefly
            gpio_put(LED_PIN,1);
            vTaskDelay(5);
            gpio_put(LED_PIN,0);
            vTaskDelay(1);
        }
        if(!gpio_get(SW2_PIN) && data_buffer == 2){ //if SW2 is pressed and data came from producer2 light LED briefly
            gpio_put(LED_PIN,1);
            vTaskDelay(20);
            gpio_put(LED_PIN,0);
            vTaskDelay(50);
        }
    }
}



int main()
{
    stdio_init_all();
    hardware_init();
    //Create Heartbeat task:
    xTaskCreate(heartbeat, "LED_Task", 256, NULL, 1, NULL);
    //Create queue:
    QueueHandle_t queue = xQueueCreate(20, sizeof(uint32_t));
    //Create producer structs:
    struct producer_data P1 = {queue, "P1", 100, 2, 1};
    struct producer_data P2 = {queue, "P2", 100, 2, 2};
    //Create tasks for 2x producers and 1x consumer:
    xTaskCreate(producer, P1.thread_name, 256, (void *) &P1, P1.priority, NULL);
    xTaskCreate(producer, P2.thread_name, 256, (void *) &P2, P2.priority, NULL);
    xTaskCreate(consumer, "Consumer_Task", 256, (void *) queue, 1, NULL);

    vTaskStartScheduler();
    myAssert(0); //In case of RTOS failure
}
