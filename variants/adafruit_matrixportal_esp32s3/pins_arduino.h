#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID            0x239A
#define USB_PID            0x8125
#define USB_MANUFACTURER   "Adafruit"
#define USB_PRODUCT        "MatrixPortal ESP32-S3"
#define USB_SERIAL         "" // Empty string for MAC adddress

#define PIN_NEOPIXEL        4
#define NEOPIXEL_PIN        4
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT+PIN_NEOPIXEL;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API neopixelWrite()
#define RGB_BUILTIN LED_BUILTIN
#define RGB_BRIGHTNESS 64

#define NEOPIXEL_NUM        1
#define PIN_LIGHTSENSOR     A5

#define PIN_BUTTON_UP       6
#define PIN_BUTTON_DOWN     7

static const uint8_t TX = 18;
static const uint8_t RX = 8;
#define TX1 TX
#define RX1 RX

static const uint8_t SDA = 16;
static const uint8_t SCL = 17;

static const uint8_t SS   = -1;
static const uint8_t MOSI = -1;
static const uint8_t SCK  = -1;
static const uint8_t MISO = -1;

static const uint8_t A0 = 12;
static const uint8_t A1 = 3;
static const uint8_t A2 = 9;
static const uint8_t A3 = 10;
static const uint8_t A4 = 11;
static const uint8_t A5 = 5; // Light

static const uint8_t T3  = 3;  // Touch pin IDs map directly
static const uint8_t T8  = 8;  // to underlying GPIO numbers NOT
static const uint8_t T9  = 9;  // the analog numbers on board silk
static const uint8_t T10 = 10;
static const uint8_t T11 = 11;
static const uint8_t T12 = 12;

#endif /* Pins_Arduino_h */
