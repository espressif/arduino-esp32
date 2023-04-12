#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x2341
#define USB_PID 0x0070

#define EXTERNAL_NUM_INTERRUPTS 46
#define NUM_DIGITAL_PINS        48
#define NUM_ANALOG_INPUTS       20

#define analogInputToDigitalPin(p)  (((p)<20)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<48)?(p):-1)
#define digitalPinHasPWM(p)         (p < 46)

static const uint8_t TX   = 43;
static const uint8_t RX   = 44;
static const uint8_t RTS  = 45;
static const uint8_t CTS  =  6;
static const uint8_t DTR  =  1;
static const uint8_t DSR  =  7;

static const uint8_t LED_RED    = 46;
static const uint8_t LED_GREEN  = 0;
static const uint8_t LED_BLUE   = 45;
static const uint8_t LED_BUILTIN = 48;
#define LED_BUILTIN  LED_BUILTIN

static const uint8_t SS   = 21;
static const uint8_t MOSI = 38;
static const uint8_t MISO = 47;
static const uint8_t SCK  = 48;

static const uint8_t A0   = 1;
static const uint8_t A1   = 2;
static const uint8_t A2   = 3;
static const uint8_t A3   = 4;
static const uint8_t A4   = 11;
static const uint8_t A5   = 12;
static const uint8_t A6   = 13;
static const uint8_t A7   = 14;

static const uint8_t D0   = 44; // RX0
static const uint8_t D1   = 43; // TX0
static const uint8_t D2   = 5;
static const uint8_t D3   = 6;
static const uint8_t D4   = 7;
static const uint8_t D5   = 8;
static const uint8_t D6   = 9;
static const uint8_t D7   = 10;

static const uint8_t D8   = 17;
static const uint8_t D9   = 18;
static const uint8_t D10  = 21; // SS
static const uint8_t D11  = 38; // MOSI
static const uint8_t D12  = 47; // MISO
static const uint8_t D13  = 48; // SCK
static const uint8_t SDA  = 11;
static const uint8_t SCL  = 12;

#endif /* Pins_Arduino_h */
