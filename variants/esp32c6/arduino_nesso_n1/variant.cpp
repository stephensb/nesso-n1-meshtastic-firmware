#include "variant.h"
#include "driver/gpio.h"
#include <Arduino.h>
#include <Wire.h>
#include <inttypes.h>

// I2C device addresses
#define PI4IO_M_ADDR  0x43
#define PI4IO_M_ADDR2 0x44

// PI4IO registers
#define PI4IO_REG_CHIP_RESET 0x01
#define PI4IO_REG_IO_DIR     0x03
#define PI4IO_REG_OUT_SET    0x05
#define PI4IO_REG_OUT_H_IM   0x07
#define PI4IO_REG_IN_DEF_STA 0x09
#define PI4IO_REG_PULL_EN    0x0B
#define PI4IO_REG_PULL_SEL   0x0D
#define PI4IO_REG_IN_STA     0x0F
#define PI4IO_REG_INT_MASK   0x11
#define PI4IO_REG_IRQ_STA    0x13

#define setbit(x, y)     x |= (0x01 << y)
#define clrbit(x, y)     x &= ~(0x01 << y)
#define reversebit(x, y) x ^= (0x01 << y)
#define getbit(x, y)     ((x) >> (y) & 0x01)

// ---------------------------------------------------------------------------
// I2C bus recovery
//
// Called automatically after NESSO_I2C_FAIL_THRESHOLD consecutive read
// failures.  Tries progressively more aggressive recovery steps:
//   1. Wire.end() + Wire.begin() at each of several candidate frequencies
//   2. Manual 9-pulse SCL clocking to free a slave holding SDA low, then
//      reinit at each frequency
//
// Every step is logged to Serial so you can paste the output and report back
// which config (if any) recovered the bus.
//
// Rate-limited: won't attempt recovery more than once per
// NESSO_I2C_RECOVERY_COOLDOWN_MS to avoid flooding the log in a persistent
// failure scenario.
// ---------------------------------------------------------------------------

#define NESSO_I2C_FAIL_THRESHOLD       3       // consecutive failures before recovery
#define NESSO_I2C_PROBE_ADDR           PI4IO_M_ADDR  // known always-present device
#define NESSO_I2C_RECOVERY_COOLDOWN_MS 10000   // 10 s between recovery attempts

static uint32_t s_i2c_fail_count = 0;          // consecutive failure counter
static uint32_t s_i2c_recovery_count = 0;      // total recovery attempts
static uint32_t s_i2c_last_recovery_ms = 0;    // millis() of last attempt

// Probe a single address; return true if device ACKs.
static bool i2c_probe(uint8_t addr)
{
    Wire.beginTransmission(addr);
    return (Wire.endTransmission() == 0);
}

// Attempt reinit at one frequency; return true if probe succeeds.
static bool i2c_try_freq(uint32_t freq, const char *label)
{
    Wire.end();
    delay(5);
    Wire.begin(I2C_SDA, I2C_SCL, freq);
    delay(2);
    bool ok = i2c_probe(NESSO_I2C_PROBE_ADDR);
    printf("[I2C-RECOVERY] freq=%s (%" PRIu32 " Hz) -> %s\n",
           label, freq, ok ? "OK" : "FAIL");
    return ok;
}

