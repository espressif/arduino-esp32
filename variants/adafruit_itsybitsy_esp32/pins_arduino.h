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

// Neopixel
static const uint8_t PIN_NEOPIXEL = 0;
static const uint8_t NEOPIXEL_POWER = 2;

static const uint8_t TX = 20;
static const uint8_t RX = 8;

#define TX1 TX
#define RX1 RX

static const uint8_t SDA = 15;
static const uint8_t SCL = 27;

static const uint8_t SS    = 32;
static const uint8_t MOSI  = 21;
static const uint8_t MISO  = 22;
static const uint8_t SCK   = 19;

static const uint8_t A0 = 25; 
static const uint8_t A1 = 26;
static const uint8_t A2 = 4;
static const uint8_t A3 = 38;
static const uint8_t A4 = 37;
static const uint8_t A5 = 36;

// internal switch
static const uint8_t BUTTON = 35;

static const uint8_t T0 = 4;
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
