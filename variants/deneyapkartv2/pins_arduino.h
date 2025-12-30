#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID          0x303A
#define USB_PID          0x82EB
#define USB_MANUFACTURER "Turkish Technology Team Foundation (T3)"
#define USB_PRODUCT      "DENEYAP KART v2"
#define USB_SERIAL       ""  // Empty string for MAC address

static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + 46;
#define BUILTIN_LED    LED_BUILTIN  // backward compatibility
#define LED_BUILTIN    LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
#define RGB_BUILTIN    LED_BUILTIN
#define RGBLED         LED_BUILTIN
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

static const uint8_t SS = 42;
static const uint8_t MOSI = 39;
static const uint8_t MISO = 40;
static const uint8_t SCK = 41;

static const uint8_t A0 = 4;
static const uint8_t A1 = 5;
static const uint8_t A2 = 6;
static const uint8_t A3 = 7;
static const uint8_t A4 = 15;
static const uint8_t A5 = 16;
static const uint8_t A6 = 17;
static const uint8_t A7 = 18;
static const uint8_t A8 = 8;
static const uint8_t A9 = 9;
static const uint8_t A10 = 10;
static const uint8_t A11 = 11;
static const uint8_t A12 = 2;
static const uint8_t A13 = 1;
static const uint8_t A14 = 3;
static const uint8_t A15 = 12;
static const uint8_t A16 = 13;
static const uint8_t A17 = 14;

static const uint8_t T0 = 4;
static const uint8_t T1 = 5;
static const uint8_t T2 = 6;
static const uint8_t T3 = 7;
static const uint8_t T4 = 8;
static const uint8_t T5 = 9;
static const uint8_t T6 = 10;
static const uint8_t T7 = 11;
static const uint8_t T8 = 2;
static const uint8_t T9 = 1;
static const uint8_t T10 = 3;
static const uint8_t T11 = 12;
static const uint8_t T12 = 13;
static const uint8_t T13 = 14;

static const uint8_t D0 = 1;
static const uint8_t D1 = 2;
static const uint8_t D2 = 43;
static const uint8_t D3 = 44;
static const uint8_t D4 = 42;
static const uint8_t D5 = 41;
static const uint8_t D6 = 40;
static const uint8_t D7 = 39;
static const uint8_t D8 = 38;
static const uint8_t D9 = 48;
static const uint8_t D10 = 47;
static const uint8_t D11 = 21;
static const uint8_t D12 = 11;
static const uint8_t D13 = 10;
static const uint8_t D14 = 9;
static const uint8_t D15 = 8;
static const uint8_t D16 = 18;
static const uint8_t D17 = 17;
static const uint8_t D18 = 16;
static const uint8_t D19 = 15;
static const uint8_t D20 = 7;
static const uint8_t D21 = 6;
static const uint8_t D22 = 5;
static const uint8_t D23 = 4;
static const uint8_t D24 = 46;
static const uint8_t D25 = 0;
static const uint8_t D26 = 3;
static const uint8_t D27 = 12;
static const uint8_t D28 = 13;
static const uint8_t D29 = 14;

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
static const uint8_t CAMH = 7;
static const uint8_t CAMV = 6;

static const uint8_t SDCM = 12;
static const uint8_t SDCK = 13;
static const uint8_t SDDA = 14;

static const uint8_t BAT = 3;
#define BAT_VOLT_PIN BAT

#endif /* Pins_Arduino_h */
