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

static const uint8_t BUTTON = 0;
static const uint8_t LED_LINK = 13;
static const uint8_t RELAY_2 = 14;
static const uint8_t RELAY_1 = 27;
static const uint8_t SWITCH_2 = 33;
static const uint8_t SWITCH_1 = 32;

static const uint8_t CSE7761_TX = 25;
static const uint8_t CSE7761_RX = 26;

#endif /* Pins_Arduino_h */
