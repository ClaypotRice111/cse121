
#include "DFRobot_LCD.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void) {

    DFRobot_RGBLCD1602 lcd(0x2D, 16, 2);

    lcd.init();

    lcd.setCursor(0, 0);
    lcd.printstr("Hello CSE121!");

    lcd.setCursor(0, 1);
    lcd.printstr("Weihao Huang");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
