#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x1001


#define RGB_PIN 15

static const uint8_t LED_BUILTIN = 35;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API rgbLedWrite()
#define RGB_BUILTIN    RGB_PIN

static const uint8_t TX = 39;
static const uint8_t RX = 40;

static const uint8_t SDA = 39;
static const uint8_t SCL = 40;

static const uint8_t SS = 1;
static const uint8_t MOSI = 2;
static const uint8_t MISO = 3;
static const uint8_t SCK = 4;

static const uint8_t D6_OUT_PIN = 35;
static const uint8_t D9_OUT_PIN = 36;
static const uint8_t D10_OUT_PIN = 10;

static const uint8_t TOUCH_PIN = 13;

static const uint8_t TRIG_PIN = 5; // GPIO connected to HC-SR04 TRIG
static const uint8_t ECHO_PIN = 6; // GPIO connected to HC-SR04 ECHO

static const uint8_t latchPin = 34;
static const uint8_t clockPin = 47;
static const uint8_t dataPin = 48;

static const uint8_t D_IN_4 = 8;
static const uint8_t D_IN_8 = 11;
static const uint8_t D_IN_12 = 9;

static const uint8_t AN_IN_4 = 17;
static const uint8_t AN_IN_8 = 16;
static const uint8_t AN_IN_12 = 7;

static const uint8_t S1pin = 37;
static const uint8_t S2pin = 38;
static const uint8_t S3pin = 14;

#endif /* Pins_Arduino_h */