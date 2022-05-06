#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 22
#define NUM_DIGITAL_PINS        22
#define NUM_ANALOG_INPUTS       6

#define analogInputToDigitalPin(p)  (((p)<NUM_ANALOG_INPUTS)?(esp32_adc2gpio[(p)]):-1)
#define digitalPinToInterrupt(p)    (((p)<NUM_DIGITAL_PINS)?(p):-1)
#define digitalPinHasPWM(p)         (p < EXTERNAL_NUM_INTERRUPTS)

static const uint8_t TX = 21;
static const uint8_t RX = 20;

static const uint8_t SDA = 6;
static const uint8_t SCL = 7;

static const uint8_t SS    = 20;
static const uint8_t MOSI  = 10;
static const uint8_t MISO  = 9;
static const uint8_t SCK   = 8;

static const uint8_t A0 = 2;
static const uint8_t A1 = 3;
static const uint8_t A2 = 4;
static const uint8_t A3 = 5;
static const uint8_t A4 = 6;
static const uint8_t A5 = 7;

#endif /* Pins_Arduino_h */
