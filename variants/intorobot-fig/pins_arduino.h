#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        7
#define NUM_ANALOG_INPUTS       10

#define analogInputToDigitalPin(p)  (((p)<20)?(esp32_adc2gpio[(p)]):-1)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)

static const uint8_t LED_BUILTIN = 4;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN

static const uint8_t RGB_R_BUILTIN = 27;
static const uint8_t RGB_G_BUILTIN = 21;
static const uint8_t RGB_B_BUILTIN = 22;

static const uint8_t KEY_BUILTIN = 0;

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t SDA = 23;
static const uint8_t SCL = 19;

static const uint8_t SS    = 5;
static const uint8_t MOSI  = 16;
static const uint8_t MISO  = 17;
static const uint8_t SCK   = 18;

static const uint8_t A0 = 36;
static const uint8_t A1 = 39;
static const uint8_t A2 = 35;
static const uint8_t A3 = 25;
static const uint8_t A4 = 26;
static const uint8_t A5 = 14;
static const uint8_t A6 = 12;
static const uint8_t A7 = 15;
static const uint8_t A8 = 13;
static const uint8_t A9 = 2;

static const uint8_t D0 = 19;
static const uint8_t D1 = 23;
static const uint8_t D2 = 18;
static const uint8_t D3 = 17;
static const uint8_t D4 = 16;
static const uint8_t D5 = 5;
static const uint8_t D6 = 4;

static const uint8_t T0 = 19;
static const uint8_t T1 = 23;
static const uint8_t T2 = 18;
static const uint8_t T3 = 17;
static const uint8_t T4 = 16;
static const uint8_t T5 = 5;
static const uint8_t T6 = 4;

static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

#endif /* Pins_Arduino_h */
