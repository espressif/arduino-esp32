
#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"
/*
            Arduino Pin Definitions for MakerGO ESP32 C6 SuperMini
  +-----------------------------------------------------------------------------+
  |     |         |        |  # |    | USB |    |  # |          |         |     |
  |:---:|:-------:|:------:|:--:|:--:|:---:|:--:|:--:|:--------:|:-------:|:---:|
  | D16 |    TX   | GPIO16 |  1 |    | TOP |    | 20 |    5V    |         |     |
  | D17 |    RX   | GPIO17 |  2 |    |     |    | 19 |    GND   |         |     |
  |  D0 |    A0   |  GPIO0 |  3 |    |     |    | 18 | 3V3(OUT) |         |     |
  |  D1 |    A1   |  GPIO1 |  4 |    |     |    | 17 |  GPIO20  |   SDA   | D14 |
  |  D2 |    A2   |  GPIO2 |  5 |    |     |    | 16 |  GPIO19  |   SCL   | D12 |
  |  D3 |    A3   |  GPIO3 |  6 |    |     |    | 15 |  GPIO18  |         | D11 |
  |  D4 |  SS/A4  |  GPIO4 |  7 |    |  21 |    | 14 |  GPIO15  |   LED   | D13 |
  |  D5 | MOSI/A5 |  GPIO5 |  8 | 23 |  22 |    | 13 |  GPIO14  |         | D10 |
  |  D6 | MISO/A6 |  GPIO6 |  9 |    |     | 24 | 12 |   GPIO9  |   BOOT  |  D9 |
  |  D7 |   SCK   |  GPIO7 | 10 |    |     | 25 | 11 |   GPIO8  | RGB_LED |  D8 |
  |     |         |        |    |    |  ↑  |    |    |          |         |     |
  +-----------------------------------  |  -------------------------------------+
                                        |
  |     |         |        |  # |    |  |  |    |  # |          |         |     |
  |:---:|:-------:|:------:|:--:|:--:|:---:|:--:|:--:|:--------:|:-------:|:---:|
  | D19 |         | GPIO21 | 21 |    |     |    |    |          |         |     |
  | D20 |         | GPIO22 | 22 |    |     |    | 24 |  GPIO12  |         | D15 |
  | D21 |         | GPIO23 | 23 |    |     |    | 25 |  GPIO13  |         | D18 |
  +-----------------------------------------------------------------------------+
*/
// The built-in RGB LED is connected to this pin
static const uint8_t PIN_RGB_LED = 8;
#define PIN_RGB_LED PIN_RGB_LED  // allow testing #ifdef PIN_RGB_LED

// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
// but also used in new Arduino API rgbLedWrite()
static const uint8_t RGB_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_RGB_LED;
#define RGB_BUILTIN RGB_BUILTIN  // allow testing #ifdef RGB_BUILTIN

// Define default brightness for the built-in RGB LED
#define RGB_BRIGHTNESS 32  // default brightness level (0-255)

// Define the color order for the built-in RGB LED
#define RGB_BUILTIN_LED_COLOR_ORDER LED_COLOR_ORDER_GRB  // default WS2812B color order

// Define the built-in LED pin (blue LED)
static const uint8_t LED_BUILTIN = 15;
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t TX = 16;
static const uint8_t RX = 17;

static const uint8_t SDA = 20;
static const uint8_t SCL = 19;

static const uint8_t SS = 4;
static const uint8_t MOSI = 5;
static const uint8_t MISO = 6;
static const uint8_t SCK = 7;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;
static const uint8_t A4 = 4;  // Note: A4 overlaps with SS
static const uint8_t A5 = 5;  // Note: A5 overlaps with MOSI
static const uint8_t A6 = 6;  // Note: A6 overlaps with MISO or SDA1

static const uint8_t D0 = 0;
static const uint8_t D1 = 1;
static const uint8_t D2 = 2;
static const uint8_t D3 = 3;
static const uint8_t D4 = 4;  // Note: D4 overlaps with SS
static const uint8_t D5 = 5;  // Note: D5 overlaps with MOSI
static const uint8_t D6 = 6;  // Note: D6 overlaps with MISO or SDA1
static const uint8_t D7 = 7;
static const uint8_t D8 = 8;  // Note: D8 overlaps with PIN_RGB_LED
static const uint8_t D9 = 9;
static const uint8_t D10 = 14;
static const uint8_t D11 = 18;
static const uint8_t D12 = 19;  // Note: D12 overlaps with SCL
static const uint8_t D13 = 15;  // Note: D13 overlaps with LED_BUILTIN
static const uint8_t D14 = 20;  // Note: D14 overlaps with SDA
static const uint8_t D15 = 12;
static const uint8_t D16 = 16;  // Note: D16 overlaps with TX
static const uint8_t D17 = 17;  // Note: D17 overlaps with RX
static const uint8_t D18 = 13;
static const uint8_t D19 = 21;
static const uint8_t D20 = 22;
static const uint8_t D21 = 23;

// LP I2C Pins are fixed on ESP32-C6
#define WIRE1_PIN_DEFINED
static const uint8_t SDA1 = 6;
static const uint8_t SCL1 = 7;

#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define BUILTIN_RGB RGB_BUILTIN  // backward compatibility

#endif /* Pins_Arduino_h */
