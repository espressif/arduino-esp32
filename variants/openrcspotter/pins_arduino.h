#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t LED_BUILTIN = 13;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t TX = 17;
static const uint8_t RX = 16;

#define TX1 TX
#define RX1 RX

static const uint8_t SDA = 27;
static const uint8_t SCL = 14;

static const uint8_t SS = 5;
static const uint8_t MOSI = 23;
static const uint8_t MISO = 19;
static const uint8_t SCK = 18;


#endif /* Pins_Arduino_h */
