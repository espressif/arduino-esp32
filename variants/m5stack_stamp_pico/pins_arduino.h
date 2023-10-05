#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        40
#define NUM_ANALOG_INPUTS       16

#define analogInputToDigitalPin(p)  (((p)<20)?(esp32_adc2gpio[(p)]):-1)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)


static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

static const uint8_t G26 = 26;
static const uint8_t G36 = 36;
static const uint8_t G18 = 18;
static const uint8_t G19 = 19;
static const uint8_t G21 = 21;
static const uint8_t G22 = 22;
static const uint8_t G25 = 25;
static const uint8_t G1 = 1;
static const uint8_t G3 = 3;
static const uint8_t G0 = 0;

static const uint8_t G32 = 32;
static const uint8_t G33 = 33;

static const uint8_t SS    = 19;
static const uint8_t MOSI  = 26;
static const uint8_t MISO  = 36;
static const uint8_t SCK   = 18;

#endif /* Pins_Arduino_h */