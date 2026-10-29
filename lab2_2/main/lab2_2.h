
#ifndef LAB2_2_H
#define LAB2_2_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"


// I2C configuration
#define I2C_MASTER_SCL_IO       8
#define I2C_MASTER_SDA_IO       7
#define I2C_MASTER_NUM          I2C_NUM_0
#define I2C_MASTER_FREQ_HZ      100000
#define I2C_MASTER_TIMEOUT_MS   1000


// SHTC3 sensor address
#define SHTC3_ADDR              0x70


// SHTC3 commands
#define SHTC3_WAKEUP            0x3517
#define SHTC3_SLEEP             0xB098
#define SHTC3_TEMP              0x7866
#define SHTC3_HUM               0x58E0


// Function declarations
static esp_err_t shtc3_write(i2c_master_dev_handle_t dev_handle, uint16_t command);

static esp_err_t shtc3_read(i2c_master_dev_handle_t dev_handle, uint8_t *data, size_t len);

static uint8_t shtc3_crc(uint8_t *data);

static esp_err_t read_temperature(i2c_master_dev_handle_t dev_handle,float *temperature, float *fahrenheit);

static esp_err_t read_humidity(i2c_master_dev_handle_t dev_handle, float *humidity);

static void i2c_master_init(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle);

#endif
