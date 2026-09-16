#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// The LCD and microSD card share clock and MOSI, with separate chip selects.
#define LCD_CLK 7
#define LCD_DIN 6
#define LCD_CS  23
#define LCD_DC  24
#define LCD_RST 26
#define LCD_BL  10

#define SD_MISO 5
#define SD_MOSI 6
#define SD_CLK  7
#define SD_SCLK SD_CLK
#define SD_CS   4

#define RGB_LED     8
#define PIN_RGB_LED RGB_LED

// Addressable RGB LEDs use the encoded built-in LED pin in Arduino APIs.
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_RGB_LED;
#define BUILTIN_LED                 LED_BUILTIN
#define LED_BUILTIN                 LED_BUILTIN
#define RGB_BUILTIN                 LED_BUILTIN
#define RGB_BRIGHTNESS              64
#define RGB_BUILTIN_LED_COLOR_ORDER LED_COLOR_ORDER_RGB

static const uint8_t TX = 11;
static const uint8_t RX = 12;

static const uint8_t USB_DM = 13;
static const uint8_t USB_DP = 14;

// This board has no dedicated default I2C pins.
static const uint8_t SDA = -1;
static const uint8_t SCL = -1;

static const uint8_t SS = SD_CS;
static const uint8_t MOSI = SD_MOSI;
static const uint8_t MISO = SD_MISO;
static const uint8_t SCK = SD_CLK;

#endif /* Pins_Arduino_h */
