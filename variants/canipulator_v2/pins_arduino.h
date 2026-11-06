#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define PIN_RGB_LED 2
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_RGB_LED;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API rgbLedWrite()
#define RGB_BUILTIN    LED_BUILTIN
#define RGB_BRIGHTNESS 64

static const uint8_t CAN0_RX = 4;
static const uint8_t CAN0_TX = 5;
static const uint8_t CAN1_RX = 6;
static const uint8_t CAN1_TX = 7;

// The following 4 pins are only available via the I2C GPIO expander (address 0x41)
static const uint8_t EXP_CAN0_RS = 0;
static const uint8_t EXP_CAN0_STBY = 1;
static const uint8_t EXP_CAN1_RS = 2;
static const uint8_t EXP_CAN1_STBY = 3;

static const uint8_t USER_BTN = 28;

static const uint8_t TX = 12;
static const uint8_t RX = 11;

static const uint8_t SDA = 0;
static const uint8_t SCL = 1;

static const uint8_t SS = 27;
static const uint8_t MOSI = 23;
static const uint8_t MISO = 25;
static const uint8_t SCK = 24;
static const uint8_t SD_DET = 26;  // uSD insertion detection, low = inserted

static const uint8_t A0 = 3;

#endif /* Pins_Arduino_h */
