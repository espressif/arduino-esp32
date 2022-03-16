#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 22
#define NUM_DIGITAL_PINS        22
#define NUM_ANALOG_INPUTS       6

#define analogInputToDigitalPin(p)  (((p)<NUM_ANALOG_INPUTS)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<NUM_DIGITAL_PINS)?(p):-1)
#define digitalPinHasPWM(p)         (p < EXTERNAL_NUM_INTERRUPTS)

#define BUTTON 9
#define PIN_NEOPIXEL 2

static const uint8_t TX = 21;
static const uint8_t RX = 20;

static const uint8_t SDA = 5;
static const uint8_t SCL = 6;

static const uint8_t SS    = 6;
static const uint8_t MOSI  = 7;
static const uint8_t MISO  = 8;
static const uint8_t SCK   = 10;

static const uint8_t A0 = 4;
static const uint8_t A1 = 3;
static const uint8_t A2 = 1;
static const uint8_t A3 = 0;
static const uint8_t A4 = 5;

#endif /* Pins_Arduino_h */
