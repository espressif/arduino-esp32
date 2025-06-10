#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID          0x303a
#define USB_PID          0x1001
#define USB_MANUFACTURER "openjumper"
#define USB_PRODUCT      "Wifiduino32-S3"
#define USB_SERIAL       ""  // Empty string for MAC address

// Some boards have too low voltage on this pin (board design bug)
// Use different pin with 3V and connect with 48
// and change this setup for the chosen pin (for example 38)
#define PIN_RGB_LED 48
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_RGB_LED;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API rgbLedWrite()
#define RGB_BUILTIN    LED_BUILTIN
#define RGB_BRIGHTNESS 64

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 4;
static const uint8_t SCL = 5;

static const uint8_t SS = 10;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 13;
static const uint8_t SCK = 12;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;
static const uint8_t A4 = 4;
static const uint8_t A5 = 5;

static const uint8_t D0 = 44;
static const uint8_t D1 = 43;
static const uint8_t D2 = 45;
static const uint8_t D3 = 46;
static const uint8_t D4 = 47;
static const uint8_t D5 = 48;
static const uint8_t D6 = 18;
static const uint8_t D7 = 17;
static const uint8_t D8 = 21;
static const uint8_t D9 = 42;
static const uint8_t D10 = 41;
static const uint8_t D11 = 40;
static const uint8_t D12 = 38;
static const uint8_t D13 = 39;

#endif /* Pins_Arduino_h */
