#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        40
#define NUM_ANALOG_INPUTS       16

#define analogInputToDigitalPin(p)  (((p)<20)?(esp32_adc2gpio[(p)]):-1)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)

typedef unsigned char uint8_t;

static const uint8_t LED_BUILTIN = 2;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN



static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

static const uint8_t SS    = 5;
static const uint8_t MOSI  = 23;
static const uint8_t MISO  = 19;
static const uint8_t SCK   = 18;

static const uint8_t D0 = 3;
static const uint8_t D1 = 1;
static const uint8_t D2 = 25;
static const uint8_t D3 = 26;
static const uint8_t D4 = 27;
static const uint8_t D5 = 9;
static const uint8_t D6 = 10;
static const uint8_t D7 = 13;
static const uint8_t D8 = 5;
static const uint8_t D9 = 2;
static const uint8_t D10 = 0;

static const uint8_t A0 = 36;
static const uint8_t A1 = 39;
static const uint8_t A2 = 34;
static const uint8_t A3 = 35;
static const uint8_t A4 = 15;
static const uint8_t A5 = 35;
static const uint8_t A6 = 4;
static const uint8_t A7 = 0;
static const uint8_t A8 = 2;
static const uint8_t A9 = 13;
static const uint8_t A10 = 12;
static const uint8_t A11 = 14;
static const uint8_t A12 = 27;
static const uint8_t A13 = 25;
static const uint8_t A14 = 26;

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
