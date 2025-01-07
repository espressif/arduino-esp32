#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// User LED
static const uint8_t LED_BUILTIN = 4;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

// Neopixel
static const uint8_t PIN_NEOPIXEL = 2;
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API rgbLedWrite() and digitalWrite() for blinking
#define RGB_BUILTIN    (PIN_NEOPIXEL + SOC_GPIO_PIN_COUNT)
#define RGB_BRIGHTNESS 64

static const uint8_t TX = 9;
static const uint8_t RX = 10;

#define TX1 TX
#define RX1 RX

static const uint8_t SDA = 14;
static const uint8_t SCL = 13;

static const uint8_t SS = 23;
static const uint8_t MOSI = 9;
static const uint8_t MISO = 10;
static const uint8_t SCK = 18;

static const uint8_t A0 = 27;

// internal switch
static const uint8_t BUTTON = 0;

static const uint8_t T0 = 27;

static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

#endif /* Pins_Arduino_h */
