#include "DFRobot_LCD.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void app_main(void)
{
    /* 第 1 参是 RGB 背光 I²C 地址：
     *   0x60 = V1.0 / 0x6B = V1.1 / 0x2D = V2.0
     * 不确定就先试 0x6B。 */
    DFRobot_LCD lcd(0x6B, 16, 2);

    /* ESP Rust Board：SDA=GPIO10, SCL=GPIO8
     * 也可直接 lcd.init(); 走默认引脚 */
    lcd.init(GPIO_NUM_10, GPIO_NUM_8);

    while (true) {
        lcd.clear();
        lcd.setRGB(255, 255, 255);        // 白背光

        lcd.setCursor(0, 0);
        lcd.print("Hello CSE121!");

        lcd.setCursor(0, 1);
        lcd.printstr("Renau");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}