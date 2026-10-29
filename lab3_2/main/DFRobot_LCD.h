
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "driver/i2c_master.h"

#define LCD_ADDRESS 0x3E

#define LCD_CLEARDISPLAY   0x01
#define LCD_RETURNHOME     0x02
#define LCD_ENTRYMODESET   0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT    0x10
#define LCD_FUNCTIONSET    0x20
#define LCD_SETCGRAMADDR   0x40
#define LCD_SETDDRAMADDR   0x80

#define LCD_ENTRYLEFT      0x02
#define LCD_DISPLAYON      0x04
#define LCD_CURSORON       0x02
#define LCD_BLINKON        0x01
#define LCD_2LINE          0x08

class DFRobot_RGBLCD1602 {
public:
    DFRobot_RGBLCD1602(
        uint8_t rgbAddr,
        uint8_t cols = 16,
        uint8_t rows = 2,
        uint8_t lcdAddr = LCD_ADDRESS
    );

    // Use an existing ESP-IDF I2C bus.
    void init(i2c_master_bus_handle_t bus);

    void clear();
    void home();
    void setCursor(uint8_t col, uint8_t row);
    void printstr(const char *str);

    size_t write(uint8_t value);
    void command(uint8_t value);

    void setRGB(uint8_t r, uint8_t g, uint8_t b);
    void setColorWhite();
    void setBacklight(bool enabled);

    void display();
    void noDisplay();
    void cursor();
    void noCursor();
    void blink();
    void stopBlink();

    void close();

private:
    void send(const uint8_t *data, size_t len);
    void setReg(uint8_t reg, uint8_t value);

    uint8_t rgbAddr_;
    uint8_t lcdAddr_;
    uint8_t cols_;
    uint8_t rows_;

    uint8_t displayControl_ = LCD_DISPLAYON;

    i2c_master_dev_handle_t lcd_ = nullptr;
    i2c_master_dev_handle_t rgb_ = nullptr;
};
