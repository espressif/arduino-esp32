#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = 1;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

//MSR Used in on-board battery measurement
static const uint8_t BAT_MEASURE = 0;
#define MSR BAT_MEASURE

static const uint8_t TX = 21;
static const uint8_t RX = 20;

static const uint8_t SDA = 4;
static const uint8_t SCL = 5;

static const uint8_t SS = 3;
static const uint8_t MOSI = 6;
static const uint8_t MISO = 7;
static const uint8_t SCK = 10;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;

static const uint8_t D0 = 0;
static const uint8_t D1 = 1;
static const uint8_t D2 = 2;
static const uint8_t D3 = 3;
static const uint8_t D4 = 4;
static const uint8_t D5 = 5;
static const uint8_t D6 = 6;
static const uint8_t D7 = 7;
static const uint8_t D8 = 8;
static const uint8_t D9 = 9;
static const uint8_t D10 = 10;

#endif /* Pins_Arduino_h */
