#pragma once
#include "configuration.h"
#if defined(ARDUINO_NESSO_N1)

#include "Observer.h"
#include "concurrency/OSThread.h"
#include "input/InputBroker.h"

/**
 * Nesso N1 Hardware Diagnostic Module
 *
 * Triggered by KEY2 long press (INPUT_BROKER_ALT_LONG).
 * Probes all expected I2C devices on the bus, reads chip IDs,
 * and prints a full diagnostic report to the serial log.
 *
 * Results are also displayed as a 5-second overlay banner on screen.
 *
 * Tests covered:
 *   - KEY1 (via i2cButton USER_PRESS/SELECT events already in log)
 *   - KEY2 (the long press that triggers this test = KEY2 confirmed working)
 *   - FT5x06 touch controller (I2C 0x38) — probe + chip ID
 *   - BMI270 accelerometer (I2C 0x68) — chip ID, expect 0x24
 *   - BQ27220 fuel gauge (I2C 0x55) — probe
 *   - AW32001E charger (I2C 0x49) — probe
 *   - PI4IO #1 (I2C 0x43) — probe
 *   - PI4IO #2 (I2C 0x44) — probe
 */
class NessoDiagModule : public concurrency::OSThread
{
  public:
    NessoDiagModule();

  private:
    CallbackObserver<NessoDiagModule, const InputEvent *> inputObserver =
        CallbackObserver<NessoDiagModule, const InputEvent *>(this, &NessoDiagModule::handleInputEvent);

    int handleInputEvent(const InputEvent *event);
    int32_t runOnce() override;

    // Probe one I2C address; returns true if device ACKs. Does NOT touch
    // the failure/recovery counters in variant.cpp.
    static bool rawProbe(uint8_t addr);

    // Probe + read one register; returns true on success.
    static bool rawReadReg(uint8_t addr, uint8_t reg, uint8_t *out);
};

extern NessoDiagModule *nessoDiag;

#endif // ARDUINO_NESSO_N1
