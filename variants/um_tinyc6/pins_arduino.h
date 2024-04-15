#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303A
#define USB_PID 0x1001
#define USB_MANUFACTURER "Unexpected Maker"
#define USB_PRODUCT "TinyC6"
#define USB_SERIAL ""

static const uint8_t TX = 16;
static const uint8_t RX = 17;

static const uint8_t SDA = 6;
static const uint8_t SCL = 7;

static const uint8_t SS = 18;
static const uint8_t MOSI = 21;
static const uint8_t MISO = 20;
static const uint8_t SDO = 21;
static const uint8_t SDI = 20;
static const uint8_t SCK = 19;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A6 = 7;
static const uint8_t A7 = 8;
static const uint8_t A8 = 9;

static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T4 = 4;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;

static const uint8_t VBAT_SENSE = 4;
static const uint8_t VBUS_SENSE = 10;

static const uint8_t RGB_DATA = 23;
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API neopixelWrite()
#define RGB_BUILTIN (RGB_DATA + SOC_GPIO_PIN_COUNT)
#define RGB_BRIGHTNESS 64
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = RGB_BUILTIN;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t RGB_PWR = 22;

#endif /* Pins_Arduino_h */
