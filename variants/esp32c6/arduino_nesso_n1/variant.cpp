#include "driver/gpio.h"
#include <Arduino.h>
#include <Wire.h>
// I2C device addr
#define PI4IO_M_ADDR 0x43
#define PI4IO_M_ADDR2 0x44

// PI4IO registers
#define PI4IO_REG_CHIP_RESET 0x01
#define PI4IO_REG_IO_DIR 0x03
#define PI4IO_REG_OUT_SET 0x05
#define PI4IO_REG_OUT_H_IM 0x07
#define PI4IO_REG_IN_DEF_STA 0x09
#define PI4IO_REG_PULL_EN 0x0B
#define PI4IO_REG_PULL_SEL 0x0D
#define PI4IO_REG_IN_STA 0x0F
#define PI4IO_REG_INT_MASK 0x11
#define PI4IO_REG_IRQ_STA 0x13
// PI4IO

#define setbit(x, y) x |= (0x01 << y)
#define clrbit(x, y) x &= ~(0x01 << y)
#define reversebit(x, y) x ^= (0x01 << y)
#define getbit(x, y) ((x) >> (y)&0x01)

// Returns true on success. On failure *value is left unchanged.
// Using endTransmission() error code and requestFrom() byte count to detect
// NACK or timeout, preventing callers from acting on stale/garbage data.
bool i2c_read_byte(uint8_t addr, uint8_t reg, uint8_t *value)
{
    Wire.beginTransmission(addr);
    Wire.write(reg);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        return false;
    }
    uint8_t count = Wire.requestFrom(addr, (uint8_t)1);
    if (count == 0 || !Wire.available()) {
        return false;
    }
    *value = Wire.read();
    return true;
}

/*******************************************************************/
void i2c_write_byte(uint8_t addr, uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(value);
    Wire.endTransmission();
}
/*******************************************************************/
void c6l_init()
{
    // P7 LoRa Reset
    // P6 RF Switch
    // P5 LNA Enable
    // P1 KEY2
    // P0 KEY1
    // P107 LED_BUILTIN
    // P106 LCD_BACKLIGHT
    // P105 VIN_DETECT
    // P102 GROVE_POWER_EN
    // P101 LCD_RESET
    // P100 POWEROFF

    printf("pi4io_init\n");
    uint8_t in_data = 0;
    i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_CHIP_RESET, 0xFF);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_read_byte(PI4IO_M_ADDR, PI4IO_REG_CHIP_RESET, &in_data);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_IO_DIR, 0b11100000); // 0: input 1: output
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_OUT_H_IM, 0b00011100); // Output High-Impedance, 1 high-impedance
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_PULL_SEL, 0b11100011); // pull up/down select, 0 down, 1 up
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_PULL_EN, 0b11100011); // pull up/down enable, 0 disable, 1 enable
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_IN_DEF_STA, 0b00000011); // P0 P1 Default state HIGH, interrupt when pressed
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_INT_MASK, 0b11111100); // P0 P1 Interrupt 0 enable, 1 disable
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_OUT_SET, 0b11100000); // default output to 0
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_read_byte(PI4IO_M_ADDR, PI4IO_REG_IRQ_STA, &in_data); // Read IRQ_STA to clear

    in_data = 0;
    if (i2c_read_byte(PI4IO_M_ADDR, PI4IO_REG_OUT_SET, &in_data)) {
        setbit(in_data, 6); // Enable LCD backlight pin HIGH
        i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_OUT_SET, in_data);
    }

    i2c_write_byte(PI4IO_M_ADDR2, PI4IO_REG_CHIP_RESET, 0xFF);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_read_byte(PI4IO_M_ADDR2, PI4IO_REG_CHIP_RESET, &in_data);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR2, PI4IO_REG_IO_DIR, 0b11000110); // 0: input 1: output
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR2, PI4IO_REG_OUT_H_IM, 0b00111000); // Output High-Impedance, 1 high-impedance
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR2, PI4IO_REG_PULL_SEL, 0b11000111); // pull up/down select, 0 down, 1 up
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR2, PI4IO_REG_PULL_EN, 0b11000111); // pull up/down enable, 0 disable, 1 enable
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR2, PI4IO_REG_IN_DEF_STA, 0b00000000); // no default-state interrupts
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR2, PI4IO_REG_INT_MASK, 0b11111111); // all interrupts masked
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR2, PI4IO_REG_OUT_SET, 0b11000110); // default output state

    // AW32001E charger - address 0x49
    // charge current 256mA (default 128mA)
    i2c_write_byte(0x49, 0x2, 0x1f);
    // charge voltage 4.200 V (default), disable watchdog timer
    i2c_write_byte(0x49, 0x5, 0x1a);
    // UVLO: 2.580 V, charge enable, disable HIZ (default 0xac)
    i2c_write_byte(0x49, 0x1, 0xa2);
    // DPM 4.520 V (default)
    i2c_write_byte(0x49, 0x0, 0x8f);
}

// Set output pin on PI4IO expander. Reads current output register, modifies
// the target bit, and writes back. Safe to call only for output-configured pins.
void gpio_ext_set(uint8_t address, uint8_t pin, bool value)
{
    uint8_t in_data = 0;
    if (!i2c_read_byte(address, PI4IO_REG_OUT_SET, &in_data)) {
        return; // I2C failure; don't write garbage back
    }
    value ? setbit(in_data, pin) : clrbit(in_data, pin);
    i2c_write_byte(address, PI4IO_REG_OUT_SET, in_data);
}

// Read back what was last written to an output pin (output register, NOT input status).
// Use gpio_ext_read_input() for input pins.
uint8_t gpio_ext_get(uint8_t address, uint8_t pin)
{
    uint8_t in_data = 0;
    i2c_read_byte(address, PI4IO_REG_OUT_SET, &in_data);
    return getbit(in_data, pin);
}

// Read the actual logic level of a pin via the input status register.
// Safe for input pins. Returns 0 on I2C failure (safe default = not asserted).
uint8_t gpio_ext_read_input(uint8_t address, uint8_t pin)
{
    uint8_t in_data = 0;
    i2c_read_byte(address, PI4IO_REG_IN_STA, &in_data);
    return getbit(in_data, pin);
}
