#include "lab2_2.h"

// Read
static esp_err_t shtc3_read(i2c_master_dev_handle_t dev_handle, uint8_t *data, size_t len)
{
    return i2c_master_receive(dev_handle, data, len, I2C_MASTER_TIMEOUT_MS);
}

// Write
static esp_err_t shtc3_write(i2c_master_dev_handle_t dev_handle, uint16_t command)
{
    uint8_t write_buf[2] = {
        (uint8_t)(command >> 8),
        (uint8_t)(command & 0xFF)
    };

    return i2c_master_transmit(dev_handle, write_buf, sizeof(write_buf), I2C_MASTER_TIMEOUT_MS);
}

// Initialize
static void i2c_master_init(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = SHTC3_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
}

// CRC checksum
static uint8_t shtc3_crc(uint8_t *data)
{
    uint8_t crc = 0xFF;

    for (int i = 0; i < 2; i++) {
        crc ^= data[i];

        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

// Read temperature
static esp_err_t read_temperature(i2c_master_dev_handle_t dev_handle, float *temperature, float *fahrenheit)
{
    esp_err_t err = shtc3_write(dev_handle, SHTC3_TEMP);
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t data[3];
    err = shtc3_read(dev_handle, data, 3);
    if (err != ESP_OK) return err;

    if (shtc3_crc(data) != data[2])
        return ESP_ERR_INVALID_CRC;

    uint16_t temp = ((uint16_t)data[0] << 8) | data[1];

    *temperature = -45.0f + 175.0f * temp / 65536.0f;
    *fahrenheit = *temperature * 9.0f / 5.0f + 32.0f;

    return ESP_OK;
}

// Read humidity
static esp_err_t read_humidity(i2c_master_dev_handle_t dev_handle, float *humidity)
{
    esp_err_t err = shtc3_write(dev_handle, SHTC3_HUM);
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t data[3];
    err = shtc3_read(dev_handle, data, 3);
    if (err != ESP_OK) return err;

    if (shtc3_crc(data) != data[2])
        return ESP_ERR_INVALID_CRC;

    uint16_t hum = ((uint16_t)data[0] << 8) | data[1];
    *humidity = 100.0f * hum / 65536.0f;

    return ESP_OK;
}

// Main
void app_main(void)
{
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;

    i2c_master_init(&bus_handle, &dev_handle);

    while (1) {
        float temperature = 0.0f;
        float fahrenheit = 0.0f;
        float humidity = 0.0f;

        // Wake up
        esp_err_t err = shtc3_write(dev_handle, SHTC3_WAKEUP);

        if (err == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(20));

            // Read temperature
            err = read_temperature(dev_handle, &temperature, &fahrenheit);

            // Read humidity
            if (err == ESP_OK)
                err = read_humidity(dev_handle, &humidity);
        }

        // Sleep
        esp_err_t sleep_err = shtc3_write(dev_handle, SHTC3_SLEEP);
        if (err == ESP_OK)
            err = sleep_err;

        if (err == ESP_OK) {
            printf("Temperature is %.0fC (or %.0fF) with a %.0f%% humidity\n",
                   temperature, fahrenheit, humidity);
        } else {
            printf("Read error: %s\n", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
