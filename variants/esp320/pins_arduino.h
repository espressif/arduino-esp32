#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t LED_BUILTIN = 5;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SDA = 2;
static const uint8_t SCL = 14;

static const uint8_t SS    = 15;
static const uint8_t MOSI  = 13;
static const uint8_t MISO  = 12;
static const uint8_t SCK   = 14;

#endif /* Pins_Arduino_h */
