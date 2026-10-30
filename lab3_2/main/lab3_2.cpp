
#include <stdio.h>
#include <stdint.h>

#include "DFRobot_LCD.h"

#include "esp_err.h"
#include "esp_check.h"

#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


// I2C configuration
#define I2C_MASTER_NUM        I2C_NUM_0
#define I2C_MASTER_SDA_IO     GPIO_NUM_7
#define I2C_MASTER_SCL_IO     GPIO_NUM_8
#define I2C_MASTER_FREQ_HZ    100000
#define I2C_MASTER_TIMEOUT_MS 1000

// SHTC3 commands
#define SHTC3_ADDR   0x70
#define SHTC3_WAKEUP 0x3517
#define SHTC3_TEMP   0x7866
#define SHTC3_SLEEP  0xB098

// LCD addresses
#define LCD_ADDR     0x3E
#define LCD_RGB_ADDR 0x2D


// I2C Initialization
static void i2c_master_init(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle) {
   
    i2c_master_bus_config_t bus_config = {};

    bus_config.i2c_port = I2C_MASTER_NUM;
    bus_config.sda_io_num = I2C_MASTER_SDA_IO;
    bus_config.scl_io_num = I2C_MASTER_SCL_IO;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));

    i2c_device_config_t dev_config = {};

    dev_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;

    dev_config.device_address = SHTC3_ADDR;
    dev_config.scl_speed_hz = I2C_MASTER_FREQ_HZ;

    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle,&dev_config,dev_handle));
}


// SHTC3 Write
static esp_err_t shtc3_write(i2c_master_dev_handle_t dev_handle, uint16_t command) {
    uint8_t write_buf[2] = {(uint8_t)(command >> 8), (uint8_t)(command & 0xFF)};

    return i2c_master_transmit( dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS);
}


// SHTC3 Read
static esp_err_t shtc3_read(i2c_master_dev_handle_t dev_handle, uint8_t *data, size_t len) {
    return i2c_master_receive(dev_handle, data, len, I2C_MASTER_TIMEOUT_MS);
}


// CRC-8
// Polynomial: 0x31
// Initial value: 0xFF
static uint8_t shtc3_crc(const uint8_t *data) {
    uint8_t crc = 0xFF;

    for (int i = 0; i < 2; i++) {
        crc ^= data[i];

        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (uint8_t)(
                    (crc << 1) ^ 0x31
                );
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}


// Read temperature and humidity
static esp_err_t read_sensor(i2c_master_dev_handle_t dev_handle, float *temperature, float *humidity) {
    // Trigger measurement.
    esp_err_t err = shtc3_write(dev_handle,SHTC3_TEMP);

    if (err != ESP_OK) {
        return err;
    }

    // Wait for conversion.
    vTaskDelay(pdMS_TO_TICKS(20));

    // 2 bytes temperature + CRC
    // 2 bytes humidity + CRC
    uint8_t data[6];

    err = shtc3_read(dev_handle, data, 6);

    if (err != ESP_OK) {
        return err;
    }

    //temperature CRC.
    if (shtc3_crc(&data[0]) != data[2]) {
        return ESP_ERR_INVALID_CRC;
    }

    //humidity CRC.
    if (shtc3_crc(&data[3]) != data[5]) {
        return ESP_ERR_INVALID_CRC;
    }

    // Raw temperature.
    uint16_t raw_temp = ((uint16_t)data[0] << 8) | data[1];

    // Raw humidity.
    uint16_t raw_hum = ((uint16_t)data[3] << 8) | data[4];

    // Celsius.
    *temperature =-45.0f + 175.0f * raw_temp / 65536.0f;

    // Relative humidity.
    *humidity = 100.0f * raw_hum / 65536.0f;

    return ESP_OK;
}


// Main

extern "C" void app_main(void) {

    i2c_master_bus_handle_t bus_handle = nullptr;
    i2c_master_dev_handle_t shtc3_handle = nullptr;

    // Initialize I2C once.
    i2c_master_init( &bus_handle,&shtc3_handle);

    // Initialize LCD on the same bus.
    DFRobot_RGBLCD1602 lcd(
        LCD_RGB_ADDR,
        16,
        2,
        LCD_ADDR
    );

    lcd.init(bus_handle);
    lcd.setColorWhite();

    lcd.setCursor(0, 0);
    lcd.printstr("SHTC3 Sensor");

    lcd.setCursor(0, 1);
    lcd.printstr("Starting...");

    // Wake the sensor once.
    esp_err_t err = shtc3_write(
        shtc3_handle,
        SHTC3_WAKEUP
    );

    if (err != ESP_OK) {
        printf(
            "Wakeup error: %s\n",
            esp_err_to_name(err)
        );

        lcd.clear();
        lcd.printstr("Sensor error");
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    // Update once per second.
    TickType_t last_update = xTaskGetTickCount();

    while (true) {

        float temperature = 0.0f;
        float humidity = 0.0f;

        err = read_sensor(
            shtc3_handle,
            &temperature,
            &humidity
        );

        if (err == ESP_OK) {

            printf(
                "Temperature: %.1f C, "
                "Humidity: %.1f%%\n",
                temperature,
                humidity
            );

            char temp_text[17];
            char hum_text[17];

            // Build both display strings.
            snprintf(
                temp_text,
                sizeof(temp_text),
                "Temp: %.1f C",
                temperature
            );

            snprintf(
                hum_text,
                sizeof(hum_text),
                "Humidity: %.1f%%",
                humidity
            );

            // Clear old characters from line 1.
            lcd.setCursor(0, 0);
            lcd.printstr("                ");

            lcd.setCursor(0, 0);
            lcd.printstr(temp_text);

            // Clear old characters from line 2.
            lcd.setCursor(0, 1);
            lcd.printstr("                ");

            lcd.setCursor(0, 1);
            lcd.printstr(hum_text);

        } else {

            printf(
                "SHTC3 read error: %s\n",
                esp_err_to_name(err)
            );

            lcd.setCursor(0, 0);
            lcd.printstr("Sensor error    ");

            lcd.setCursor(0, 1);
            lcd.printstr("Check SHTC3     ");
        }

        // One-second sampling period.
        vTaskDelayUntil(
            &last_update,
            pdMS_TO_TICKS(1000)
        );
    }
}
