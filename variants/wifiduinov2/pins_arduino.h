#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define NUM_DIGITAL_PINS        SOC_GPIO_PIN_COUNT    // GPIO 0..21 - not all are available
#define NUM_ANALOG_INPUTS       6                     // GPIO 0..5
#define EXTERNAL_NUM_INTERRUPTS NUM_DIGITAL_PINS      // All GPIOs

#define analogInputToDigitalPin(p)  (((p)<NUM_ANALOG_INPUTS)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<NUM_DIGITAL_PINS)?(p):NOT_AN_INTERRUPT)
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
