
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"

#include "esp_hidd_prf_api.h"
#include "hid_dev.h"

#include "driver/i2c_master.h"
#include "driver/gpio.h"

// ===========================================
// Configuration
// ===========================================

#define TAG "LAB4_3"

// RUST-2 I2C pins verified in Lab 4.1
#define I2C_SDA_GPIO       7
#define I2C_SCL_GPIO       8
#define I2C_FREQ_HZ        100000
#define I2C_TIMEOUT_MS     1000

// ICM-42670-P
#define ICM_ADDR           0x68
#define ICM_WHO_AM_I       0x75
#define ICM_PWR_MGMT0      0x1F
#define ICM_ACCEL_CONFIG0  0x21
#define ICM_ACCEL_DATA_X1  0x0B

// Mouse settings
#define MOUSE_INTERVAL_MS  20
#define DEAD_ZONE          0.12f
#define BIT_TILT           0.30f
#define LOT_TILT           0.60f

// Adjust these according to board orientation
#define MOUSE_X_SIGN       1
#define MOUSE_Y_SIGN      -1

// GPIO9 is an example button pin.
// Confirm actual board wiring before checkout.
#define CLICK_GPIO         GPIO_NUM_9
#define ENABLE_BUTTON      1

// BLE HID
#define HIDD_DEVICE_NAME   "CSE121 Tilt Mouse"

// ===========================================
// Global variables
// ===========================================

static uint16_t hid_conn_id = 0;
static volatile bool sec_conn = false;

static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t imu = NULL;

static float offset_x = 0.0f;
static float offset_y = 0.0f;

static esp_hidd_cb_event_t unused_event_type
    __attribute__((unused));

static uint8_t hidd_service_uuid128[] = {
    0xfb, 0x34, 0x9b, 0x5f,
    0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00,
    0x12, 0x18, 0x00, 0x00
};

static esp_ble_adv_data_t hidd_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = ESP_BLE_GAP_CONN_ITVL_MS(7.5),
    .max_interval = ESP_BLE_GAP_CONN_ITVL_MS(20),
    .appearance = 0x03c2,
    .manufacturer_len = 0,
    .p_manufacturer_data = NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = sizeof(hidd_service_uuid128),
    .p_service_uuid = hidd_service_uuid128,
    .flag = 0x6,
};

static esp_ble_adv_params_t hidd_adv_params = {
    .adv_int_min = ESP_BLE_GAP_ADV_ITVL_MS(20),
    .adv_int_max = ESP_BLE_GAP_ADV_ITVL_MS(30),
    .adv_type = ADV_TYPE_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy =
        ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

// ===========================================
// I2C driver
// ===========================================

static esp_err_t imu_read(
    uint8_t reg,
    uint8_t *data,
    size_t len)
{
    return i2c_master_transmit_receive(
        imu,
        &reg,
        1,
        data,
        len,
        I2C_TIMEOUT_MS
    );
}

static esp_err_t imu_write(
    uint8_t reg,
    uint8_t value)
{
    uint8_t buf[2] = {reg, value};

    return i2c_master_transmit(
        imu,
        buf,
        sizeof(buf),
        I2C_TIMEOUT_MS
    );
}

static void imu_init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(
        i2c_new_master_bus(
            &bus_config,
            &i2c_bus
        )
    );

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ICM_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };

    ESP_ERROR_CHECK(
        i2c_master_bus_add_device(
            i2c_bus,
            &dev_config,
            &imu
        )
    );

    uint8_t id = 0;

    ESP_ERROR_CHECK(
        imu_read(ICM_WHO_AM_I, &id, 1)
    );

    ESP_LOGI(TAG, "WHO_AM_I: 0x%02X", id);

    if (id != 0x67) {
        ESP_LOGE(TAG, "Invalid IMU ID");
        ESP_ERROR_CHECK(ESP_ERR_INVALID_RESPONSE);
    }

    // +/- 2g, 100 Hz
    ESP_ERROR_CHECK(
        imu_write(ICM_ACCEL_CONFIG0, 0x69)
    );

    // Accelerometer Low Noise, gyroscope OFF
    ESP_ERROR_CHECK(
        imu_write(ICM_PWR_MGMT0, 0x03)
    );

    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "ICM-42670-P initialized");
}