// Manual 9-SCL-pulse recovery: frees a slave that is holding SDA low mid-
// transaction.  Returns true if device ACKs after clocking.
static bool i2c_manual_clock_recovery(uint32_t recover_freq)
{
    printf("[I2C-RECOVERY] Manual 9-pulse SCL clock on SDA=%d SCL=%d...\n",
           I2C_SDA, I2C_SCL);
    Wire.end();
    delay(5);

    // Briefly take GPIO control of both pins
    pinMode(I2C_SCL, OUTPUT);
    pinMode(I2C_SDA, INPUT_PULLUP);
    digitalWrite(I2C_SCL, HIGH);
    delayMicroseconds(10);

    for (int pulse = 0; pulse < 9; pulse++) {
        bool sda_high = digitalRead(I2C_SDA);
        printf("[I2C-RECOVERY]   pulse %d  SDA=%d\n", pulse + 1, sda_high ? 1 : 0);
        if (sda_high && pulse > 0) {
            printf("[I2C-RECOVERY]   SDA released after %d pulses\n", pulse + 1);
            break;
        }
        digitalWrite(I2C_SCL, LOW);
        delayMicroseconds(5);
        digitalWrite(I2C_SCL, HIGH);
        delayMicroseconds(5);
    }

    // Generate a STOP condition: SDA low -> SCL high -> SDA high
    pinMode(I2C_SDA, OUTPUT);
    digitalWrite(I2C_SDA, LOW);
    delayMicroseconds(5);
    digitalWrite(I2C_SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(I2C_SDA, HIGH);
    delayMicroseconds(5);

    // Hand back to Wire
    Wire.begin(I2C_SDA, I2C_SCL, recover_freq);
    delay(5);
    bool ok = i2c_probe(NESSO_I2C_PROBE_ADDR);
    printf("[I2C-RECOVERY] After manual clock at %" PRIu32 " Hz -> %s\n",
           recover_freq, ok ? "OK" : "FAIL");
    return ok;
}

// Full recovery sequence.  Returns true if bus is usable again.
static bool i2c_recover_bus()
{
    // Rate-limit: don't hammer recovery on every failed read
    uint32_t now = millis();
    if (s_i2c_recovery_count > 0 &&
        (now - s_i2c_last_recovery_ms) < NESSO_I2C_RECOVERY_COOLDOWN_MS) {
        return false;
    }
    s_i2c_last_recovery_ms = now;
    s_i2c_recovery_count++;

    printf("[I2C-RECOVERY] === Attempt #%" PRIu32 " after %" PRIu32
           " consecutive failures ===\n",
           s_i2c_recovery_count, s_i2c_fail_count);

    // Step 1: reinit at candidate frequencies (slowest first)
    struct { uint32_t hz; const char *name; } freqs[] = {
        { 10000,  "10kHz"  },
        { 50000,  "50kHz"  },
        { 100000, "100kHz" },
        { 400000, "400kHz" },
    };
    for (auto &f : freqs) {
        if (i2c_try_freq(f.hz, f.name)) {
            printf("[I2C-RECOVERY] SUCCESS with simple reinit at %s\n", f.name);
            printf("[I2C-RECOVERY] >>> Report this frequency: %s <<<\n", f.name);
            return true;
        }
    }

    // Step 2: manual SCL clocking, then retry each frequency
    printf("[I2C-RECOVERY] Simple reinit failed — trying manual SCL clock\n");
    for (auto &f : freqs) {
        if (i2c_manual_clock_recovery(f.hz)) {
            printf("[I2C-RECOVERY] SUCCESS after manual clock at %s\n", f.name);
            printf("[I2C-RECOVERY] >>> Report this frequency: %s <<<\n", f.name);
            return true;
        }
    }

    printf("[I2C-RECOVERY] !!! ALL RECOVERY ATTEMPTS FAILED !!!\n");
    printf("[I2C-RECOVERY] SDA=%d SCL=%d — check wiring/pullups\n",
           I2C_SDA, I2C_SCL);
    return false;
}

// ---------------------------------------------------------------------------
// Public I2C helpers
// ---------------------------------------------------------------------------

// Returns true on success; *value is written only on success.
// Tracks consecutive failures and triggers automatic bus recovery.
bool i2c_read_byte(uint8_t addr, uint8_t reg, uint8_t *value)
{
    Wire.beginTransmission(addr);
    Wire.write(reg);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        s_i2c_fail_count++;
        if (s_i2c_fail_count >= NESSO_I2C_FAIL_THRESHOLD) {
            printf("[I2C] %" PRIu32 " consecutive failures (last: addr=0x%02X reg=0x%02X err=%d)\n",
                   s_i2c_fail_count, addr, reg, err);
            if (i2c_recover_bus()) {
                s_i2c_fail_count = 0;
            }
        }
        return false;
    }
    uint8_t count = Wire.requestFrom(addr, (uint8_t)1);
    if (count == 0 || !Wire.available()) {
        s_i2c_fail_count++;
        if (s_i2c_fail_count >= NESSO_I2C_FAIL_THRESHOLD) {
            printf("[I2C] %" PRIu32 " consecutive failures (last: addr=0x%02X reg=0x%02X no data)\n",
                   s_i2c_fail_count, addr, reg);
            if (i2c_recover_bus()) {
                s_i2c_fail_count = 0;
            }
        }
        return false;
    }
    *value = Wire.read();
    s_i2c_fail_count = 0; // success: reset counter
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
    // PI4IO_M_ADDR (0x43) pin map:
    //   P7 = LoRa Reset (output)
    //   P6 = RF Switch  (output)
    //   P5 = LNA Enable (output)
    //   P1 = KEY2       (input)
    //   P0 = KEY1       (input)
    //
    // PI4IO_M_ADDR2 (0x44) pin map:
    //   P107 = LED_BUILTIN    (output)
    //   P106 = LCD_BACKLIGHT  (output)
    //   P105 = VIN_DETECT     (input, pull-down: HIGH=USB present)
    //   P102 = GROVE_POWER_EN (output)
    //   P101 = LCD_RESET      (output)
    //   P100 = POWEROFF       (input)

    printf("pi4io_init\n");
    uint8_t in_data = 0;

    // --- PI4IO 0x43 ---
    i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_CHIP_RESET, 0xFF);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_read_byte(PI4IO_M_ADDR, PI4IO_REG_CHIP_RESET, &in_data);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_IO_DIR,     0b11100000); // P7,P6,P5=output; rest=input
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_OUT_H_IM,   0b00011100); // P4,P3,P2=high-Z outputs
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_PULL_SEL,   0b11100011); // P7,P6,P5,P1,P0=pull-up
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_PULL_EN,    0b11100011); // enable those pulls
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_IN_DEF_STA, 0b00000011); // P0,P1 default HIGH -> IRQ on press
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_INT_MASK,   0b11111100); // P0,P1 interrupt enabled
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_OUT_SET,    0b11100000); // P7,P6,P5=HIGH (LoRa RST, RF SW, LNA)
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_read_byte(PI4IO_M_ADDR, PI4IO_REG_IRQ_STA, &in_data); // clear any pending IRQ

    // OUT_SET already set above; re-read + ensure RF Switch (P6) is on
    in_data = 0;
    if (i2c_read_byte(PI4IO_M_ADDR, PI4IO_REG_OUT_SET, &in_data)) {
        setbit(in_data, 6); // P6 = RF Switch enable
        i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_OUT_SET, in_data);
    }

    // --- PI4IO 0x44 ---
    i2c_write_byte(PI4IO_M_ADDR2, PI4IO_REG_CHIP_RESET, 0xFF);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_read_byte(PI4IO_M_ADDR2, PI4IO_REG_CHIP_RESET, &in_data);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR2, PI4IO_REG_IO_DIR,     0b11000110); // P107,P106,P102,P101=output; rest=input
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR2, PI4IO_REG_OUT_H_IM,   0b00111000); // P105,P104,P103=high-Z (inputs)
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR2, PI4IO_REG_PULL_SEL,   0b11000111); // P105=pull-down; rest pull-up
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR2, PI4IO_REG_PULL_EN,    0b11000111); // enable those pulls
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR2, PI4IO_REG_IN_DEF_STA, 0b00000000); // no default-state interrupts
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR2, PI4IO_REG_INT_MASK,   0b11111111); // all interrupts masked
    vTaskDelay(10 / portTICK_PERIOD_MS);
    i2c_write_byte(PI4IO_M_ADDR2, PI4IO_REG_OUT_SET,    0b11000110); // P107,P106=HIGH; P102,P101=HIGH

    // --- AW32001E charger (0x49) ---
    i2c_write_byte(0x49, 0x2, 0x1f); // charge current 256 mA
    i2c_write_byte(0x49, 0x5, 0x1a); // charge voltage 4.200 V, disable watchdog
    i2c_write_byte(0x49, 0x1, 0xa2); // UVLO 2.580 V, charge enable, disable HIZ
    i2c_write_byte(0x49, 0x0, 0x8f); // DPM 4.520 V
}

// Set output pin on PI4IO expander. Reads current output register, modifies
// the target bit, and writes back. Call only for output-configured pins.
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
