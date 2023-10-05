#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x1001

static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + 45;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
#define RGB_BUILTIN LED_BUILTIN
#define RGB_BRIGHTNESS 64

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 12;
static const uint8_t SCL = 13;

static const uint8_t SDA1 = 2;
static const uint8_t SCL1 = 1;

static const uint8_t SS    = 41;
static const uint8_t MOSI  = 40;
static const uint8_t MISO  = 39;
static const uint8_t SCK   = 38;

static const uint8_t D0 = 1;
static const uint8_t D1 = 2;
static const uint8_t D2 = 44;
static const uint8_t D3 = 43;
static const uint8_t D4 = 42;
static const uint8_t D5 = 41;
static const uint8_t D6 = 40;
static const uint8_t D7 = 39;
static const uint8_t D8 = 38;
static const uint8_t D9 = 27;
static const uint8_t D10 = 45;
static const uint8_t D11 = 4;
static const uint8_t D12 = 5;
static const uint8_t D13 = 6;
static const uint8_t D14 = 7;
static const uint8_t D15 = 15;
static const uint8_t D16 = 16;
static const uint8_t D17 = 17;
static const uint8_t D18 = 18;

static const uint8_t A0 = 4;
static const uint8_t A1 = 5;
static const uint8_t A2 = 6;
static const uint8_t A3 = 7;
static const uint8_t A4 = 1;
static const uint8_t A5 = 2;


#endif /* Pins_Arduino_h */
