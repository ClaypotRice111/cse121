
#include "DFRobot_LCD.h"

#include "esp_check.h"
#include "esp_rom_sys.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP32-C3-DevKit-RUST-2
static constexpr gpio_num_t SDA_PIN = GPIO_NUM_7;
static constexpr gpio_num_t SCL_PIN = GPIO_NUM_8;

static constexpr uint32_t I2C_SPEED = 100000;
static constexpr int TIMEOUT_MS = 1000;

static void delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms) + 1);
}

DFRobot_RGBLCD1602::DFRobot_RGBLCD1602(
    uint8_t rgbAddr,
    uint8_t lcdCols,
    uint8_t lcdRows,
    uint8_t lcdAddr
)
    : rgbAddr_(rgbAddr),
      lcdAddr_(lcdAddr),
      cols_(lcdCols),
      rows_(lcdRows) {
}

DFRobot_RGBLCD1602::~DFRobot_RGBLCD1602() {
    if (lcd_ != nullptr) {
        i2c_master_bus_rm_device(lcd_);
    }

    if (rgb_ != nullptr) {
        i2c_master_bus_rm_device(rgb_);
    }

    if (bus_ != nullptr) {
        i2c_del_master_bus(bus_);
    }
}

// --------------------------
// I2C communication
// --------------------------

void DFRobot_RGBLCD1602::send(
    const uint8_t *data,
    size_t len
) {
    ESP_ERROR_CHECK(
        i2c_master_transmit(
            lcd_,
            data,
            len,
            TIMEOUT_MS
        )
    );
}

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
            TIMEOUT_MS
        )
    );
}

// --------------------------
// LCD commands
// --------------------------

void DFRobot_RGBLCD1602::command(uint8_t value) {
    uint8_t data[2] = {0x80, value};

    send(data, sizeof(data));
    esp_rom_delay_us(100);
}

size_t DFRobot_RGBLCD1602::write(uint8_t value) {
    uint8_t data[2] = {0x40, value};

    send(data, sizeof(data));
    esp_rom_delay_us(100);

    return 1;
}

void DFRobot_RGBLCD1602::printstr(
    const char *str
) {
    if (str == nullptr) {
        return;
    }

    while (*str != '\0') {
        write(static_cast<uint8_t>(*str));
        str++;
    }
}

// --------------------------
// LCD initialization
// --------------------------

void DFRobot_RGBLCD1602::init() {

    // Create master bus once.
    if (bus_ == nullptr) {

        i2c_master_bus_config_t bus_config = {};

        bus_config.i2c_port = I2C_NUM_0;
        bus_config.sda_io_num = SDA_PIN;
        bus_config.scl_io_num = SCL_PIN;
        bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_config.glitch_ignore_cnt = 7;
        bus_config.flags.enable_internal_pullup = true;

        ESP_ERROR_CHECK(
            i2c_new_master_bus(
                &bus_config,
                &bus_
            )
        );

        i2c_device_config_t dev_config = {};

        dev_config.dev_addr_length =
            I2C_ADDR_BIT_LEN_7;

        dev_config.scl_speed_hz = I2C_SPEED;

        // Add LCD controller.
        dev_config.device_address = lcdAddr_;

        ESP_ERROR_CHECK(
            i2c_master_bus_add_device(
                bus_,
                &dev_config,
                &lcd_
            )
        );

        // Add RGB controller.
        dev_config.device_address = rgbAddr_;

        ESP_ERROR_CHECK(
            i2c_master_bus_add_device(
                bus_,
                &dev_config,
                &rgb_
            )
        );
    }

    // LCD power-on delay.
    delay_ms(50);

    uint8_t function = LCD_FUNCTIONSET;

    if (rows_ > 1) {
        function |= LCD_2LINE;
    }

    // LCD initialization sequence.
    command(function);
    delay_ms(5);

    command(function);
    delay_ms(5);

    command(function);

    // Display on, cursor off.
    displayControl_ = LCD_DISPLAYON;
    command(LCD_DISPLAYCONTROL | displayControl_);

    clear();

    // Left-to-right text.
    displayMode_ = LCD_ENTRYLEFT;
    command(LCD_ENTRYMODESET | displayMode_);

    setColorWhite();
}

