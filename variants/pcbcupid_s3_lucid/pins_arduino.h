#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#include "soc/soc_caps.h"

#define USB_VID          0x303A
#define USB_PID          0x8401
#define USB_MANUFACTURER "PCBCupid"
#define USB_PRODUCT      "Pcbcupid GLYPH S3 Lucid"
#define USB_SERIAL       ""

// Built-in NeoPixel
#define PIN_NEOPIXEL 33
#define NEOPIXEL_NUM 1
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_NEOPIXEL;
#define BUILTIN_LED    LED_BUILTIN
#define LED_BUILTIN    LED_BUILTIN
#define RGB_BUILTIN    LED_BUILTIN
#define RGB_BRIGHTNESS 64

// Battery measurement (ADC2_CH0, 200K/200K divider -> multiply reading by 2)
static const uint8_t BAT_MEASURE = 11;
#define BAT_VOLT_PIN BAT_MEASURE
#define MSR          BAT_MEASURE

// Serial
static const uint8_t TX = 43;
static const uint8_t RX = 44;

// I2C
static const uint8_t SDA = 4;
static const uint8_t SCL = 5;

// SPI (FSPI) - SCK/MOSI are shared with the on-board LCD (LCD CS = GPIO21)
static const uint8_t SS = 10;
static const uint8_t MOSI = 36;
static const uint8_t MISO = 37;
static const uint8_t SCK = 35;

// Analog pins
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A6 = 6;
static const uint8_t A7 = 7;
static const uint8_t A8 = 8;
static const uint8_t A9 = 9;
static const uint8_t A10 = 10;

// Digital pins
static const uint8_t D1 = 1;
static const uint8_t D2 = 2;
static const uint8_t D4 = 4;
static const uint8_t D5 = 5;
static const uint8_t D6 = 6;
static const uint8_t D7 = 7;
static const uint8_t D8 = 8;
static const uint8_t D9 = 9;
static const uint8_t D10 = 10;
static const uint8_t D13 = 13;
static const uint8_t D14 = 14;
static const uint8_t D15 = 15;
static const uint8_t D16 = 16;
static const uint8_t D17 = 17;
static const uint8_t D18 = 18;
static const uint8_t D35 = 35;
static const uint8_t D36 = 36;
static const uint8_t D37 = 37;
static const uint8_t D43 = 43;
static const uint8_t D44 = 44;

#endif /* Pins_Arduino_h */
