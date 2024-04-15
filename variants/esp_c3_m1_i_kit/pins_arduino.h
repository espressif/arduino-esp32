/*
   For more information about this board:
   https://docs.ai-thinker.com/_media/esp32/docs/nodemcu-esp-c3-m1-i-kit_v1.2.0_specification.pdf
*/

#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// User LEDs are also connected to USB D- and D+
static const uint8_t LED_WARM = 18;
static const uint8_t LED_COLD = 19;

// RGB LED
static const uint8_t LED_RED = 3;
static const uint8_t LED_GREEN = 4;
static const uint8_t LED_BLUE = 5;

static const uint8_t LED_BUILTIN = LED_WARM;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

// Standard ESP32-C3 GPIOs
static const uint8_t TX = 21;
static const uint8_t RX = 20;

static const uint8_t SDA = 8;
static const uint8_t SCL = 9;

static const uint8_t SS = 7;
static const uint8_t MOSI = 6;
static const uint8_t MISO = 5;
static const uint8_t SCK = 4;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;
static const uint8_t A4 = 4;
static const uint8_t A5 = 5;

#endif /* Pins_Arduino_h */
