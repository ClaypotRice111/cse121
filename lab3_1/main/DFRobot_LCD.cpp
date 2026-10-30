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
    close();
}

// I2C communication
void DFRobot_RGBLCD1602::send(const uint8_t *data, size_t len)
{
    ESP_ERROR_CHECK(
        i2c_master_transmit(
            lcd_,
            data,
            len,
            TIMEOUT_MS
        )
    );
}

void DFRobot_RGBLCD1602::setReg(uint8_t reg, uint8_t value)
{
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

// LCD commands

void DFRobot_RGBLCD1602::command(uint8_t value)
{
    uint8_t data[2] = {0x80, value};

    send(data, sizeof(data));
    esp_rom_delay_us(100);
}

size_t DFRobot_RGBLCD1602::write(uint8_t value)
{
    uint8_t data[2] = {0x40, value};

    send(data, sizeof(data));
    esp_rom_delay_us(100);

    return 1;
}

void DFRobot_RGBLCD1602::printstr(const char *str)
{
    if (str == nullptr) {
        return;
    }

    while (*str != '\0') {
        write(static_cast<uint8_t>(*str));
        str++;
    }
}

// LCD initialization

void DFRobot_RGBLCD1602::init() {

    if (initialized_) {
        return;
    }

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

        ownsBus_ = true;
    }

    initDevices();
}

// Use existing I2C bus
void DFRobot_RGBLCD1602::init(i2c_master_bus_handle_t bus)
{
    if (bus == nullptr) {
        ESP_ERROR_CHECK(ESP_ERR_INVALID_ARG);
    }

    if (bus_ != nullptr && bus_ != bus) {
        ESP_ERROR_CHECK(ESP_ERR_INVALID_STATE);
    }

    if (initialized_) {
        return;
    }

    bus_ = bus;
    ownsBus_ = false;

    initDevices();
}

void DFRobot_RGBLCD1602::initDevices()
{
    if (initialized_) {
        return;
    }

    i2c_device_config_t dev_config = {};

    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_config.scl_speed_hz = I2C_SPEED;

    if (lcd_ == nullptr) {
        dev_config.device_address = lcdAddr_;               // Add LCD controller.

        ESP_ERROR_CHECK(
            i2c_master_bus_add_device(
                bus_,
                &dev_config,
                &lcd_
            )
        );
    }

    if (rgb_ == nullptr) {
        dev_config.device_address = rgbAddr_;                // Add RGB controller.

        ESP_ERROR_CHECK(
            i2c_master_bus_add_device(
                bus_,
                &dev_config,
                &rgb_
            )
        );
    }

    delay_ms(50);                                            // LCD power-on delay.

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
    command(LCD_DISPLAYCONTROL | LCD_DISPLAYON);

    clear();

    // Left-to-right text.
    command(LCD_ENTRYMODESET | LCD_ENTRYLEFT);

    setColorWhite();

    initialized_ = true;
}

// Display control
void DFRobot_RGBLCD1602::clear() {
    command(LCD_CLEARDISPLAY);
    esp_rom_delay_us(2000);
}

// Cursor positioning
void DFRobot_RGBLCD1602::setCursor(uint8_t col, uint8_t row) {
    if (row >= rows_ || col >= cols_) {
        return;
    }

    uint8_t address = col;

    if (row == 1) {
        address += 0x40;
    }

    command(LCD_SETDDRAMADDR | address);
}

// RGB backlight
void DFRobot_RGBLCD1602::setRGB(uint8_t r, uint8_t g, uint8_t b)
{
    setReg(0x01, r);
    setReg(0x02, g);
    setReg(0x03, b);
}

void DFRobot_RGBLCD1602::setColorWhite()
{
    setRGB(255, 255, 255);
}

// Resource management
void DFRobot_RGBLCD1602::close()
{
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

    if (ownsBus_ && bus_ != nullptr) {
        ESP_ERROR_CHECK(
            i2c_del_master_bus(bus_)
        );
    }

    bus_ = nullptr;
    ownsBus_ = false;
    initialized_ = false;
}