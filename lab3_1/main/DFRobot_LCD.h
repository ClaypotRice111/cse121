#ifndef __DFROBOT_LCD_H__
#define __DFROBOT_LCD_H__

#include <cstdint>
#include <cstddef>

#include "driver/i2c_master.h"
#include "driver/gpio.h"

/* ---------------- 常量 & 宏（与官方头文件一致） ---------------- */
#define LCD_ADDRESS     (0x7c >> 1)

#define WHITE 0
#define RED   1
#define GREEN 2
#define BLUE  3

#define REG_MODE1   0x00
#define REG_MODE2   0x01
#define REG_OUTPUT  0x08

#define LCD_CLEARDISPLAY     0x01
#define LCD_RETURNHOME       0x02
#define LCD_ENTRYMODESET     0x04
#define LCD_DISPLAYCONTROL   0x08
#define LCD_CURSORSHIFT      0x10
#define LCD_FUNCTIONSET      0x20
#define LCD_SETCGRAMADDR     0x40
#define LCD_SETDDRAMADDR     0x80

#define LCD_ENTRYRIGHT           0x00
#define LCD_ENTRYLEFT            0x02
#define LCD_ENTRYSHIFTINCREMENT  0x01
#define LCD_ENTRYSHIFTDECREMENT  0x00

#define LCD_DISPLAYON   0x04
#define LCD_DISPLAYOFF  0x00
#define LCD_CURSORON    0x02
#define LCD_CURSOROFF   0x00
#define LCD_BLINKON     0x01
#define LCD_BLINKOFF    0x00

#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE  0x00
#define LCD_MOVERIGHT   0x04
#define LCD_MOVELEFT    0x00

#define LCD_8BITMODE  0x10
#define LCD_4BITMODE  0x00
#define LCD_2LINE     0x08
#define LCD_1LINE     0x00
#define LCD_5x10DOTS  0x04
#define LCD_5x8DOTS   0x00

/* ---------------- 类声明 ---------------- */
class DFRobot_LCD {
public:
    /* 与官方签名一致：RGBAddr 在第一位 */
    DFRobot_LCD(uint8_t RGBAddr,
                uint8_t lcdCols = 16,
                uint8_t lcdRows = 2,
                uint8_t lcdAddr = LCD_ADDRESS);

    /* 原版 init() 无参；ESP-IDF 需显式建总线，提供两种重载 */
    void init();                                          // 默认 SDA=GPIO10, SCL=GPIO8
    void init(gpio_num_t sda, gpio_num_t scl,
              uint32_t freq = 100000);

    /* 高层 API（与官方 .h 逐条对齐，返回 void） */
    void clear();
    void home();
    void noDisplay();
    void display();
    void stopBlink();
    void blink();
    void noCursor();
    void cursor();
    void scrollDisplayLeft();
    void scrollDisplayRight();
    void leftToRight();
    void rightToLeft();
    void noAutoscroll();
    void autoscroll();
    void customSymbol(uint8_t location, uint8_t charmap[]);
    void setCursor(uint8_t col, uint8_t row);
    void setRGB(uint8_t r, uint8_t g, uint8_t b);

    /* 与官方一致的 inline 语义，含 0x6B 版本特判 */
    void setPWM(uint8_t color, uint8_t pwm) {
        setReg(color, pwm);
        if (_RGBAddr == 0x6B) setReg(0x07, pwm);
    }

    void setColor(uint8_t color);
    void closeBacklight()  { setRGB(0, 0, 0); }
    void setColorWhite()   { setRGB(255, 255, 255); }

    virtual size_t write(uint8_t data);
    void command(uint8_t data);
    void setBacklight(bool mode);

    /* 替代 Arduino Print 的最小重载集合 */
    size_t print(const char *str);
    size_t print(char c)               { return write((uint8_t)c); }
    size_t print(int n);
    size_t print(float f, uint8_t decimals = 2);
    size_t printstr(const char *str)   { return print(str); }   // 供示例使用

private:
    void begin(uint8_t rows, uint8_t charSize = LCD_5x8DOTS);
    void send(uint8_t *data, uint8_t len);
    void setReg(uint8_t addr, uint8_t data);

    /* ===== 成员声明顺序 =====（构造函数初始化列表必须按此顺序！） */
    uint8_t _showFunction;
    uint8_t _showControl;
    uint8_t _showMode;
    uint8_t _initialized;
    uint8_t _numLines;
    uint8_t _currLine;
    uint8_t _lcdAddr;
    uint8_t _RGBAddr;
    uint8_t _cols;
    uint8_t _rows;

    i2c_master_bus_handle_t _bus    = nullptr;
    i2c_master_dev_handle_t _lcdDev = nullptr;
    i2c_master_dev_handle_t _rgbDev = nullptr;

public:
    /* 原版是成员变量（依硬件版本在构造函数体中赋值），不是宏 */
    uint8_t REG_RED   = 0;
    uint8_t REG_GREEN = 0;
    uint8_t REG_BLUE  = 0;
    uint8_t REG_ONLY  = 0;
};

#endif