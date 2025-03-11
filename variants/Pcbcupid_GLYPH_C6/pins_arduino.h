#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = 14;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

//MSR Used in on-board battery measurement
static const uint8_t BAT_MEASURE = 0;
#define MSR BAT_MEASURE

static const uint8_t TX = 16;
static const uint8_t RX = 17;

static const uint8_t SDA = 4;
static const uint8_t SCL = 5;

static const uint8_t SS = 20;
static const uint8_t MOSI = 22;
static const uint8_t MISO = 23;
static const uint8_t SCK = 21;

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
static const uint8_t D14 = 14;
static const uint8_t D15 = 15;
static const uint8_t D16 = 16;
static const uint8_t D17 = 17;
static const uint8_t D18 = 18;
static const uint8_t D19 = 19;
static const uint8_t D20 = 20;
static const uint8_t D21 = 21;
static const uint8_t D22 = 22;
static const uint8_t D23 = 23;

#endif /* Pins_Arduino_h */