// --------------------------
// Display control
// --------------------------

void DFRobot_RGBLCD1602::clear() {
    command(LCD_CLEARDISPLAY);
    esp_rom_delay_us(2000);
}

void DFRobot_RGBLCD1602::home() {
    command(LCD_RETURNHOME);
    esp_rom_delay_us(2000);
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

// --------------------------
// Cursor positioning
// --------------------------

void DFRobot_RGBLCD1602::setCursor(
    uint8_t col,
    uint8_t row
) {
    if (row >= rows_ || col >= cols_) {
        return;
    }

    uint8_t address = col;

    if (row == 1) {
        address += 0x40;
    }

    command(LCD_SETDDRAMADDR | address);
}

// --------------------------
// Text movement
// --------------------------

void DFRobot_RGBLCD1602::scrollDisplayLeft() {
    command(
        LCD_CURSORSHIFT |
        LCD_DISPLAYMOVE |
        LCD_MOVELEFT
    );
}

void DFRobot_RGBLCD1602::scrollDisplayRight() {
    command(
        LCD_CURSORSHIFT |
        LCD_DISPLAYMOVE |
        LCD_MOVERIGHT
    );
}

void DFRobot_RGBLCD1602::leftToRight() {
    displayMode_ |= LCD_ENTRYLEFT;
    command(LCD_ENTRYMODESET | displayMode_);
}

void DFRobot_RGBLCD1602::rightToLeft() {
    displayMode_ &= ~LCD_ENTRYLEFT;
    command(LCD_ENTRYMODESET | displayMode_);
}

void DFRobot_RGBLCD1602::autoscroll() {
    displayMode_ |= LCD_ENTRYSHIFTINCREMENT;
    command(LCD_ENTRYMODESET | displayMode_);
}

void DFRobot_RGBLCD1602::noAutoscroll() {
    displayMode_ &= ~LCD_ENTRYSHIFTINCREMENT;
    command(LCD_ENTRYMODESET | displayMode_);
}

// --------------------------
// Custom symbols
// --------------------------

void DFRobot_RGBLCD1602::customSymbol(
    uint8_t location,
    uint8_t charmap[]
) {
    if (charmap == nullptr) {
        return;
    }

    location &= 0x07;

    command(
        LCD_SETCGRAMADDR |
        (location << 3)
    );

    uint8_t data[9];

    data[0] = 0x40;

    for (int i = 0; i < 8; i++) {
        data[i + 1] = charmap[i];
    }

    send(data, sizeof(data));
}

// --------------------------
// RGB backlight
// --------------------------

void DFRobot_RGBLCD1602::setRGB(
    uint8_t r,
    uint8_t g,
    uint8_t b
) {
    // DFRobot RGB controller at 0x2D
    setReg(0x01, r);
    setReg(0x02, g);
    setReg(0x03, b);
}

void DFRobot_RGBLCD1602::setColor(
    uint8_t color
) {
    switch (color) {
        case WHITE:
            setRGB(255, 255, 255);
            break;

        case RED:
            setRGB(255, 0, 0);
            break;

        case GREEN:
            setRGB(0, 255, 0);
            break;

        case BLUE:
            setRGB(0, 0, 255);
            break;

        default:
            break;
    }
}

void DFRobot_RGBLCD1602::setColorWhite() {
    setRGB(255, 255, 255);
}

void DFRobot_RGBLCD1602::closeBacklight() {
    setRGB(0, 0, 0);
}

void DFRobot_RGBLCD1602::setBacklight(
    bool enabled
) {
    if (enabled) {
        setColorWhite();
    } else {
        closeBacklight();
    }
}

void DFRobot_RGBLCD1602::setPWM(
    uint8_t reg,
    uint8_t pwm
) {
    setReg(reg, pwm);
}
