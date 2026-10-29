#include "DFRobot_LCD.h"          // ← 必须放在第一行

#include <cstdio>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ===========================================================
 * 构造函数
 *   初始化列表顺序必须严格匹配 .h 里的成员声明顺序：
 *   _showFunction → _showControl → _showMode → _initialized →
 *   _numLines → _currLine → _lcdAddr → _RGBAddr → _cols → _rows
 * =========================================================== */
DFRobot_LCD::DFRobot_LCD(uint8_t RGBAddr, uint8_t lcdCols,
                         uint8_t lcdRows, uint8_t lcdAddr)
    : _showFunction(LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS),
      _showControl (LCD_DISPLAYON | LCD_CURSOROFF | LCD_BLINKOFF),
      _showMode    (LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT),
      _initialized (0),
      _numLines    (lcdRows),
      _currLine    (0),
      _lcdAddr     (lcdAddr),
      _RGBAddr     (RGBAddr),
      _cols        (lcdCols),
      _rows        (lcdRows)
{
    /* 寄存器地址依硬件版本不同，与官方 .cpp 保持一致 */
    if (_RGBAddr == 0x6B) {           // V1.1
        REG_RED   = 0x04; REG_GREEN = 0x03;
        REG_BLUE  = 0x02; REG_ONLY  = 0x05;
    } else if (_RGBAddr == 0x60) {    // V1.0
        REG_RED   = 0x06; REG_GREEN = 0x07;
        REG_BLUE  = 0x05; REG_ONLY  = 0x08;
    } else if (_RGBAddr == 0x2D) {    // V2.0
        REG_RED   = 0x01; REG_GREEN = 0x02;
        REG_BLUE  = 0x03; REG_ONLY  = 0x04;
    }
}

/* ===========================================================
 * I²C 底层
 * =========================================================== */
void DFRobot_LCD::send(uint8_t *data, uint8_t len) {
    i2c_master_transmit(_lcdDev, data, len, -1);
}

void DFRobot_LCD::setReg(uint8_t addr, uint8_t data) {
    uint8_t buf[2] = { addr, data };
    i2c_master_transmit(_rgbDev, buf, 2, -1);
}

/* ===========================================================
 * 初始化
 * =========================================================== */
void DFRobot_LCD::init() {
    /* ESP Rust Board: SDA = GPIO10, SCL = GPIO8 */
    init(GPIO_NUM_10, GPIO_NUM_8);
}

void DFRobot_LCD::init(gpio_num_t sda, gpio_num_t scl, uint32_t freq) {
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port = I2C_NUM_0;
    bus_cfg.sda_io_num = sda;
    bus_cfg.scl_io_num = scl;
    bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt = 7;
    bus_cfg.flags.enable_internal_pullup = true;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &_bus));

    i2c_device_config_t lcd_cfg = {};
    lcd_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    lcd_cfg.device_address  = _lcdAddr;
    lcd_cfg.scl_speed_hz    = freq;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(_bus, &lcd_cfg, &_lcdDev));

    i2c_device_config_t rgb_cfg = {};
    rgb_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    rgb_cfg.device_address  = _RGBAddr;
    rgb_cfg.scl_speed_hz    = freq;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(_bus, &rgb_cfg, &_rgbDev));

    begin(_rows);

    /* RGB 背光控制器上电配置 */
    setReg(REG_MODE1, 0x00);
    setReg(REG_OUTPUT, 0xAA);       // 三路 PWM 使能
    setReg(REG_MODE2, 0x20);
}

void DFRobot_LCD::begin(uint8_t rows, uint8_t /*charSize*/) {
    _numLines = rows;
    _currLine = 0;

    vTaskDelay(pdMS_TO_TICKS(50));   // 等 LCD 上电稳定

    /* HD44780 4-bit 初始化序列 */
    const uint8_t seq[] = { 0x33, 0x32, 0x28, 0x0C, 0x06, 0x01 };
    for (uint8_t c : seq) {
        command(c);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    vTaskDelay(pdMS_TO_TICKS(5));
}

/* ===========================================================
 * 命令 / 写字符
 * =========================================================== */
void DFRobot_LCD::command(uint8_t data) {
    uint8_t buf[2] = { 0x00, data };   // 0x00 = 命令寄存器
    send(buf, 2);
}

size_t DFRobot_LCD::write(uint8_t data) {
    uint8_t buf[2] = { 0x40, data };   // 0x40 = 数据寄存器
    send(buf, 2);
    return 1;
}

/* ===========================================================
 * Print 重载
 * =========================================================== */
size_t DFRobot_LCD::print(const char *str) {
    size_t n = 0;
    while (*str) n += write((uint8_t)*str++);
    return n;
}

size_t DFRobot_LCD::print(int n) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", n);
    return print(buf);
}

