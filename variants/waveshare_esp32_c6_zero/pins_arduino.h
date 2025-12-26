#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"
/*
       Arduino Pin Definitions for Waveshare ESP32-C6-Zero
  +----------------------------------------------------------------+
  |     |         |          |  # | USB |  # |        |      |     |
  |:---:|:-------:|:--------:|:--:|:---:|:--:|:------:|:----:|:---:|
  |     |         |    5V    |  1 | TOP | 18 | GPIO16 |  TX  | D16 |
  |     |         |    GND   |  2 | TOP | 17 | GPIO17 |  RX  | D17 |
  |     |         | 3V3(OUT) |  3 | TOP | 16 | GPIO14 |  SDA | D12 |
  |  D0 |    A0   |   GPIO0  |  4 | TOP | 15 | GPIO15 |  SCL | D11 |
  |  D1 |    A1   |   GPIO1  |  5 | TOP | 14 | GPIO18 |  SS  | D10 |
  |  D2 |    A2   |   GPIO2  |  6 | TOP | 13 | GPIO19 | MOSI |  D9 |
  |  D3 |    A3   |   GPIO3  |  7 | TOP | 12 | GPIO20 | MISO |  D8 |
  |  D4 |    A4   |   GPIO4  |  8 | TOP | 11 | GPIO21 |  SCK |  D7 |
  |  D5 |    A5   |   GPIO5  |  9 | TOP | 10 | GPIO22 |      |  D6 |
  +----------------------------------------------------------------+

  +----------------------------------------------------------------+
  |     |         |          |  # | USB |  # |        |      |     |
  |:---:|:-------:|:--------:|:--:|:---:|:--:|:------:|:----:|:---:|
  |     |         |          |    | BOT |    |        |      |     |
  |     |         |          |    | BOT |    |        |      |     |
  | D21 |         |  GPIO13  | 19 | BOT |    |        |      |     |
  | D20 |         |  GPIO12  | 20 | BOT |    |        |      |     |
  | D19 |         |  GPIO23  | 21 | BOT |    |        |      |     |
  | D18 |   BOOT  |   GPIO9  | 22 | BOT |    |        |      |     |
  | D13 | RGB_LED |   GPIO8  | 23 | BOT |    |        |      |     |
  | D15 |         |   GPIO7  | 24 | BOT |    |        |      |     |
  | D14 |    A6   |   GPIO6  | 25 | BOT |    |        |      |     |
  +----------------------------------------------------------------+
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
#define RGB_BUILTIN_LED_COLOR_ORDER LED_COLOR_ORDER_RGB

// Define the built-in as LED pin (RGB LED) to use with digitalWrite()
static const uint8_t LED_BUILTIN = RGB_BUILTIN;
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t TX = 16;
static const uint8_t RX = 17;

static const uint8_t SDA = 14;
static const uint8_t SCL = 15;

static const uint8_t SS = 18;
static const uint8_t MOSI = 19;
static const uint8_t MISO = 20;
static const uint8_t SCK = 21;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;
static const uint8_t A4 = 4;
static const uint8_t A5 = 5;
static const uint8_t A6 = 6;

static const uint8_t D0 = 0;
static const uint8_t D1 = 1;
static const uint8_t D2 = 2;
static const uint8_t D3 = 3;
static const uint8_t D4 = 4;
static const uint8_t D5 = 5;
static const uint8_t D6 = 22;
static const uint8_t D7 = 21;
static const uint8_t D8 = 20;
static const uint8_t D9 = 19;
static const uint8_t D10 = 18;
static const uint8_t D11 = 15;
static const uint8_t D12 = 14;
static const uint8_t D13 = 8;
static const uint8_t D14 = 6;
static const uint8_t D15 = 7;
static const uint8_t D16 = 16;
static const uint8_t D17 = 17;
static const uint8_t D18 = 9;
static const uint8_t D19 = 23;
static const uint8_t D20 = 12;
static const uint8_t D21 = 13;

// LP I2C Pins are fixed on ESP32-C6
#define WIRE1_PIN_DEFINED
static const uint8_t SDA1 = 6;
static const uint8_t SCL1 = 7;

#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define BUILTIN_RGB RGB_BUILTIN  // backward compatibility

#endif /* Pins_Arduino_h */
