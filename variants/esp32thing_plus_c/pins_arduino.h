#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

static const uint8_t LED_BUILTIN = 13;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API neopixelWrite()
static const uint8_t RGB_BUILTIN = SOC_GPIO_PIN_COUNT + 2;
#define RGB_BUILTIN RGB_BUILTIN  // necessary to make digitalWrite/digitalMode find it
#define RGB_BRIGHTNESS 64

static const uint8_t TX = 17;
static const uint8_t RX = 16;

#define TX1 TX
#define RX1 RX

static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

static const uint8_t SS = 15;
static const uint8_t MOSI = 23;
static const uint8_t MISO = 19;
static const uint8_t SCK = 18;

static const uint8_t A0 = 26;
static const uint8_t A1 = 25;
static const uint8_t A2 = 34;
static const uint8_t A3 = 39;
static const uint8_t A4 = 36;
static const uint8_t A5 = 35;
static const uint8_t A6 = 14;
static const uint8_t A7 = 32;
static const uint8_t A8 = 15;
static const uint8_t A9 = 33;
static const uint8_t A10 = 27;
static const uint8_t A11 = 12;
static const uint8_t A12 = 13;


static const uint8_t T0 = 4;
static const uint8_t T1 = 0;
static const uint8_t T2 = 2;
static const uint8_t T3 = 15;
static const uint8_t T4 = 13;
static const uint8_t T5 = 12;
static const uint8_t T6 = 14;
static const uint8_t T7 = 27;
static const uint8_t T8 = 33;
static const uint8_t T9 = 32;

static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

#endif /* Pins_Arduino_h */
