#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x1001

// Pixie has a built in RGB LED WS2812B and a regular LED
#define PIN_RGB_LED 21
#define PIN_LED     11
// Regular built-in LED (pin 11) - for use with digitalWrite()
#define LED_BUILTIN PIN_LED
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
// RGB LED (pin 21) - use with RGB library functions
#define RGB_LED PIN_RGB_LED
// Allow testing for LED_BUILTIN
#ifdef LED_BUILTIN
// Defined and ready to use
#endif

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 10;
static const uint8_t SCL = 11;

static const uint8_t SS = 1;
static const uint8_t MOSI = 12;
static const uint8_t MISO = 13;
static const uint8_t SCK = 14;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A6 = 7;
static const uint8_t A7 = 8;
static const uint8_t A8 = 9;
static const uint8_t A9 = 10;
static const uint8_t A10 = 11;
static const uint8_t A11 = 12;
static const uint8_t A12 = 13;
static const uint8_t A13 = 14;
static const uint8_t A14 = 15;
static const uint8_t A15 = 16;
static const uint8_t A16 = 17;
static const uint8_t A17 = 18;
static const uint8_t A18 = 19;
static const uint8_t A19 = 20;

static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T4 = 4;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;
static const uint8_t T10 = 10;
static const uint8_t T11 = 11;
static const uint8_t T12 = 12;
static const uint8_t T13 = 13;
static const uint8_t T14 = 14;

#endif /* Pins_Arduino_h */
