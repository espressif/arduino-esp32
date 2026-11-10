#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define PIN_RGB_LED 8
// Use the virtual RGB LED pin so the standard Blink example works.
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_RGB_LED;
#define BUILTIN_LED    LED_BUILTIN
#define LED_BUILTIN    LED_BUILTIN
#define RGB_BUILTIN    LED_BUILTIN
#define RGB_BRIGHTNESS 64

// The BOOT button is active low. The reset button is connected to CHIP_EN.
static const uint8_t BOOT_BUILTIN = 9;

static const uint8_t TX = 16;
static const uint8_t RX = 17;

// These pins are also connected to the on-board I2C connector.
static const uint8_t SDA = 6;
static const uint8_t SCL = 7;

// Default SPI pins on the expansion header.
static const uint8_t SS = 18;
static const uint8_t MOSI = 19;
static const uint8_t MISO = 20;
static const uint8_t SCK = 21;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;
static const uint8_t A4 = 4;
static const uint8_t A5 = 5;
static const uint8_t A6 = 6;

// LP I2C pins are fixed on ESP32-C6 and share the on-board I2C connector.
#define WIRE1_PIN_DEFINED
static const uint8_t SDA1 = 6;
static const uint8_t SCL1 = 7;

#endif /* Pins_Arduino_h */
