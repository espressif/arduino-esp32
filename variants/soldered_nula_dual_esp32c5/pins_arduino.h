#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define PIN_RGB_LED 8
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_RGB_LED;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API rgbLedWrite()
#define RGB_BUILTIN    LED_BUILTIN
#define RGB_BRIGHTNESS 64

static const uint8_t TX = 11;
static const uint8_t RX = 12;

// static const uint8_t USB_DM = 13;
// static const uint8_t USB_DP = 14;

static const uint8_t SDA = 4;
static const uint8_t SCL = 5;

static const uint8_t SS = 10;
static const uint8_t MOSI = 7;
static const uint8_t MISO = 2;
static const uint8_t SCK = 6;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;
static const uint8_t A4 = 4;
static const uint8_t A5 = 5;
static const uint8_t A6 = 6;
static const uint8_t A7 = 7;
static const uint8_t A8 = 8;
static const uint8_t A9 = 9;
static const uint8_t A10 = 10;
static const uint8_t A13 = 13;
static const uint8_t A14 = 14;
static const uint8_t A23 = 23;
static const uint8_t A24 = 24;
static const uint8_t A25 = 25;
static const uint8_t A28 = 28;

// LP I2C Pins are fixed on ESP32-C5
static const uint8_t LP_SDA = 2;
static const uint8_t LP_SCL = 3;
#define WIRE1_PIN_DEFINED
#define SDA1 LP_SDA
#define SCL1 LP_SCL

// LP UART Pins are fixed on ESP32-C5
static const uint8_t LP_RX = 4;
static const uint8_t LP_TX = 5;

#endif /* Pins_Arduino_h */
