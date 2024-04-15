#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define PIN_NEOPIXEL 8
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_NEOPIXEL;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API neopixelWrite()
#define RGB_BUILTIN LED_BUILTIN
#define RGB_BRIGHTNESS 64


static const uint8_t KEY_BUILTIN = 9;

static const uint8_t TX = 24;
static const uint8_t RX = 23;

static const uint8_t SDA = 12;
static const uint8_t SCL = 22;

static const uint8_t SS = 0;
static const uint8_t MOSI = 25;
static const uint8_t MISO = 11;
static const uint8_t SCK = 10;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;

#endif /* Pins_Arduino_h */
