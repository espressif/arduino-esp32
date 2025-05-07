#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// User LED
static const uint8_t LED_BUILTIN = 4;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

// Neopixel
static const uint8_t PIN_NEOPIXEL = 18;
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API rgbLedWrite() and digitalWrite() for blinking
#define RGB_BUILTIN    (PIN_NEOPIXEL + SOC_GPIO_PIN_COUNT)
#define RGB_BRIGHTNESS 64

static const uint8_t TX = 25;  // not broken out, defined to fix compilation
static const uint8_t RX = 26;  // not broken out, defined to fix compilation

#define TX1 TX
#define RX1 RX

static const uint8_t SDA = 13;  // not broken out, defined to fix compilation
static const uint8_t SCL = 15;  // not broken out, defined to fix compilation

static const uint8_t SS = 19;    // not broken out, defined to fix compilation
static const uint8_t MOSI = 21;  // output to drive dotstars
static const uint8_t SCK = 22;   // output to drive dotstars
static const uint8_t MISO = 23;  // not broken out, defined to fix compilation

static const uint8_t A0 = 2;  // not broken out, defined to fix compilation

// internal switch
static const uint8_t BUTTON = 0;

static const uint8_t DAC1 = 25;  // not broken out, defined to fix compilation
static const uint8_t DAC2 = 26;  // not broken out, defined to fix compilation

#endif /* Pins_Arduino_h */
