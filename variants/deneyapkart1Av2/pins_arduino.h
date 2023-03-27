#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID            0x303A
#define USB_PID            0x8147
#define USB_MANUFACTURER   "Turkish Technnology Team Foundation (T3)"
#define USB_PRODUCT        "DENEYAP KART 1A v2"
#define USB_SERIAL         "" // Empty string for MAC adddress

#define EXTERNAL_NUM_INTERRUPTS	46
#define NUM_DIGITAL_PINS	48
#define NUM_ANALOG_INPUTS	20

#define analogInputToDigitalPin(p)  (((p)<20)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<48)?(p):-1)
#define digitalPinHasPWM(p)         (p < 46)

static const uint8_t LED_BUILTIN = 48;
#define BUILTIN_LED LED_BUILTIN
#define LED_BUILTIN LED_BUILTIN
#define RGB_BUILTIN LED_BUILTIN
#define RGBLED      LED_BUILTIN
#define RGB_BRIGHTNESS 64

static const uint8_t GPKEY = 0;
#define KEY_BUILTIN GPKEY
#define BUILTIN_KEY GPKEY

static const uint8_t TX = 43;
static const uint8_t RX = 44;
#define TX1 TX
#define RX1 RX

static const uint8_t SDA = 47;
static const uint8_t SCL = 21;

static const uint8_t SS		= 42;
static const uint8_t MOSI	= 39;
static const uint8_t MISO	= 40;
static const uint8_t SCK	= 41;

static const uint8_t A0 = 4;
static const uint8_t A1 = 5;
static const uint8_t A2 = 6;
static const uint8_t A3 = 7;
static const uint8_t A4 = 15;
static const uint8_t A5 = 16;
static const uint8_t A6 = 17;
static const uint8_t A7 = 18;
static const uint8_t A8 = 9;

static const uint8_t T0 = 4;
static const uint8_t T1 = 5;
static const uint8_t T2 = 6;
static const uint8_t T3 = 7;
static const uint8_t T4 = 8;
static const uint8_t T5 = 3;
static const uint8_t T6 = 10;
static const uint8_t T7 = 1;
static const uint8_t T8 = 2;

static const uint8_t D0  = 1;
static const uint8_t D1  = 2;
static const uint8_t D2  = 43;
static const uint8_t D3  = 44;
static const uint8_t D4  = 42;
static const uint8_t D5  = 41;
static const uint8_t D6  = 40;
static const uint8_t D7  = 39;
static const uint8_t D8  = 38;
static const uint8_t D9  = 48;
static const uint8_t D10 = 47;
static const uint8_t D11 = 21;
static const uint8_t D12 = 10;
static const uint8_t D13 = 3;
static const uint8_t D14 = 8;
static const uint8_t D15 = 0;
static const uint8_t D16 = 13;
static const uint8_t D17 = 12;
static const uint8_t D18 = 11;
static const uint8_t D19 = 14;

static const uint8_t PWM0 = 15;
static const uint8_t PWM1 = 16;
static const uint8_t PWM2 = 17;
static const uint8_t PWM3 = 18;
static const uint8_t PWM4 = 38;

static const uint8_t CAMSD = 4;
static const uint8_t CAMSC = 5;
static const uint8_t CAMD2 = 41;
static const uint8_t CAMD3 = 2;
static const uint8_t CAMD4 = 1;
static const uint8_t CAMD5 = 42;
static const uint8_t CAMD6 = 40;
static const uint8_t CAMD7 = 38;
static const uint8_t CAMD8 = 17;
static const uint8_t CAMD9 = 15;
static const uint8_t CAMPC = 39;
static const uint8_t CAMXC = 16;
static const uint8_t CAMH  = 7;
static const uint8_t CAMV  = 6;

static const uint8_t SDMI = 14;
static const uint8_t SDMO = 12;
static const uint8_t SDCS = 11;
static const uint8_t SDCK = 13;

static const uint8_t BAT = 9;

#endif /* Pins_Arduino_h */
