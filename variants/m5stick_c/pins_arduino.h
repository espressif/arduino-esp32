#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        40
#define NUM_ANALOG_INPUTS       16

#define analogInputToDigitalPin(p)  (((p)<20)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t TXD2 = 33;
static const uint8_t RXD2 = 32;

static const uint8_t SDA = 32;
static const uint8_t SCL = 33;

static const uint8_t SS    = 5;
static const uint8_t MOSI  = 15;
static const uint8_t MISO  = 36;
static const uint8_t SCK   = 13;

static const uint8_t G9 = 9;
static const uint8_t G10 = 10;
static const uint8_t G37 = 37;
static const uint8_t G39 = 39;
static const uint8_t G32 = 32;
static const uint8_t G33 = 33;
static const uint8_t G26 = 26;
static const uint8_t G36 = 36;
static const uint8_t G0 = 0;

static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

static const uint8_t ADC1 = 35;
static const uint8_t ADC2 = 36;

#endif /* Pins_Arduino_h */
