#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID          0x303A
#define USB_PID          0x1001
#define USB_MANUFACTURER "M5Stack"
#define USB_PRODUCT      "NanoC6"
#define USB_SERIAL       ""

static const uint8_t LED_BUILTIN = 7;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t TX = 16;
static const uint8_t RX = 17;

static const uint8_t SDA = 2;
static const uint8_t SCL = 1;

static const uint8_t SS = 4;    // Not connected
static const uint8_t MOSI = 5;  // Not connected
static const uint8_t MISO = 6;  // Not connected
static const uint8_t SCK = 8;   // Not connected

static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;

static const uint8_t D1 = 1;
static const uint8_t D2 = 2;
static const uint8_t D3 = 3;
static const uint8_t D4 = 7;
static const uint8_t D5 = 8;
static const uint8_t D6 = 9;

static const uint8_t BLUE_LED_PIN = 7;
static const uint8_t BTN_PIN = 9;
static const uint8_t IR_TX_PIN = 3;
static const uint8_t RGB_LED_PWR_PIN = 19;
static const uint8_t RGB_LED_DATA_PIN = 20;

#endif /* Pins_Arduino_h */
