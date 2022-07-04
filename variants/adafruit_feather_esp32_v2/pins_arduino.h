#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        40
#define NUM_ANALOG_INPUTS       16

#define analogInputToDigitalPin(p)  (((p)<20)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)

static const uint8_t LED_BUILTIN = 13;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN

static const uint8_t TX = 8;
static const uint8_t RX = 7;
#define TX1 TX
#define RX1 RX

static const uint8_t SDA = 22;
static const uint8_t SCL = 20;

static const uint8_t SS    = 33;
static const uint8_t MOSI  = 19;
static const uint8_t MISO  = 21;
static const uint8_t SCK   = 5;

// mapping to match other feathers and also in order
static const uint8_t A0 = 26; 
static const uint8_t A1 = 25;
static const uint8_t A2 = 34;
static const uint8_t A3 = 39;
static const uint8_t A4 = 36;
static const uint8_t A5 = 4;
static const uint8_t A6 = 14;
static const uint8_t A7 = 32;
static const uint8_t A8 = 15;
static const uint8_t A9 = 33;
static const uint8_t A10 = 27;
static const uint8_t A11 = 12;
static const uint8_t A12 = 13;
static const uint8_t A13 = 35;

// vbat measure
#define BATT_MONITOR 35

// internal switch
#define BUTTON 38

// Neopixel
#define PIN_NEOPIXEL 0

// Neopixel & I2C power
#define NEOPIXEL_I2C_POWER 2

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
