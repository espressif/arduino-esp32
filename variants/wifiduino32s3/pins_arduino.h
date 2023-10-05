#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x1001

// Some boards have too low voltage on this pin (board design bug)
// Use different pin with 3V and connect with 48
// and change this setup for the chosen pin (for example 38)
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT+48;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
#define RGB_BUILTIN LED_BUILTIN
#define RGB_BRIGHTNESS 64
#define BOARD_HAS_NEOPIXEL
#define LED_BRIGHTNESS 64

static const uint8_t TX = 45;
static const uint8_t RX = 44;

static const uint8_t SDA = 4;
static const uint8_t SCL = 5;

static const uint8_t SS    = 46;
static const uint8_t MOSI  = 3;
static const uint8_t MISO  = 20;
static const uint8_t SCK   = 19;

static const uint8_t A0 = 7;
static const uint8_t A1 = 6;
static const uint8_t A2 = 2;
static const uint8_t A3 = 1;
static const uint8_t A4 = 4;
static const uint8_t A5 = 5;

static const uint8_t D0 = 44;
static const uint8_t D1 = 45;
static const uint8_t D2 = 42;
static const uint8_t D3 = 41;
static const uint8_t D4 = 0;
static const uint8_t D5 = 45;
static const uint8_t D6 = 48;
static const uint8_t D7 = 47;
static const uint8_t D8 = 21;
static const uint8_t D9 = 14;
static const uint8_t D10 = 46;
static const uint8_t D11 = 3;
static const uint8_t D12 = 20;
static const uint8_t D13 = 19;

#endif /* Pins_Arduino_h */
