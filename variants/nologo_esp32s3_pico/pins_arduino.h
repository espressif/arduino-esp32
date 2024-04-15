#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x1001

static const uint8_t LED_BUILTIN = 21;
#define BUILTIN_LED LED_BUILTIN
#define LED_BUILTIN LED_BUILTIN
#define RGB_BUILTIN SOC_GPIO_PIN_COUNT + LED_BUILTIN
#define RGB_BRIGHTNESS 64

// SPI - unused but you can create your own definition in your sketch
static const int8_t SCK = -1;
static const int8_t MISO = -1;
static const int8_t MOSI = -1;
static const int8_t SS = -1;

// I2C - unused but you can create your own definition in your sketch
static const uint8_t SDA = -1;
static const uint8_t SCL = -1;

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

#endif /* Pins_Arduino_h */
