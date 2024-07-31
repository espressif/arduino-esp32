#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define PIN_NEOPIXEL 9
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = 15;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API neopixelWrite()
#define RGB_BUILTIN    (SOC_GPIO_PIN_COUNT + PIN_NEOPIXEL)
#define RGB_BRIGHTNESS 64

#define NEOPIXEL_I2C_POWER     20  // I2C power pin
#define PIN_NEOPIXEL_I2C_POWER 20  // I2C power pin

static const uint8_t TX = 16;
static const uint8_t RX = 17;

static const uint8_t SDA = 19;
static const uint8_t SCL = 18;

static const uint8_t SS = 0;
static const uint8_t MOSI = 22;
static const uint8_t MISO = 23;
static const uint8_t SCK = 21;

static const uint8_t A0 = 1;
static const uint8_t A1 = 4;
static const uint8_t A2 = 6;
static const uint8_t A3 = 5;
static const uint8_t A4 = 3;
static const uint8_t A5 = 2;
static const uint8_t A6 = 0;

#endif /* Pins_Arduino_h */
