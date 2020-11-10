#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define EXTERNAL_NUM_INTERRUPTS 16
#define NUM_DIGITAL_PINS        40
#define NUM_ANALOG_INPUTS       16

#define analogInputToDigitalPin(p)  (((p)<20)?(esp32_adc2gpio[(p)]):-1)
#define digitalPinToInterrupt(p)    (((p)<40)?(p):-1)
#define digitalPinHasPWM(p)         (p < 34)

#define ALKSESP32 // tell library to not map pins again

static const uint8_t LED_BUILTIN = 23;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN

static const uint8_t TX = 1;
static const uint8_t RX = 3;

static const uint8_t D0 = 40;
static const uint8_t D1 = 41;
static const uint8_t D2 = 15;
static const uint8_t D3 = 2;
static const uint8_t D4 = 0;
static const uint8_t D5 = 4;
static const uint8_t D6 = 16;
static const uint8_t D7 = 17;
static const uint8_t D8 = 5;
static const uint8_t D9 = 18;
static const uint8_t D10 = 19;
static const uint8_t D11 = 21;
static const uint8_t D12 = 22;
static const uint8_t D13 = 23;

static const uint8_t A0 = 32;
static const uint8_t A1 = 33;
static const uint8_t A2 = 25;
static const uint8_t A3 = 26;
static const uint8_t A4 = 27;
static const uint8_t A5 = 14;
static const uint8_t A6 = 12;
static const uint8_t A7 = 15;

static const uint8_t L_R = 22;
static const uint8_t L_G = 17;
static const uint8_t L_Y = 23;
static const uint8_t L_B = 5;
static const uint8_t L_RGB_R = 4;
static const uint8_t L_RGB_G = 21;
static const uint8_t L_RGB_B = 16;

static const uint8_t SW1 = 15;
static const uint8_t SW2 = 2;
static const uint8_t SW3 = 0;

static const uint8_t POT1 = 32;
static const uint8_t POT2 = 33;

static const uint8_t PIEZO1 = 19;
static const uint8_t PIEZO2 = 18;

static const uint8_t PHOTO = 25;

static const uint8_t DHT_PIN = 26;

static const uint8_t S1 = 4;
static const uint8_t S2 = 16;
static const uint8_t S3 = 18;
static const uint8_t S4 = 19;
static const uint8_t S5 = 21;

static const uint8_t SDA = 27;
static const uint8_t SCL = 14;

static const uint8_t SS    = 19;
static const uint8_t MOSI  = 21;
static const uint8_t MISO  = 22;
static const uint8_t SCK   = 23;

static const uint8_t DAC1 = 25;
static const uint8_t DAC2 = 26;

#endif /* Pins_Arduino_h */
