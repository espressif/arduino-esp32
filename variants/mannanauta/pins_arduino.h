#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 5;
static const uint8_t SCL = 4;

static const uint8_t SS = 10;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 13;
static const uint8_t SCK = 12;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A6 = 7;
static const uint8_t A7 = 8;
static const uint8_t A8 = 9;
static const uint8_t A9 = 10;
static const uint8_t A10 = 11;
static const uint8_t A11 = 12;
static const uint8_t A12 = 13;
static const uint8_t A13 = 14;
static const uint8_t A14 = 15;
static const uint8_t A15 = 16;
static const uint8_t A16 = 17;
static const uint8_t A17 = 18;
static const uint8_t A18 = 19;
static const uint8_t A19 = 20;

static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T4 = 4;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;
static const uint8_t T10 = 10;
static const uint8_t T11 = 11;
static const uint8_t T12 = 12;
static const uint8_t T13 = 13;
static const uint8_t T14 = 14;

static const uint8_t BOOT_BUTTON = 0;

static const uint8_t TOUCH_LEFT = 7;
static const uint8_t TOUCH_RIGHT = 6;

#define LSM6DS_I2C_ADDRESS 0x6A

#define PIN_NEOPIXEL  47
#define NEOPIXEL_PIN  PIN_NEOPIXEL
#define NEOPIXEL_NUM  25

// The built-in RGB LED API and the Blink example control only the first LED
// in the 25-pixel WS2812 matrix. Use a dedicated library to control the full matrix.
#define RGB_BUILTIN    (PIN_NEOPIXEL + SOC_GPIO_PIN_COUNT)
#define RGB_BRIGHTNESS 64

#define LED_BUILTIN RGB_BUILTIN
#define BUILTIN_LED LED_BUILTIN  // backward compatibility

#endif /* Pins_Arduino_h */
