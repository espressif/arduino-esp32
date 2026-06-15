#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x1001

#define PIN_RGB_LED 60
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_RGB_LED;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API rgbLedWrite()
#define RGB_BUILTIN    LED_BUILTIN
#define RGB_BRIGHTNESS 64

static const uint8_t TX = 58;
static const uint8_t RX = 59;

static const uint8_t SDA = 2;
static const uint8_t SCL = 3;

static const uint8_t SS = 37;
static const uint8_t MOSI = 38;
static const uint8_t MISO = 39;
static const uint8_t SCK = 40;

static const uint8_t A0 = 42;
static const uint8_t A1 = 43;
static const uint8_t A2 = 44;
static const uint8_t A3 = 45;
static const uint8_t A4 = 46;
static const uint8_t A5 = 47;
static const uint8_t A6 = 48;
static const uint8_t A7 = 49;
static const uint8_t A8 = 50;
static const uint8_t A9 = 51;
static const uint8_t A10 = 52;
static const uint8_t A11 = 53;
static const uint8_t A12 = 54;
static const uint8_t A13 = 55;
static const uint8_t A14 = 56;
static const uint8_t A15 = 57;

static const uint8_t T0 = 6;
static const uint8_t T1 = 7;
static const uint8_t T2 = 8;
static const uint8_t T3 = 9;
static const uint8_t T4 = 10;
static const uint8_t T5 = 11;
static const uint8_t T6 = 12;
static const uint8_t T7 = 13;
static const uint8_t T8 = 14;
static const uint8_t T9 = 15;
static const uint8_t T10 = 16;
static const uint8_t T11 = 17;
static const uint8_t T12 = 18;
static const uint8_t T13 = 19;

#endif /* Pins_Arduino_h */
