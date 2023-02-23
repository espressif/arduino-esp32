#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define Wireless_Stick_Lite true
#define DISPLAY_HEIGHT 0
#define DISPLAY_WIDTH  0

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        40
#define NUM_ANALOG_INPUTS       15

#define analogInputToDigitalPin(p)  (((p)<20)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)

static const uint8_t LED_BUILTIN = 35;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN

static const uint8_t TX = 44;
static const uint8_t RX = 43;

static const uint8_t SDA = 2;
static const uint8_t SCL = 3;

static const uint8_t SS    = 34;
static const uint8_t MOSI  = 35;
static const uint8_t SCK   = 36;
static const uint8_t MISO  = 37;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A6 = 7;
static const uint8_t A10 = 12;
static const uint8_t A12 = 14;
static const uint8_t A13 = 15;
static const uint8_t A14 = 16;
static const uint8_t A15 = 17;
static const uint8_t A16 = 18;
static const uint8_t A17 = 19;
static const uint8_t A18 = 20;

static const uint8_t T0 = 1;
static const uint8_t T1 = 2;
static const uint8_t T2 = 3;
static const uint8_t T3 = 4;
static const uint8_t T4 = 5;
static const uint8_t T5 = 6;
static const uint8_t T6 = 7;

static const uint8_t Vext = 36;
static const uint8_t LED  = 35;

#endif /* Pins_Arduino_h */
