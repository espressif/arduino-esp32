#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define EXTERNAL_NUM_INTERRUPTS 24
#define NUM_DIGITAL_PINS        24
#define NUM_ANALOG_INPUTS       7

static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT+8;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN
#define RGB_BUILTIN LED_BUILTIN
#define RGB_BRIGHTNESS 64

#define analogInputToDigitalPin(p)  (((p)<NUM_ANALOG_INPUTS)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<NUM_DIGITAL_PINS)?(p):-1)
#define digitalPinHasPWM(p)         (p < EXTERNAL_NUM_INTERRUPTS)

static const uint8_t TX = 16;
static const uint8_t RX = 17;

static const uint8_t SDA = 23;
static const uint8_t SCL = 22;

static const uint8_t SS    = 18;
static const uint8_t MOSI  = 19;
static const uint8_t MISO  = 20;
static const uint8_t SCK   = 21;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;
static const uint8_t A4 = 4;
static const uint8_t A5 = 5;
static const uint8_t A6 = 6;

#endif /* Pins_Arduino_h */
