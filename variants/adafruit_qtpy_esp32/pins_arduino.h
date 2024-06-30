#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// Neopixel
#define PIN_NEOPIXEL 5
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_NEOPIXEL;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API neopixelWrite()
#define RGB_BUILTIN    LED_BUILTIN
#define RGB_BRIGHTNESS 64

#define NEOPIXEL_POWER 8

static const uint8_t TX = 32;
static const uint8_t RX = 7;

#define TX1 32
#define RX1 7

static const uint8_t SDA = 4;
static const uint8_t SCL = 33;

#define WIRE1_PIN_DEFINED 1  // See Wire.cpp at bool TwoWire::initPins(int sdaPin, int sclPin)
static const uint8_t SDA1 = 22;
static const uint8_t SCL1 = 19;

static const uint8_t SS = 27;
static const uint8_t MOSI = 13;
static const uint8_t MISO = 12;
static const uint8_t SCK = 14;

static const uint8_t A0 = 26;
static const uint8_t A1 = 25;
static const uint8_t A2 = 27;
static const uint8_t A3 = 15;
static const uint8_t A4 = 4;
static const uint8_t A5 = 33;
static const uint8_t A6 = 32;
static const uint8_t A7 = 7;
static const uint8_t A8 = 14;
static const uint8_t A9 = 12;
static const uint8_t A10 = 13;

static const uint8_t BUTTON = 0;

static const uint8_t T0 = 4;
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
