
#include "DFRobot_LCD.h"

#include "esp_check.h"
#include "esp_rom_sys.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static constexpr int I2C_TIMEOUT_MS = 1000;

DFRobot_RGBLCD1602::DFRobot_RGBLCD1602(
    uint8_t rgbAddr,
    uint8_t cols,
    uint8_t rows,
    uint8_t lcdAddr
)
    : rgbAddr_(rgbAddr),
      lcdAddr_(lcdAddr),
      cols_(cols),
      rows_(rows) {
}

// Send data to LCD.
void DFRobot_RGBLCD1602::send(
    const uint8_t *data,
    size_t len
) {
    ESP_ERROR_CHECK(
        i2c_master_transmit(
            lcd_,
            data,
            len,
            I2C_TIMEOUT_MS
        )
    );
}

// Write RGB controller register.
void DFRobot_RGBLCD1602::setReg(
    uint8_t reg,
    uint8_t value
) {
    uint8_t data[2] = {reg, value};

    ESP_ERROR_CHECK(
        i2c_master_transmit(
            rgb_,
            data,
            sizeof(data),
            I2C_TIMEOUT_MS
        )
    );
}

// Send LCD command.
void DFRobot_RGBLCD1602::command(uint8_t value) {
    uint8_t data[2] = {0x80, value};

    send(data, sizeof(data));
    esp_rom_delay_us(100);
}

// Send one character.
size_t DFRobot_RGBLCD1602::write(uint8_t value) {
    uint8_t data[2] = {0x40, value};

    send(data, sizeof(data));
    esp_rom_delay_us(100);

    return 1;
}

// Replacement for Arduino Print.
void DFRobot_RGBLCD1602::printstr(
    const char *str
) {
    if (str == nullptr) {
        return;
    }

    while (*str != '\0') {
        write((uint8_t)*str);
        str++;
    }
}

// Initialize LCD using shared I2C bus.
void DFRobot_RGBLCD1602::init(
    i2c_master_bus_handle_t bus
) {
    if (lcd_ == nullptr) {

        i2c_device_config_t config = {};

        config.dev_addr_length =
            I2C_ADDR_BIT_LEN_7;

        config.scl_speed_hz = 100000;

        // LCD controller.
        config.device_address = lcdAddr_;

        ESP_ERROR_CHECK(
            i2c_master_bus_add_device(
                bus, &config, &lcd_
            )
        );

        // RGB controller.
        config.device_address = rgbAddr_;

        ESP_ERROR_CHECK(
            i2c_master_bus_add_device(
                bus, &config, &rgb_
            )
        );
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    // Function set: 2-line display.
    uint8_t function = LCD_FUNCTIONSET;

    if (rows_ > 1) {
        function |= LCD_2LINE;
    }

    command(function);
    vTaskDelay(pdMS_TO_TICKS(5));

    command(function);
    vTaskDelay(pdMS_TO_TICKS(5));

    command(function);

    // Display on, cursor and blink off.
    displayControl_ = LCD_DISPLAYON;
    command(LCD_DISPLAYCONTROL | displayControl_);

    clear();

    // Left-to-right entry mode.
    command(LCD_ENTRYMODESET | LCD_ENTRYLEFT);

    // White backlight.
    setColorWhite();
}

void DFRobot_RGBLCD1602::clear() {
    command(LCD_CLEARDISPLAY);
    esp_rom_delay_us(2000);
}

void DFRobot_RGBLCD1602::home() {
    command(LCD_RETURNHOME);
    esp_rom_delay_us(2000);
}

void DFRobot_RGBLCD1602::setCursor(
    uint8_t col,
    uint8_t row
) {
    if (col >= cols_ || row >= rows_) {
        return;
    }

    uint8_t address = col;

    if (row == 1) {
        address += 0x40;
    }

    command(LCD_SETDDRAMADDR | address);
}

// RGB controller version 2.0: 0x2D
void DFRobot_RGBLCD1602::setRGB(
    uint8_t r,
    uint8_t g,
    uint8_t b
) {
    setReg(0x01, r);
    setReg(0x02, g);
    setReg(0x03, b);
}

void DFRobot_RGBLCD1602::setColorWhite() {
    setRGB(255, 255, 255);
}

void DFRobot_RGBLCD1602::setBacklight(
    bool enabled
) {
    if (enabled) {
        setColorWhite();
    } else {
        setRGB(0, 0, 0);
    }
}

void DFRobot_RGBLCD1602::display() {
    displayControl_ |= LCD_DISPLAYON;
    command(LCD_DISPLAYCONTROL | displayControl_);
}

void DFRobot_RGBLCD1602::noDisplay() {
    displayControl_ &= ~LCD_DISPLAYON;
    command(LCD_DISPLAYCONTROL | displayControl_);
}

void DFRobot_RGBLCD1602::cursor() {
    displayControl_ |= LCD_CURSORON;
    command(LCD_DISPLAYCONTROL | displayControl_);
}

void DFRobot_RGBLCD1602::noCursor() {
    displayControl_ &= ~LCD_CURSORON;
    command(LCD_DISPLAYCONTROL | displayControl_);
}

void DFRobot_RGBLCD1602::blink() {
    displayControl_ |= LCD_BLINKON;
    command(LCD_DISPLAYCONTROL | displayControl_);
}

void DFRobot_RGBLCD1602::stopBlink() {
    displayControl_ &= ~LCD_BLINKON;
    command(LCD_DISPLAYCONTROL | displayControl_);
}

void DFRobot_RGBLCD1602::close() {
    if (lcd_ != nullptr) {
        ESP_ERROR_CHECK(
            i2c_master_bus_rm_device(lcd_)
        );
        lcd_ = nullptr;
    }

    if (rgb_ != nullptr) {
        ESP_ERROR_CHECK(
            i2c_master_bus_rm_device(rgb_)
        );
        rgb_ = nullptr;
    }
}
