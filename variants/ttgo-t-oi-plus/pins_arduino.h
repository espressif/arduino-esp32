#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 12
#define NUM_DIGITAL_PINS        12
#define NUM_ANALOG_INPUTS       3

#define analogInputToDigitalPin(p)  (((p)<NUM_ANALOG_INPUTS)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<NUM_DIGITAL_PINS)?(p):-1)
#define digitalPinHasPWM(p)         (p < EXTERNAL_NUM_INTERRUPTS)

static const uint8_t TX = 21;
static const uint8_t RX = 20;

static const uint8_t SDA = 19;
static const uint8_t SCL = 18;

static const uint8_t SS = 7;
static const uint8_t MOSI = 6;
static const uint8_t MISO = 5;
static const uint8_t SCK = 4;

static const uint8_t A1 = 2;
static const uint8_t A2 = 4;
static const uint8_t A3 = 5;

#endif /* Pins_Arduino_h */
