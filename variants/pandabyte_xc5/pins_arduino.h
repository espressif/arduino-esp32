#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

static const uint8_t LED_BUILTIN = 23;
static const uint8_t RGB_BUILTIN = 10;
static const uint8_t BUTTON_BUILTIN = 24;

static const uint8_t TX = 11;
static const uint8_t RX = 12;

// static const uint8_t USB_DM = 13;
// static const uint8_t USB_DP = 14;

static const uint8_t SDA = 0;
static const uint8_t SCL = 1;

static const uint8_t SS = 6;
static const uint8_t MOSI = 8;
static const uint8_t MISO = 9;
static const uint8_t SCK = 10;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;

// LP I2C Pins are fixed on ESP32-C5
static const uint8_t LP_SDA = 2;
static const uint8_t LP_SCL = 3;
#define WIRE1_PIN_DEFINED
#define SDA1 LP_SDA
#define SCL1 LP_SCL

// LP UART Pins are fixed on ESP32-C5
static const uint8_t LP_RX = 4;
static const uint8_t LP_TX = 5;

#endif /* Pins_Arduino_h */
