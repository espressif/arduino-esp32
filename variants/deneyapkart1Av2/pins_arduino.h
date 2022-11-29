#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define EXTERNAL_NUM_INTERRUPTS	46
#define NUM_DIGITAL_PINS	48
#define NUM_ANALOG_INPUTS	20

static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT+48;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN
#define RGB_BUILTIN LED_BUILTIN
#define RGBLED	LED_BUILTIN
#define RGB_BRIGHTNESS 64

#define analogInputToDigitalPin(p)  (((p)<20)?(esp32_adc2gpio[(p)]):-1)
#define digitalPinToInterrupt(p)    (((p)<48)?(p):-1)
#define digitalPinHasPWM(p)         (p < 46)

static const uint8_t GPKEY  = 0;
#define KEY_BUILTIN GPKEY
#define BUILTIN_KEY GPKEY

static const uint8_t TX 	= 43;
static const uint8_t RX 	= 44;

static const uint8_t SDA 	= 47;
static const uint8_t SCL 	= 21;

static const uint8_t SS		= 42;
static const uint8_t MOSI	= 39;
static const uint8_t MISO	= 40;
static const uint8_t SCK	= 41;

static const uint8_t A0 	= 4;
static const uint8_t A1 	= 5;
static const uint8_t A2 	= 6;
static const uint8_t A3 	= 7;
static const uint8_t A4 	= 15;
static const uint8_t A5 	= 16;
static const uint8_t A6 	= 17;
static const uint8_t A7 	= 18;

static const uint8_t D0 	= 1;
static const uint8_t D1 	= 2;
static const uint8_t D2 	= 43;
static const uint8_t D3 	= 44;
static const uint8_t D4 	= 42;
static const uint8_t D5 	= 41;
static const uint8_t D6 	= 40;
static const uint8_t D7 	= 39;
static const uint8_t D8 	= 38;
static const uint8_t D9 	= 48;
static const uint8_t D10 	= 47;
static const uint8_t D11 	= 21;
static const uint8_t D12 	= 0;
static const uint8_t D13 	= 10;
static const uint8_t D14 	= 3;
static const uint8_t D15 	= 8;

#endif /* Pins_Arduino_h */