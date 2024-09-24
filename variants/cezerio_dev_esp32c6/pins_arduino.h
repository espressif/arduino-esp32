#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID          0x303A
#define USB_PID          0x1001 //set uniq PID
#define USB_MANUFACTURER "RFtek Electronics"
#define USB_PRODUCT      "CEZERIO DEV ESP32-C6"
#define USB_SERIAL       ""

#define PIN_RGB_LED 8
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_RGB_LED;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API rgbLedWrite()
#define RGB_BUILTIN LED_BUILTIN
#define RGB_BRIGHTNESS 64

static const uint8_t BUT_BUILTIN = 9;
#define BUILTIN_BUT BUT_BUILTIN  // backward compatibility
#define BUT_BUILTIN BUT_BUILTIN  // allow testing #ifdef BUT_BUILTIN

static const uint8_t TX = 16;
static const uint8_t RX = 17;

static const uint8_t SDA = 19;
static const uint8_t SCL = 18;

static const uint8_t SS = 0;
static const uint8_t MOSI = 22;
static const uint8_t MISO = 23;
static const uint8_t SCK = 21;

static const uint8_t A0 = 1;
static const uint8_t A1 = 4;
static const uint8_t A2 = 6;
static const uint8_t A3 = 5;
static const uint8_t A4 = 3;
static const uint8_t A5 = 2;
static const uint8_t A6 = 0;

#endif /* Pins_Arduino_h */
