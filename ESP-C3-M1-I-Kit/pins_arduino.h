#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 30
#define NUM_DIGITAL_PINS        30
#define NUM_ANALOG_INPUTS       6

#define analogInputToDigitalPin(p)  (((p)<NUM_ANALOG_INPUTS)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<NUM_DIGITAL_PINS)?(p):-1)
#define digitalPinHasPWM(p)         (p < EXTERNAL_NUM_INTERRUPTS)

static const uint8_t LED_BUILTIN = 18;
static const uint8_t LED_BUILTIN = 19;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility

static const uint8_t LED_GREEN = 4;
static const uint8_t LED_RED = 3;
static const uint8_t LED_BLUE = 5;

static const uint8_t TX = 18;
static const uint8_t RX = 19;

static const uint8_t SDA = 4;
static const uint8_t SCL = 5;

static const uint8_t SS    = 23;
static const uint8_t MOSI  = 21;
static const uint8_t MISO  = 22;
static const uint8_t SCK   = 28;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 4;
static const uint8_t A3 = 5;
static const uint8_t A4 = 6;
static const uint8_t A5 = 13;

#endif /* Pins_Arduino_h */
