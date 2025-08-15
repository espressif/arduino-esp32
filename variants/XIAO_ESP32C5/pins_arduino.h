#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x2886
#define USB_PID 0x0067

static const uint8_t LED_BUILTIN = 27;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t TX = 11;
static const uint8_t RX = 12;

static const uint8_t SDA = 23;
static const uint8_t SCL = 24;

static const uint8_t SS = 7;
static const uint8_t MOSI = 10;
static const uint8_t MISO = 9;
static const uint8_t SCK = 8;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;

static const uint8_t D0 = 1;
static const uint8_t D1 = 0;
static const uint8_t D2 = 25;
static const uint8_t D3 = 7;
static const uint8_t D4 = 23;
static const uint8_t D5 = 24;
static const uint8_t D6 = 11;
static const uint8_t D7 = 12;
static const uint8_t D8 = 8;
static const uint8_t D9 = 9;
static const uint8_t D10 = 10;

#endif /* Pins_Arduino_h */
