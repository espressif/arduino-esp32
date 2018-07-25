#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        20
#define NUM_ANALOG_INPUTS       16

#define analogInputToDigitalPin(p)  (((p)<20)?(esp32_adc2gpio[(p)]):-1)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SDA = 21;
static const uint8_t SCL = 22;

static const uint8_t SS    = 5;
static const uint8_t MOSI  = 23;
static const uint8_t MISO  = 19;
static const uint8_t SCK   = 18;

static const uint8_t G23 = 23;
static const uint8_t G19 = 19;
static const uint8_t G18 = 18;
static const uint8_t G3 = 3;
static const uint8_t G16 = 16;
static const uint8_t G21 = 21;
static const uint8_t G2 = 2;
static const uint8_t G12 = 12;
static const uint8_t G15 = 15;
static const uint8_t G35 = 35;
static const uint8_t G36 = 36;
static const uint8_t G25 = 25;
static const uint8_t G26 = 26;
static const uint8_t G1 = 1;
static const uint8_t G17 = 17;
static const uint8_t G22 = 22;
static const uint8_t G5 = 5;
static const uint8_t G13 = 13;
static const uint8_t G0 = 0;
static const uint8_t G34 = 34;

static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

static const uint8_t ADC1 = 35;
static const uint8_t ADC2 = 36;

#endif /* Pins_Arduino_h */
