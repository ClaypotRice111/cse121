#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define LED_GPIO        10   
#define LED_ON          1
#define LED_OFF         0
#define LED_STRIP       2


void blink_task(void *pvParameter)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    while(1) {
        // LED on
        gpio_set_level(LED_GPIO, LED_ON);
        printf ("Blinking LED on GPIO %d\n", LED_GPIO);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second


        // LED off
        gpio_set_level(LED_GPIO, LED_OFF);
        printf ("Turning LED off GPIO %d\n", LED_GPIO);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    }
}

void app_main(void)
{
    xTaskCreate(blink_task, "blink_task", 2048, NULL, 5, NULL);
}