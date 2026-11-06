#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define PIN_RGB_LED 8
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_RGB_LED;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API rgbLedWrite()
#define RGB_BUILTIN    LED_BUILTIN
#define RGB_BRIGHTNESS 64

static const uint8_t CAN0_RX = 16;
static const uint8_t CAN0_TX = 17;
static const uint8_t CAN1_RX = 18;
static const uint8_t CAN1_TX = 19;

static const uint8_t CAN0_RS = 0;
static const uint8_t CAN0_STBY = 3;
static const uint8_t CAN1_RS = 1;
static const uint8_t CAN1_STBY = 2;

static const uint8_t USER_BTN = 9;

static const uint8_t TX = 20;
static const uint8_t RX = 21;

static const uint8_t SDA = 23;
static const uint8_t SCL = 22;

static const uint8_t SS = 15;
static const uint8_t MOSI = 4;
static const uint8_t MISO = 5;
static const uint8_t SCK = 10;

static const uint8_t A0 = 4;
static const uint8_t A1 = 5;
static const uint8_t A2 = 6;

#endif /* Pins_Arduino_h */
