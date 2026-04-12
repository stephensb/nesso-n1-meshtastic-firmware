#include "i2cButton.h"
#include "meshUtils.h"

#include "configuration.h"
#if defined(M5STACK_UNITC6L) || defined(ARDUINO_NESSO_N1)

#include "MeshService.h"
#include "RadioLibInterface.h"
#include "buzz.h"
#include "input/InputBroker.h"
#include "main.h"
#include "modules/CannedMessageModule.h"
#include "modules/ExternalNotificationModule.h"
#include "power.h"
#include "sleep.h"
#ifdef ARCH_PORTDUINO
#include "platform/portduino/PortduinoGlue.h"
#endif

i2cButtonThread *i2cButton;

using namespace concurrency;

extern bool i2c_read_byte(uint8_t addr, uint8_t reg, uint8_t *value);

extern void i2c_write_byte(uint8_t addr, uint8_t reg, uint8_t value);

#define PI4IO_M_ADDR 0x43
#define getbit(x, y) ((x) >> (y)&0x01)
#define PI4IO_REG_IRQ_STA 0x13
#define PI4IO_REG_IN_STA 0x0F
#define PI4IO_REG_CHIP_RESET 0x01

i2cButtonThread::i2cButtonThread(const char *name) : OSThread(name)
{
    _originName = name;
    if (inputBroker)
        inputBroker->registerSource(this);
}

int32_t i2cButtonThread::runOnce()
{
    // KEY1 (PI4IO 0x43 P0) state
    static bool btn1_pressed = false;
    static uint32_t press_start_time = 0;
    static bool long_press_triggered = false;

    // KEY2 (PI4IO 0x43 P1) state
    static bool btn2_pressed = false;
    static uint32_t press2_start_time = 0;
    static bool long2_press_triggered = false;

    const uint32_t LONG_PRESS_TIME = 1000;

    uint8_t in_data = 0;
    if (!i2c_read_byte(PI4IO_M_ADDR, PI4IO_REG_IRQ_STA, &in_data)) {
        // PI4IO read failed (I2C bus error). Skip this poll cycle.
        return 50;
    }
    i2c_write_byte(PI4IO_M_ADDR, PI4IO_REG_IRQ_STA, in_data);

    // Read input state once if either button triggered an IRQ
    uint8_t input_state = 0xFF; // safe default: no button pressed (active-low, pull-up)
    if (getbit(in_data, 0) || getbit(in_data, 1)) {
        i2c_read_byte(PI4IO_M_ADDR, PI4IO_REG_IN_STA, &input_state);
    }

    // --- KEY1 (P0 / bit 0): short press → USER_PRESS, long press → SELECT ---
    if (getbit(in_data, 0)) {
        if (!getbit(input_state, 0)) {
            // Button pressed (active low)
            if (!btn1_pressed) {
                btn1_pressed = true;
                press_start_time = millis();
                long_press_triggered = false;
            }
        } else {
            // Button released
            if (btn1_pressed) {
                btn1_pressed = false;
                uint32_t press_duration = millis() - press_start_time;
                if (long_press_triggered) {
                    long_press_triggered = false;
                } else if (press_duration < LONG_PRESS_TIME) {
                    InputEvent evt;
                    evt.source = "UserButton";
                    evt.inputEvent = INPUT_BROKER_USER_PRESS;
                    evt.kbchar = 0;
                    evt.touchX = 0;
                    evt.touchY = 0;
                    this->notifyObservers(&evt);
                }
            }
        }
    }

    // KEY1 long-press detection (checked every poll cycle while held)
    if (btn1_pressed && !long_press_triggered && (millis() - press_start_time >= LONG_PRESS_TIME)) {
        long_press_triggered = true;
        InputEvent evt;
        evt.source = "UserButton";
        evt.inputEvent = INPUT_BROKER_SELECT;
        evt.kbchar = 0;
        evt.touchX = 0;
        evt.touchY = 0;
        this->notifyObservers(&evt);
    }

    // --- KEY2 (P1 / bit 1): short press → ALT_PRESS, long press → ALT_LONG ---
    if (getbit(in_data, 1)) {
        if (!getbit(input_state, 1)) {
            // Button pressed (active low)
            if (!btn2_pressed) {
                btn2_pressed = true;
                press2_start_time = millis();
                long2_press_triggered = false;
            }
        } else {
            // Button released
            if (btn2_pressed) {
                btn2_pressed = false;
                uint32_t press_duration = millis() - press2_start_time;
                if (long2_press_triggered) {
                    long2_press_triggered = false;
                } else if (press_duration < LONG_PRESS_TIME) {
                    InputEvent evt;
                    evt.source = "AltButton";
                    evt.inputEvent = INPUT_BROKER_ALT_PRESS;
                    evt.kbchar = 0;
                    evt.touchX = 0;
                    evt.touchY = 0;
                    this->notifyObservers(&evt);
                }
            }
        }
    }

    // KEY2 long-press detection
    if (btn2_pressed && !long2_press_triggered && (millis() - press2_start_time >= LONG_PRESS_TIME)) {
        long2_press_triggered = true;
        InputEvent evt;
        evt.source = "AltButton";
        evt.inputEvent = INPUT_BROKER_ALT_LONG;
        evt.kbchar = 0;
        evt.touchX = 0;
        evt.touchY = 0;
        this->notifyObservers(&evt);
    }

    return 50;
}
#endif
