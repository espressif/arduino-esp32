#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define EXTERNAL_NUM_INTERRUPTS 22
#define NUM_DIGITAL_PINS        22
#define NUM_ANALOG_INPUTS       6

static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT+13;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN
#define BOARD_HAS_NEOPIXEL
#define LED_BRIGHTNESS 64

#define analogInputToDigitalPin(p)  (((p)<NUM_ANALOG_INPUTS)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<NUM_DIGITAL_PINS)?(p):-1)
#define digitalPinHasPWM(p)         (p < EXTERNAL_NUM_INTERRUPTS)

static const uint8_t TX = 21;
static const uint8_t RX = 20;

static const uint8_t SDA = 4;
static const uint8_t SCL = 5;

static const uint8_t SS    = 7;
static const uint8_t MOSI  = 3;
static const uint8_t MISO  = 10;
static const uint8_t SCK   = 2;

static const uint8_t A0 = 0;
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;
static const uint8_t A4 = 4;
static const uint8_t A5 = 5;

static const uint8_t D0 = 20;
static const uint8_t D1 = 21;
static const uint8_t D2 = 9;
static const uint8_t D3 = 13;
static const uint8_t D4 = 12;
static const uint8_t D5 = 11;
static const uint8_t D6 = 6;
static const uint8_t D7 = 18;
static const uint8_t D8 = 19;
static const uint8_t D9 = 8;
static const uint8_t D10 = 7;
static const uint8_t D11 = 3;
static const uint8_t D12 = 10;
static const uint8_t D13 = 2;

#endif /* Pins_Arduino_h */
