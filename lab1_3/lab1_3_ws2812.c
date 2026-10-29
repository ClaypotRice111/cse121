#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
// #include "led_strip.h"


#define LED_GPIO        10   
#define LED_ON          1
#define LED_OFF         0
#define LED_STRIP       2


void blink_task(void *pvParameter)
{
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

    // // Configure WS2812 LED
    // led_strip_config_t strip_config = {.strip_gpio_num = LED_GPIO, .max_leds = 1, .led_pixel_format = LED_PIXEL_FORMAT_GRB, .led_model = LED_MODEL_WS2812, };
    // // Configure RMT
    // led_strip_rmt_config_t rmt_config = { .clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = 10000000, .flags.with_dma = false, };
    // // Initialize LED
    // led_strip_handle_t strip;
    // led_strip_new_rmt_device( &strip_config, &rmt_config, &strip);

    while(1) {
        // LED on
        gpio_set_level(LED_GPIO, LED_ON);
        printf ("Blinking LED on GPIO %d\n", LED_GPIO);

        // // LED_Strip on
        // led_strip_set_pixel(strip, 0, 0, 0, 255);
        // led_strip_refresh(strip);

        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second


        // LED off
        gpio_set_level(LED_GPIO, LED_OFF);
        printf ("Turning LED off GPIO %d\n", LED_GPIO);

        // // LED_Strip off
        // led_strip_clear(strip);

        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay for 1 second
    }
}

void app_main(void)
{
    xTaskCreate(blink_task, "blink_task", 2048, NULL, 5, NULL);
}
