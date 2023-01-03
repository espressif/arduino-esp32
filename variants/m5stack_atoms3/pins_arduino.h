#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 46
#define NUM_DIGITAL_PINS        48
#define NUM_ANALOG_INPUTS       18

#define analogInputToDigitalPin(p) \
    (((p) < 20) ? (analogChannelToDigitalPin(p)) : -1)
#define digitalPinToInterrupt(p) (((p) < 48) ? (p) : -1)
#define digitalPinHasPWM(p)      (p < 46)

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t TXD2 = 1;
static const uint8_t RXD2 = 2;

static const uint8_t SDA = 38;
static const uint8_t SCL = 39;

static const uint8_t SS   = 15;
static const uint8_t MOSI = 21;
static const uint8_t MISO = -1;
static const uint8_t SCK  = 17;

static const uint8_t G0  = 0;
static const uint8_t G1  = 1;
static const uint8_t G2  = 2;
static const uint8_t G3  = 3;
static const uint8_t G4  = 4;
static const uint8_t G5  = 5;
static const uint8_t G6  = 6;
static const uint8_t G7  = 7;
static const uint8_t G8  = 8;
static const uint8_t G36 = 36;
static const uint8_t G37 = 37;
static const uint8_t G38 = 38;
static const uint8_t G39 = 39;
static const uint8_t G40 = 40;
static const uint8_t G42 = 42;

static const uint8_t ADC1 = 7;
static const uint8_t ADC2 = 8;

#endif /* Pins_Arduino_h */
