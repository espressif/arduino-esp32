#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = 0;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

//MSR Used in on-board battery measurement
static const uint8_t BAT_MEASURE = 1;
#define MSR BAT_MEASURE

static const uint8_t TX = 24;
static const uint8_t RX = 23;

static const uint8_t SDA = 4;
static const uint8_t SCL = 5;

static const uint8_t SS = 3;
static const uint8_t MOSI = 22;
static const uint8_t MISO = 25;
static const uint8_t SCK = 11;

static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;

static const uint8_t D0 = 0;
static const uint8_t D1 = 1;
static const uint8_t D2 = 2;
static const uint8_t D3 = 3;
static const uint8_t D4 = 4;
static const uint8_t D5 = 5;
static const uint8_t D8 = 8;
static const uint8_t D9 = 9;
static const uint8_t D10 = 10;
static const uint8_t D11 = 11;
static const uint8_t D12 = 12;
static const uint8_t D13 = 13;
static const uint8_t D14 = 14;

#endif /* Pins_Arduino_h */
