#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID            0x303A
#define USB_PID            0x8141
#define USB_MANUFACTURER   "Turkish Technnology Team Foundation (T3)"
#define USB_PRODUCT        "DENEYAP MINI"
#define USB_SERIAL         "" // Empty string for MAC adddress

static const uint8_t LED_BUILTIN = 35;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
#define LEDB        LED_BUILTIN
#define LEDR 34
#define LEDG 33

static const uint8_t GPKEY = 0;
#define KEY_BUILTIN GPKEY
#define BUILTIN_KEY GPKEY
#define BT GPKEY

static const uint8_t TX = 43;
static const uint8_t RX = 44;
#define TX1 TX
#define RX1 RX

static const uint8_t SDA = 36;
static const uint8_t SCL = 37;

static const uint8_t SS   = 21;
static const uint8_t MOSI = 40;
static const uint8_t MISO = 39;
static const uint8_t SCK  = 38;

static const uint8_t A0 = 8;
static const uint8_t A1 = 9;
static const uint8_t A2 = 10;
static const uint8_t A3 = 11;
static const uint8_t A4 = 12;
static const uint8_t A5 = 13;
static const uint8_t A6 = 16;

static const uint8_t T0 = 8;
static const uint8_t T1 = 9;
static const uint8_t T2 = 10;
static const uint8_t T3 = 11;
static const uint8_t T4 = 12;
static const uint8_t T5 = 13;

static const uint8_t D0  = 44;
static const uint8_t D1  = 43;
static const uint8_t D2  = 42;
static const uint8_t D3  = 41;
static const uint8_t D4  = 40;
static const uint8_t D5  = 39;
static const uint8_t D6  = 38;
static const uint8_t D7  = 37;
static const uint8_t D8  = 36;
static const uint8_t D9  = 26;
static const uint8_t D10 = 21;
static const uint8_t D11 = 18;
static const uint8_t D12 = 17;
static const uint8_t D13 = 0;
static const uint8_t D14 = 35;
static const uint8_t D15 = 33;
static const uint8_t D16 = 34;

static const uint8_t PWM0 = 42;
static const uint8_t PWM1 = 41;

static const uint8_t DAC0 = 17;
static const uint8_t DAC1 = 18;

/*
#define SD SDA
#define SC SCL

#define MO MOSI
#define MI MISO
#define MC SCK

#define DA0 DAC0
#define DA1 DAC1
*/

#endif /* Pins_Arduino_h */
