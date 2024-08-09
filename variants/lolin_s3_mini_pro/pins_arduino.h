#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x8216

static const uint8_t LED_BUILTIN = 8 + SOC_GPIO_PIN_COUNT;
;
#define BUILTIN_LED    LED_BUILTIN  // backward compatibility
#define LED_BUILTIN    LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
#define RGB_BUILTIN    LED_BUILTIN
#define RGB_BRIGHTNESS 5
#define RGB_POWER      7  //RGB LED POWER PIN

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 12;
static const uint8_t SCL = 11;

static const uint8_t SS = 37;
static const uint8_t MOSI = 38;
static const uint8_t MISO = 39;
static const uint8_t SCK = 40;

//TFT
static const uint8_t TFT_BL = 33;
static const uint8_t TFT_DC = 36;
static const uint8_t TFT_CS = 35;
static const uint8_t TFT_RST = 34;

//IR
static const uint8_t PIN_IR = 9;

//BUTTON
static const uint8_t BUTTON_LEFT = 0;
static const uint8_t BUTTON_OK = 47;
static const uint8_t BUTTON_RIGHT = 48;

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
