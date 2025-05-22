#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x303A
#define USB_PID 0x1001

static const uint8_t LED_BUILTIN = 47;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 8;
static const uint8_t SCL = 9;

static const uint8_t SS = 10;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 13;
static const uint8_t SCK = 12;

// 2 connectors (X, Y) with 4 driver channels each
static const uint8_t X1 = 35;
static const uint8_t X2 = 36;
static const uint8_t X3 = 37;
static const uint8_t X4 = 38;
static const uint8_t Y1 = 4;
static const uint8_t Y2 = 5;
static const uint8_t Y3 = 6;
static const uint8_t Y4 = 7;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 8;
static const uint8_t A4 = 9;
static const uint8_t A5 = 10;
static const uint8_t A6 = 11;
static const uint8_t A7 = 12;
static const uint8_t A8 = 13;
static const uint8_t A9 = 14;
static const uint8_t A10 = 15;
static const uint8_t A11 = 16;
static const uint8_t A12 = 17;
static const uint8_t A13 = 18;

static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;
static const uint8_t T10 = 10;
static const uint8_t T11 = 11;
static const uint8_t T12 = 12;
static const uint8_t T13 = 13;
static const uint8_t T14 = 14;

#endif /* Pins_Arduino_h */
