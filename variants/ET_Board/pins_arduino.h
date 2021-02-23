#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        40
#define NUM_ANALOG_INPUTS       7

#define analogInputToDigitalPin(p)  (((p)<20)?(esp32_adc2gpio[(p)]):-1)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)

static const uint8_t TX = 34;
static const uint8_t RX = 35;

static const uint8_t SDA = 33;
static const uint8_t SCL = 36;

static const uint8_t SS    = 29;
static const uint8_t MOSI  = 37;
static const uint8_t MISO  = 31;
static const uint8_t SCK   = 30;

static const uint8_t A0 = 36;   // BUILTIN_Potentiometer
static const uint8_t A1 = 39;   // BUILTIN_CDS
static const uint8_t A2 = 32;   // BUILTIN_temperature
static const uint8_t A3 = 33;   // Analog Input
static const uint8_t A4 = 34;   // Analog Input
static const uint8_t A5 = 35;   // Analog Input
static const uint8_t A6 = 25;   // Analog Input
static const uint8_t A7 = 26;   // Analog Input


static const uint8_t D2 = 27;   // BUILTIN_LED_Red
static const uint8_t D3 = 14;   // BUILTIN_LED_Blue
static const uint8_t D4 = 12;   // BUILTIN_LED_Green
static const uint8_t D5 = 13;   // BUILTIN_LED_Yellow
static const uint8_t D6 = 15;   // BUILTIN_BUTTON_Red
static const uint8_t D7 = 16;   // BUILTIN_BUTTON_Blue
static const uint8_t D8 = 17;   // BUILTIN_BUTTON_Green
static const uint8_t D9 = 4;    // BUILTIN_BUTTON_Yellow

static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

#endif /* Pins_Arduino_h */