// ===========================================
// Accelerometer
// ===========================================

static esp_err_t imu_read_accel(
    float *x,
    float *y,
    float *z)
{
    uint8_t data[6];

    esp_err_t err = imu_read(
        ICM_ACCEL_DATA_X1,
        data,
        sizeof(data)
    );

    if (err != ESP_OK) {
        return err;
    }

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

// ===========================================
// Calibration
// ===========================================

static void imu_calibrate(void)
{
    float sum_x = 0;
    float sum_y = 0;

    int samples = 0;

    ESP_LOGI(TAG,
             "Keep board LEVEL for calibration");

    vTaskDelay(pdMS_TO_TICKS(1000));

    for (int i = 0; i < 50; i++) {
        float x, y, z;

        if (imu_read_accel(&x, &y, &z) == ESP_OK) {
            sum_x += x;
            sum_y += y;
            samples++;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (samples > 0) {
        offset_x = sum_x / samples;
        offset_y = sum_y / samples;
    }

    ESP_LOGI(TAG,
             "Calibration X=%.3f Y=%.3f",
             offset_x, offset_y);
}

// ===========================================
// Mouse calculation
// ===========================================

static int mouse_direction(float value)
{
    if (fabsf(value) < DEAD_ZONE) {
        return 0;
    }

    return value > 0 ? 1 : -1;
}

static int mouse_delta(
    float acceleration,
    uint32_t held_ms)
{
    float magnitude = fabsf(acceleration);

    if (magnitude < DEAD_ZONE) {
        return 0;
    }

    // Speed levels
    int speed;

    if (magnitude < BIT_TILT) {
        speed = 2;
    }
    else if (magnitude < LOT_TILT) {
        speed = 5;
    }
    else {
        speed = 9;
    }

    // Time-based acceleration
    int multiplier = 1;

    if (held_ms >= 700) {
        multiplier = 4;
    }
    else if (held_ms >= 300) {
        multiplier = 3;
    }
    else if (held_ms >= 100) {
        multiplier = 2;
    }

    int delta = speed * multiplier;

    return acceleration > 0 ? delta : -delta;
}

// ===========================================
// Button
// ===========================================

static void button_init(void)
{
#if ENABLE_BUTTON
    gpio_config_t config = {
        .pin_bit_mask = (1ULL << CLICK_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&config));

    ESP_LOGI(TAG,
             "Mouse button configured on GPIO%d",
             CLICK_GPIO);
#endif
}

// ===========================================
// BLE HID event callback
// ===========================================

static void hidd_event_callback(
    esp_hidd_cb_event_t event,
    esp_hidd_cb_param_t *param)
{
    switch (event) {

    case ESP_HIDD_EVENT_REG_FINISH:
        if (param->init_finish.state ==
            ESP_HIDD_INIT_OK) {

            esp_ble_gap_set_device_name(
                HIDD_DEVICE_NAME
            );

            esp_ble_gap_config_adv_data(
                &hidd_adv_data
            );
        }
        break;

    case ESP_BAT_EVENT_REG:
        break;

    case ESP_HIDD_EVENT_DEINIT_FINISH:
        break;

    case ESP_HIDD_EVENT_BLE_CONNECT:
        hid_conn_id = param->connect.conn_id;

        ESP_LOGI(TAG, "BLE connected");
        break;

    case ESP_HIDD_EVENT_BLE_DISCONNECT:
        sec_conn = false;

        ESP_LOGI(TAG, "BLE disconnected");

        esp_ble_gap_start_advertising(
            &hidd_adv_params
        );
        break;

    case ESP_HIDD_EVENT_BLE_VENDOR_REPORT_WRITE_EVT:
        break;

    case ESP_HIDD_EVENT_BLE_LED_REPORT_WRITE_EVT:
        break;

    default:
        break;
    }
}

// ===========================================
// BLE GAP callback
// ===========================================

static void gap_event_handler(
    esp_gap_ble_cb_event_t event,
    esp_ble_gap_cb_param_t *param)
{
    switch (event) {

    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(
            &hidd_adv_params
        );

        ESP_LOGI(TAG, "BLE advertising");
        break;

    case ESP_GAP_BLE_SEC_REQ_EVT:
        esp_ble_gap_security_rsp(
            param->ble_security.ble_req.bd_addr,
            true
        );
        break;

    case ESP_GAP_BLE_AUTH_CMPL_EVT:
        if (param->ble_security.auth_cmpl.success) {
            sec_conn = true;

            ESP_LOGI(TAG,
                     "BLE pairing successful");
        }
        else {
            sec_conn = false;

            ESP_LOGE(TAG,
                     "Pairing failed: 0x%x",
                     param->ble_security.auth_cmpl.fail_reason);
        }
        break;

    default:
        break;
    }
}

// ===========================================
// HID Mouse task
// ===========================================

void hid_demo_task(void *pvParameters)
{
    int last_dir_x = 0;
    int last_dir_y = 0;

    uint32_t held_x = 0;
    uint32_t held_y = 0;

    float filtered_x = 0;
    float filtered_y = 0;

    bool button_stable = false;
    bool button_last_raw = false;
    TickType_t button_changed = 0;

    const float alpha = 0.25f;

    TickType_t last_wake = xTaskGetTickCount();

    uint32_t log_counter = 0;

    while (1) {

        // ---------------------------
        // Read IMU
        // ---------------------------

        float x, y, z;

        esp_err_t err = imu_read_accel(
            &x, &y, &z
        );

        if (err != ESP_OK) {
            ESP_LOGE(TAG,
                     "IMU read error: %s",
                     esp_err_to_name(err));

            vTaskDelayUntil(
                &last_wake,
                pdMS_TO_TICKS(MOUSE_INTERVAL_MS)
            );
            continue;
        }

        // Remove level-position offsets
        x -= offset_x;
        y -= offset_y;

        // Low pass filter
        filtered_x += alpha * (x - filtered_x);
        filtered_y += alpha * (y - filtered_y);

        // Coordinate mapping
        float mouse_x = filtered_x * MOUSE_X_SIGN;
        float mouse_y = filtered_y * MOUSE_Y_SIGN;

        int dir_x = mouse_direction(mouse_x);
        int dir_y = mouse_direction(mouse_y);

        // ---------------------------
        // Acceleration timing
        // ---------------------------

        if (dir_x == 0 || dir_x != last_dir_x) {
            held_x = 0;
        }
        else {
            held_x += MOUSE_INTERVAL_MS;
        }

        if (dir_y == 0 || dir_y != last_dir_y) {
            held_y = 0;
        }
        else {
            held_y += MOUSE_INTERVAL_MS;
        }

        last_dir_x = dir_x;
        last_dir_y = dir_y;

        int dx = mouse_delta(mouse_x, held_x);
        int dy = mouse_delta(mouse_y, held_y);

        // ---------------------------
        // Button debounce
        // ---------------------------

        bool button_pressed = false;

#if ENABLE_BUTTON
        bool raw_pressed =
            gpio_get_level(CLICK_GPIO) == 0;

        TickType_t now = xTaskGetTickCount();

        if (raw_pressed != button_last_raw) {
            button_last_raw = raw_pressed;
            button_changed = now;
        }

        if ((now - button_changed) >=
            pdMS_TO_TICKS(30)) {
            button_stable = raw_pressed;
        }

        button_pressed = button_stable;
#endif

        // ---------------------------
        // Send HID report
        // ---------------------------

        if (sec_conn) {
            uint8_t buttons =
                button_pressed ? 0x01 : 0x00;

            esp_hidd_send_mouse_value(
                hid_conn_id,
                buttons,
                (int8_t)dx,
                (int8_t)dy
            );
        }
        else {
            // Reset acceleration while disconnected
            held_x = 0;
            held_y = 0;
            last_dir_x = 0;
            last_dir_y = 0;
        }

        // ---------------------------
        // Debug log
        // ---------------------------

        if (++log_counter >= 10) {
            log_counter = 0;

            const char *vertical =
                dir_y < 0 ? "UP" :
                dir_y > 0 ? "DOWN" : "";

            const char *horizontal =
                dir_x < 0 ? "LEFT" :
                dir_x > 0 ? "RIGHT" : "";

            ESP_LOGI(TAG,
                "X:%.2f Y:%.2f Z:%.2f | "
                "%s %s | dx:%d dy:%d "
                "| click:%d | BLE:%d",
                x, y, z,
                vertical, horizontal,
                dx, dy,
                button_pressed,
                sec_conn);
        }

        vTaskDelayUntil(
            &last_wake,
            pdMS_TO_TICKS(MOUSE_INTERVAL_MS)
        );
    }
}

// ===========================================
// Main
// ===========================================

void app_main(void)
{
    esp_err_t ret;

    // Initialize IMU
    imu_init();

    // Calibrate while board is flat
    imu_calibrate();

    // Initialize click button
    button_init();

    // Initialize NVS
    ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    // BLE controller
    ESP_ERROR_CHECK(
        esp_bt_controller_mem_release(
            ESP_BT_MODE_CLASSIC_BT
        )
    );

    esp_bt_controller_config_t bt_cfg =
        BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(
        esp_bt_controller_init(&bt_cfg)
    );

    ESP_ERROR_CHECK(
        esp_bt_controller_enable(
            ESP_BT_MODE_BLE
        )
    );

    // Bluedroid
    esp_bluedroid_config_t cfg =
        BT_BLUEDROID_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(
        esp_bluedroid_init_with_cfg(&cfg)
    );

    ESP_ERROR_CHECK(
        esp_bluedroid_enable()
    );

    // HID profile
    ESP_ERROR_CHECK(
        esp_hidd_profile_init()
    );

    ESP_ERROR_CHECK(
        esp_ble_gap_register_callback(
            gap_event_handler
        )
    );

    ESP_ERROR_CHECK(
        esp_hidd_register_callbacks(
            hidd_event_callback
        )
    );

    // BLE security settings
    esp_ble_auth_req_t auth_req =
        ESP_LE_AUTH_BOND;

    esp_ble_io_cap_t iocap =
        ESP_IO_CAP_NONE;

    uint8_t key_size = 16;

    uint8_t init_key =
        ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

    uint8_t rsp_key =
        ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;

    ESP_ERROR_CHECK(
        esp_ble_gap_set_security_param(
            ESP_BLE_SM_AUTHEN_REQ_MODE,
            &auth_req,
            sizeof(auth_req)
        )
    );

    ESP_ERROR_CHECK(
        esp_ble_gap_set_security_param(
            ESP_BLE_SM_IOCAP_MODE,
            &iocap,
            sizeof(iocap)
        )
    );

    ESP_ERROR_CHECK(
        esp_ble_gap_set_security_param(
            ESP_BLE_SM_MAX_KEY_SIZE,
            &key_size,
            sizeof(key_size)
        )
    );

    ESP_ERROR_CHECK(
        esp_ble_gap_set_security_param(
            ESP_BLE_SM_SET_INIT_KEY,
            &init_key,
            sizeof(init_key)
        )
    );

    ESP_ERROR_CHECK(
        esp_ble_gap_set_security_param(
            ESP_BLE_SM_SET_RSP_KEY,
            &rsp_key,
            sizeof(rsp_key)
        )
    );

    // Mouse task
    BaseType_t task_result = xTaskCreate(
        hid_demo_task,
        "hid_mouse_task",
        4096,
        NULL,
        5,
        NULL
    );

    if (task_result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create mouse task");
    }

    ESP_LOGI(TAG,
             "CSE121 Bluetooth Mouse initialized");
}
