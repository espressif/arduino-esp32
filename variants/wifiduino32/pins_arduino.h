#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

static const uint8_t LED_BUILTIN = 2;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t KEY_BUILTIN = 0;

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SDA = 5;
static const uint8_t SCL = 16;

static const uint8_t SS = 5;
static const uint8_t MOSI = 23;
static const uint8_t MISO = 19;
static const uint8_t SCK = 18;

static const uint8_t A0 = 27;
static const uint8_t A1 = 14;
static const uint8_t A2 = 12;
static const uint8_t A3 = 35;
static const uint8_t A4 = 13;
static const uint8_t A5 = 4;


static const uint8_t D0 = 3;
static const uint8_t D1 = 1;
static const uint8_t D2 = 17;
static const uint8_t D3 = 15;
static const uint8_t D4 = 32;
static const uint8_t D5 = 33;
static const uint8_t D6 = 25;
static const uint8_t D7 = 26;
static const uint8_t D8 = 23;
static const uint8_t D9 = 22;
static const uint8_t D10 = 21;
static const uint8_t D11 = 19;
static const uint8_t D12 = 18;
static const uint8_t D13 = 2;

static const uint8_t T0 = 4;
static const uint8_t T1 = 0;
static const uint8_t T2 = 2;
static const uint8_t T3 = 15;
static const uint8_t T4 = 13;
static const uint8_t T5 = 12;
static const uint8_t T6 = 14;
static const uint8_t T7 = 27;
static const uint8_t T8 = 33;
static const uint8_t T9 = 32;

static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

#endif /* Pins_Arduino_h */