size_t DFRobot_LCD::print(float f, uint8_t decimals) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", decimals, f);
    return print(buf);
}

/* ===========================================================
 * 高层 API
 * =========================================================== */
void DFRobot_LCD::clear() {
    command(LCD_CLEARDISPLAY);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void DFRobot_LCD::home() {
    command(LCD_RETURNHOME);
    vTaskDelay(pdMS_TO_TICKS(2));
}

void DFRobot_LCD::noDisplay() {
    _showControl &= ~LCD_DISPLAYON;
    command(LCD_DISPLAYCONTROL | _showControl);
}

void DFRobot_LCD::display() {
    _showControl |= LCD_DISPLAYON;
    command(LCD_DISPLAYCONTROL | _showControl);
}

void DFRobot_LCD::stopBlink() {
    _showControl &= ~LCD_BLINKON;
    command(LCD_DISPLAYCONTROL | _showControl);
}

void DFRobot_LCD::blink() {
    _showControl |= LCD_BLINKON;
    command(LCD_DISPLAYCONTROL | _showControl);
}

void DFRobot_LCD::noCursor() {
    _showControl &= ~LCD_CURSORON;
    command(LCD_DISPLAYCONTROL | _showControl);
}

void DFRobot_LCD::cursor() {
    _showControl |= LCD_CURSORON;
    command(LCD_DISPLAYCONTROL | _showControl);
}

void DFRobot_LCD::scrollDisplayLeft() {
    command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVELEFT);
}

void DFRobot_LCD::scrollDisplayRight() {
    command(LCD_CURSORSHIFT | LCD_DISPLAYMOVE | LCD_MOVERIGHT);
}

void DFRobot_LCD::leftToRight() {
    _showMode |= LCD_ENTRYLEFT;
    command(LCD_ENTRYMODESET | _showMode);
}

void DFRobot_LCD::rightToLeft() {
    _showMode &= ~LCD_ENTRYLEFT;
    command(LCD_ENTRYMODESET | _showMode);
}

void DFRobot_LCD::noAutoscroll() {
    _showMode &= ~LCD_ENTRYSHIFTINCREMENT;
    command(LCD_ENTRYMODESET | _showMode);
}

void DFRobot_LCD::autoscroll() {
    _showMode |= LCD_ENTRYSHIFTINCREMENT;
    command(LCD_ENTRYMODESET | _showMode);
}

void DFRobot_LCD::customSymbol(uint8_t location, uint8_t charmap[]) {
    location &= 0x7;    // 只有 8 个 CGRAM 位置
    command(LCD_SETCGRAMADDR | (location << 3));
    for (int i = 0; i < 8; i++) write(charmap[i]);
}

void DFRobot_LCD::setCursor(uint8_t col, uint8_t row) {
    if (row >= _numLines) row = _numLines - 1;
    static const uint8_t row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
    command(LCD_SETDDRAMADDR | (col + row_offsets[row]));
}

/* ===========================================================
 * RGB 背光
 * =========================================================== */
void DFRobot_LCD::setRGB(uint8_t r, uint8_t g, uint8_t b) {
    setReg(REG_RED,   r);
    setReg(REG_GREEN, g);
    setReg(REG_BLUE,  b);
}

void DFRobot_LCD::setColor(uint8_t color) {
    switch (color) {
        case WHITE: setRGB(255, 255, 255); break;
        case RED:   setRGB(255,   0,   0); break;
        case GREEN: setRGB(  0, 255,   0); break;
        case BLUE:  setRGB(  0,   0, 255); break;
    }
}

void DFRobot_LCD::setBacklight(bool mode) {
    if (mode) setColorWhite();
    else      closeBacklight();
}