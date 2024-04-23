#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

static const uint8_t LED_BUILTIN = 15;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t TX = 16;
static const uint8_t RX = 17;

static const uint8_t SDA = 19;
static const uint8_t SCL = 20;

static const uint8_t SS = 4;
static const uint8_t MOSI = 22;
static const uint8_t MISO = 21;
static const uint8_t SCK = 23;

#endif /* Pins_Arduino_h */
