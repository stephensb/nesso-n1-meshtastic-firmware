#pragma once
#include <stdint.h> // for uint8_t in function declarations below

void c6l_init();
void gpio_ext_set(uint8_t address, uint8_t pin, bool value);
uint8_t gpio_ext_get(uint8_t address, uint8_t pin);
uint8_t gpio_ext_read_input(uint8_t address, uint8_t pin);

#define SERIAL_PRINT_PORT 1

#define USE_TFTDISPLAY 1

#define HAS_GPS 0
#define GPS_RX_PIN -1
#define GPS_TX_PIN -1

#define I2C_SDA 10
#define I2C_SCL 8

#define LCD_CS 17
#define LCD_RS 16
#define SYS_IRQ 3

// MOSI/MISO/SCK are defined as const variables in pins_arduino.h (21/22/20).
// Do NOT redefine them here as macros — doing so causes syntax errors when
// pins_arduino.h is included after this header.

#define PIN_BUZZER 11

#define IO_EXPANDER 0x40
#define LCD_BACKLIGHT 0x106

// #define BUTTON_PIN 9
#define BUTTON_EXTENDER

#undef LORA_SCK
#undef LORA_MISO
#undef LORA_MOSI
#undef LORA_CS

// battery charger AW32001E (I2C 0x49); registers configured in c6l_init()
//#define HAS_PPM 1
//#define XPOWERS_CHIP_BQ25896

// battery fuel gauge BQ27220 (I2C 0x55)
#define HAS_BQ27220 1
#define BQ27220_I2C_SDA I2C_SDA
#define BQ27220_I2C_SCL I2C_SCL
#define BQ27220_DESIGN_CAPACITY 250

// BMI270 6-axis IMU (I2C 0x68) — enables screen wake-on-motion
#define HAS_BMI270

// WaveShare Core1262-868M OK
// https://www.waveshare.com/wiki/Core1262-868M
#define USE_SX1262

#define LORA_MISO 22
#define LORA_SCK 20
#define LORA_MOSI 21
#define LORA_CS 23
#define LORA_RESET RADIOLIB_NC
#define LORA_DIO1 15
#define LORA_BUSY 19
#define SX126X_CS LORA_CS
#define SX126X_DIO1 LORA_DIO1
#define SX126X_BUSY LORA_BUSY
#define SX126X_RESET LORA_RESET
#define SX126X_DIO2_AS_RF_SWITCH
#define SX126X_DIO3_TCXO_VOLTAGE 3.0

#define ST7789_DRIVER
#define ST7789_CS 17
#define ST7789_RS 16
#define ST7789_SDA 21
#define ST7789_SCK 20
#define ST7789_RESET -1
#define ST7789_MISO 22
#define ST7789_BUSY -1
#define ST7789_SPI_HOST SPI2_HOST
#define SPI_FREQUENCY 40000000
#define SPI_READ_FREQUENCY 16000000
// Logical display dimensions (used for UI layout).
// Panel config native values are hardcoded in TFTDisplay.cpp for ARDUINO_NESSO_N1.
// Build with -D NESSO_PORTRAIT for portrait orientation.
#ifdef NESSO_PORTRAIT
#define TFT_WIDTH 135
#define TFT_HEIGHT 240
#else
#define TFT_WIDTH 240
#define TFT_HEIGHT 135
#endif
#define TFT_OFFSET_ROTATION 0
#define SCREEN_TRANSITION_FRAMERATE 10
#define BRIGHTNESS_DEFAULT 130
// FT5x06 touch (I2C 0x38) is present in hardware but disabled in firmware:
// its LovyanGFX init locks the I2C bus at boot, breaking BQ27220 and PI4IO.
// Set HAS_TOUCHSCREEN 0 to prevent TouchScreenImpl1 polling thread from running.
#define HAS_TOUCHSCREEN 0
#define TOUCH_I2C_PORT 0
#define TOUCH_SLAVE_ADDRESS 0x38
#define SCREEN_TOUCH_INT 3
#define TFT_BL_EXT (LCD_BACKLIGHT | IO_EXPANDER)
