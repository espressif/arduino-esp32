#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define BUTTON 9

// Neopixel
#define PIN_NEOPIXEL 2
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_NEOPIXEL;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API rgbLedWrite()
#define RGB_BUILTIN    LED_BUILTIN
#define RGB_BRIGHTNESS 64

static const uint8_t TX = 21;
static const uint8_t RX = 20;

static const uint8_t SDA = 5;
static const uint8_t SCL = 6;

static const uint8_t SS = 6;
static const uint8_t MOSI = 7;
static const uint8_t MISO = 8;
static const uint8_t SCK = 10;

static const uint8_t A0 = 4;
static const uint8_t A1 = 3;
static const uint8_t A2 = 1;
static const uint8_t A3 = 0;
static const uint8_t A4 = 5;

#endif /* Pins_Arduino_h */
