#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t LED_BUILTIN = 2;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t A0 = 14;
static const uint8_t A1 = 13;
static const uint8_t A2 = 12;
static const uint8_t A3 = 4;
static const uint8_t A4 = 2;
static const uint8_t A5 = 0;

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t TX2 = 17;
static const uint8_t RX2 = 16;

static const uint8_t SS = 4;
static const uint8_t RST = 14;
static const uint8_t DIO0 = 2;

#endif /* Pins_Arduino_h */