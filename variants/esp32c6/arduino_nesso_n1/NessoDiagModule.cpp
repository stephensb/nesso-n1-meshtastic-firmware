#include "NessoDiagModule.h"
#if defined(ARDUINO_NESSO_N1)

#include "configuration.h"
#include "graphics/Screen.h"
#include "main.h" // extern graphics::Screen *screen
#include <Wire.h>

NessoDiagModule *nessoDiag = nullptr;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

NessoDiagModule::NessoDiagModule() : OSThread("NessoDiag")
{
    disable(); // Stay idle until ALT_LONG (KEY2 long press) arrives
    if (inputBroker)
        inputObserver.observe(inputBroker);
    LOG_INFO("[DIAG] NessoDiagModule ready — KEY2 long press to run hardware tests");
}

// ---------------------------------------------------------------------------
// Input handler — KEY2 long press triggers the test
// ---------------------------------------------------------------------------

int NessoDiagModule::handleInputEvent(const InputEvent *event)
{
    if (event->inputEvent == INPUT_BROKER_ALT_LONG) {
        // Re-enable the thread (disable() set enabled=false) and run soon
        enabled = true;
        setIntervalFromNow(10);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// I2C raw helpers (bypass variant.cpp failure counter)
// ---------------------------------------------------------------------------

bool NessoDiagModule::rawProbe(uint8_t addr)
{
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

bool NessoDiagModule::rawReadReg(uint8_t addr, uint8_t reg, uint8_t *out)
{
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) // repeated-start
        return false;
    if (Wire.requestFrom(addr, (uint8_t)1) != 1 || !Wire.available())
        return false;
    *out = Wire.read();
    return true;
}

// ---------------------------------------------------------------------------
// Main test run
// ---------------------------------------------------------------------------

int32_t NessoDiagModule::runOnce()
{
    LOG_INFO("[DIAG] ========================================");
    LOG_INFO("[DIAG]   NESSO N1 HARDWARE DIAGNOSTIC REPORT  ");
    LOG_INFO("[DIAG] ========================================");

    // ---- Buttons ----
    LOG_INFO("[DIAG] [BUTTONS]");
    LOG_INFO("[DIAG]   KEY2 (P1/PI4IO-0x43): PASS  <- triggered this test (ALT_LONG)");
    LOG_INFO("[DIAG]   KEY1 (P0/PI4IO-0x43): check logs above for USER_PRESS / SELECT events");

    // ---- FT5x06 touch (0x38) ----
    LOG_INFO("[DIAG] [TOUCHSCREEN - FT5x06 @ 0x38]");
    bool touch_ack = rawProbe(0x38);
    if (touch_ack) {
        uint8_t chip_id = 0xFF;
        bool got_id = rawReadReg(0x38, 0xA3, &chip_id); // CHIP_VENDOR_ID register
        uint8_t fw_id = 0xFF;
        rawReadReg(0x38, 0xA6, &fw_id); // FIRMWARE_ID register
        uint8_t touch_pts = 0xFF;
        rawReadReg(0x38, 0x02, &touch_pts); // TD_STATUS: number of touch points
        LOG_INFO("[DIAG]   ACK: FOUND");
        if (got_id)
            LOG_INFO("[DIAG]   chip_vendor_id(0xA3)=0x%02X  firmware_id(0xA6)=0x%02X  touch_pts(0x02)=%d",
                     chip_id, fw_id, touch_pts & 0x0F);
        else
            LOG_WARN("[DIAG]   chip_id read failed after ACK (unexpected)");
    } else {
        LOG_INFO("[DIAG]   ACK: NOT FOUND");
        LOG_INFO("[DIAG]   Note: FT5x06 init is disabled in LGFX (prevented I2C bus lockup).");
        LOG_INFO("[DIAG]   Device may be in sleep mode. A hardware reset via LCD_RESET (P101)");
        LOG_INFO("[DIAG]   followed by 300ms wait would be required to wake it.");
    }

    // ---- BMI270 accelerometer (0x68) ----
    LOG_INFO("[DIAG] [ACCELEROMETER - BMI270 @ 0x68]");
    bool accel_ack = rawProbe(0x68);
    if (accel_ack) {
        uint8_t chip_id = 0xFF;
        bool got_id = rawReadReg(0x68, 0x00, &chip_id); // CHIP_ID register, expected 0x24
        LOG_INFO("[DIAG]   ACK: FOUND");
        if (got_id) {
            bool id_ok = (chip_id == 0x24);
            LOG_INFO("[DIAG]   chip_id(0x00)=0x%02X  [%s, expected 0x24]",
                     chip_id, id_ok ? "PASS" : "UNEXPECTED");
            if (!id_ok)
                LOG_WARN("[DIAG]   Unexpected BMI270 chip_id 0x%02X — wrong device at 0x68?", chip_id);

            // Read internal status register (0x21) to check if sensor is initialized
            uint8_t int_status = 0xFF;
            rawReadReg(0x68, 0x21, &int_status);
            LOG_INFO("[DIAG]   internal_status(0x21)=0x%02X", int_status);
        } else {
            LOG_WARN("[DIAG]   chip_id read failed after ACK (I2C partial failure)");
        }
    } else {
        LOG_WARN("[DIAG]   ACK: NOT FOUND — BMI270 not responding at 0x68");
    }

    // ---- BQ27220 fuel gauge (0x55) ----
    LOG_INFO("[DIAG] [FUEL GAUGE - BQ27220 @ 0x55]");
    bool fg_ack = rawProbe(0x55);
    LOG_INFO("[DIAG]   ACK: %s", fg_ack ? "FOUND" : "NOT FOUND");
    if (fg_ack) {
        // Read device type register (0x00 = Temperature, better: use standard cmd 0x01 = Voltage)
        // BQ27220 uses standard Gas Gauge subcommands. Voltage is at std command 0x04 (16-bit)
        uint8_t volt_lo = 0xFF, volt_hi = 0xFF;
        if (rawReadReg(0x55, 0x04, &volt_lo) && rawReadReg(0x55, 0x05, &volt_hi)) {
            uint16_t mv = (uint16_t)volt_lo | ((uint16_t)volt_hi << 8);
            LOG_INFO("[DIAG]   voltage_cmd(0x04-0x05)=%d mV", mv);
        }
    }

    // ---- AW32001E charger (0x49) ----
    LOG_INFO("[DIAG] [CHARGER - AW32001E @ 0x49]");
    bool chrg_ack = rawProbe(0x49);
    LOG_INFO("[DIAG]   ACK: %s", chrg_ack ? "FOUND" : "NOT FOUND");
    if (chrg_ack) {
        uint8_t reg0 = 0xFF;
        rawReadReg(0x49, 0x00, &reg0);
        LOG_INFO("[DIAG]   reg0x00=0x%02X", reg0);
    }

    // ---- PI4IO GPIO expanders ----
    LOG_INFO("[DIAG] [GPIO EXPANDERS]");
    bool pi43_ack = rawProbe(0x43);
    bool pi44_ack = rawProbe(0x44);
    LOG_INFO("[DIAG]   PI4IO #1 (0x43): %s", pi43_ack ? "FOUND" : "NOT FOUND");
    LOG_INFO("[DIAG]   PI4IO #2 (0x44): %s", pi44_ack ? "FOUND" : "NOT FOUND");
    if (pi43_ack) {
        uint8_t in_sta = 0xFF;
        rawReadReg(0x43, 0x0F, &in_sta); // IN_STA register
        LOG_INFO("[DIAG]   PI4IO #1 IN_STA(0x0F)=0x%02X  KEY1=%d KEY2=%d",
                 in_sta, (in_sta >> 0) & 1, (in_sta >> 1) & 1);
    }
    if (pi44_ack) {
        uint8_t in_sta = 0xFF;
        rawReadReg(0x44, 0x0F, &in_sta); // IN_STA register
        // P105=VIN_DETECT (bit5), P100=POWEROFF (bit0)
        LOG_INFO("[DIAG]   PI4IO #2 IN_STA(0x0F)=0x%02X  VIN_DETECT=%d POWEROFF=%d",
                 in_sta, (in_sta >> 5) & 1, (in_sta >> 0) & 1);
    }

    LOG_INFO("[DIAG] ========================================");

    // ---- Summary banner on screen ----
    if (screen) {
        static char banner[128];
        snprintf(banner, sizeof(banner),
                 "HW DIAGNOSTIC\n"
                 "KEY2: PASS\n"
                 "Touch(38h): %s\n"
                 "BMI270(68h): %s\n"
                 "Fuel(55h): %s  Chgr(49h): %s",
                 touch_ack ? "FOUND" : "sleep",
                 accel_ack ? "PASS" : "FAIL",
                 fg_ack ? "OK" : "FAIL",
                 chrg_ack ? "OK" : "FAIL");
        screen->showSimpleBanner(banner, 8000);
    }

    disable(); // One-shot — re-arm on next KEY2 long press
    return INT32_MAX;
}

#endif // ARDUINO_NESSO_N1
