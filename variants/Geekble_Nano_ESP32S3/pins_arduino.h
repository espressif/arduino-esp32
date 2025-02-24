#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID          0x303a
#define USB_PID          0x82C5
#define USB_MANUFACTURER "Geekble"
#define USB_PRODUCT      "Geekble nano ESP32-S3"
#define USB_SERIAL       ""  // Empty string for MAC address

static const uint8_t D0 = 44;  // also RX
static const uint8_t D1 = 43;  // also TX
static const uint8_t D2 = 5;
static const uint8_t D3 = 6;  // also CTS
static const uint8_t D4 = 7;  // also DSR
static const uint8_t D5 = 8;
static const uint8_t D6 = 9;
static const uint8_t D7 = 10;
static const uint8_t D8 = 17;
static const uint8_t D9 = 18;
static const uint8_t D10 = 21;  // also SS
static const uint8_t D11 = 38;  // also MOSI
static const uint8_t D12 = 47;  // also MISO
static const uint8_t D13 = 48;  // also SCK, LED_BUILTIN

static const uint8_t A0 = 1;  // also DTR
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 11;  // also SDA
static const uint8_t A5 = 12;  // also SCL
static const uint8_t A6 = 13;
static const uint8_t A7 = 14;

// alternate pin functions

static const uint8_t LED_BUILTIN = D13;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t SW_BUILTIN = 0;

static const uint8_t TX = D1;
static const uint8_t RX = D0;
static const uint8_t RTS = 45;
static const uint8_t CTS = D3;
static const uint8_t DTR = A0;
static const uint8_t DSR = D4;

static const uint8_t SS = D10;
static const uint8_t MOSI = D11;
static const uint8_t MISO = D12;
static const uint8_t SCK = D13;

static const uint8_t SDA = A4;
static const uint8_t SCL = A5;

// Touch input capable pins on the header
static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T4 = 4;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;

#define PIN_I2S_SCK    D7
#define PIN_I2S_FS     D8
#define PIN_I2S_SD     D9
#define PIN_I2S_SD_OUT D9  // same as bidir
#define PIN_I2S_SD_IN  D10

#endif /* Pins_Arduino_h */
