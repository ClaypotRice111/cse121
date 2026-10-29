#include <stdio.h>
#include <stdint.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c_master.h"

#define I2C_SDA_GPIO        7
#define I2C_SCL_GPIO        8
#define I2C_FREQ_HZ         100000

#define ICM_ADDR            0x68
#define WHO_AM_I            0x75
#define ACCEL_DATA_X1       0x0B
#define PWR_MGMT0           0x1F
#define ACCEL_CONFIG0       0x21

#define THRESHOLD          0.25f

static const char *TAG = "LAB4_1";
static i2c_master_dev_handle_t imu;

// Read registers
static esp_err_t imu_read(uint8_t reg, uint8_t *data,
                          size_t len)
{
    return i2c_master_transmit_receive(
        imu, &reg, 1, data, len, 1000
    );
}

// Write register
static esp_err_t imu_write(uint8_t reg, uint8_t value)
{
    uint8_t buf[2] = {reg, value};

    return i2c_master_transmit(
        imu, buf, sizeof(buf), 1000
    );
}

// Initialize I2C and ICM-42670-P
static void imu_init(void)
{
    i2c_master_bus_handle_t bus;

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true
    };

    ESP_ERROR_CHECK(
        i2c_new_master_bus(&bus_config, &bus)
    );

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ICM_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ
    };

    ESP_ERROR_CHECK(
        i2c_master_bus_add_device(
            bus, &dev_config, &imu
        )
    );

ESP_LOGI(TAG, "Scanning I2C bus...");
ESP_LOGI(TAG, "Testing I2C bus...");

uint8_t addresses[] = {0x68, 0x69, 0x70};

for (int i = 0; i < 3; i++) {
    uint8_t addr = addresses[i];

    esp_err_t err = i2c_master_probe(bus, addr, 1000);

    ESP_LOGI(TAG,
             "Address 0x%02X: %s (0x%x)",
             addr,
             esp_err_to_name(err),
             (unsigned int)err);
}

for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    esp_err_t err = i2c_master_probe(bus, addr, 100);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Found device: 0x%02X", addr);
    }
}

uint8_t id = 0;
esp_err_t err = imu_read(WHO_AM_I, &id, 1);

if (err != ESP_OK) {
    ESP_LOGE(TAG, "IMU read failed: %s", esp_err_to_name(err));
    return;
}

ESP_LOGI(TAG, "WHO_AM_I: 0x%02X", id);

    ESP_LOGI(TAG, "WHO_AM_I: 0x%02X", id);

    if (id != 0x67) {
        ESP_LOGE(TAG, "Unexpected IMU device ID");
        ESP_ERROR_CHECK(ESP_ERR_INVALID_RESPONSE);
    }

    // Accelerometer: +/-2g, 100Hz
    ESP_ERROR_CHECK(imu_write(ACCEL_CONFIG0, 0x69));

    // Accelerometer Low Noise mode, gyro OFF
    ESP_ERROR_CHECK(imu_write(PWR_MGMT0, 0x03));

    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "IMU initialized");
}

// Read accelerometer in g
static esp_err_t imu_read_accel(float *x, float *y,
                                float *z)
{
    uint8_t data[6];

    esp_err_t err = imu_read(
        ACCEL_DATA_X1, data, sizeof(data)
    );

    if (err != ESP_OK)
        return err;

    int16_t raw_x = (int16_t)(
        ((uint16_t)data[0] << 8) | data[1]
    );

    int16_t raw_y = (int16_t)(
        ((uint16_t)data[2] << 8) | data[3]
    );

    int16_t raw_z = (int16_t)(
        ((uint16_t)data[4] << 8) | data[5]
    );

    *x = raw_x / 16384.0f;
    *y = raw_y / 16384.0f;
    *z = raw_z / 16384.0f;

    return ESP_OK;
}

void app_main(void)
{
    imu_init();

    while (1) {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        esp_err_t err = imu_read_accel(&x, &y, &z);

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "IMU read failed: %s",
                     esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        const char *vertical = "";
        const char *horizontal = "";

        // UP / DOWN
        if (y > THRESHOLD) {
            vertical = "UP";
        }
        else if (y < -THRESHOLD) {
            vertical = "DOWN";
        }

        // LEFT / RIGHT
        if (x > THRESHOLD) {
            horizontal = "RIGHT";
        }
        else if (x < -THRESHOLD) {
            horizontal = "LEFT";
        }

        ESP_LOGI(TAG,
                 "X: %.2f Y: %.2f Z: %.2f | %s %s",
                 x, y, z, vertical, horizontal);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
