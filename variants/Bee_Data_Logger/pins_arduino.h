#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303A
#define USB_PID 0x815C
#define USB_MANUFACTURER "Smart Bee Designs"
#define USB_PRODUCT "Bee Data Logger"
#define USB_SERIAL ""

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 37;
static const uint8_t SCL = 36;

static const uint8_t SS = 47;
static const uint8_t MOSI = 46;
static const uint8_t MISO = 45;
static const uint8_t SDO = 35;
static const uint8_t SDI = 37;
static const uint8_t SCK = 48;

static const uint8_t A3 = 3;
static const uint8_t A4 = 4;
static const uint8_t A5 = 5;
static const uint8_t A6 = 6;
static const uint8_t A7 = 7;
static const uint8_t A8 = 8;
static const uint8_t A9 = 9;


static const uint8_t D3 = 3;
static const uint8_t D4 = 4;
static const uint8_t D5 = 5;
static const uint8_t D6 = 6;
static const uint8_t D7 = 7;
static const uint8_t D8 = 8;
static const uint8_t D9 = 9;
static const uint8_t D38 = 38;
static const uint8_t D39 = 39;
static const uint8_t D41 = 41;
static const uint8_t D42 = 42;
static const uint8_t D43 = 43;
static const uint8_t D44 = 44;


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

static const uint8_t BOOT_BTN = 0;
static const uint8_t VBAT_VOLTAGE = 1;
static const uint8_t VBUS_SENSE = 2;
static const uint8_t LDO2 = 34;
static const uint8_t RGB_DATA = 40;
static const uint8_t RGB_PWR = 34;

#define PIN_NEOPIXEL RGB_DATA
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_NEOPIXEL;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API neopixelWrite()
#define RGB_BUILTIN LED_BUILTIN
#define RGB_BRIGHTNESS 64

#endif /* Pins_Arduino_h */
