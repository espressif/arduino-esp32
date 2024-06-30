#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x16D0
#define USB_PID 0x1178

#define LED_BUILTIN 13
#define BUILTIN_LED LED_BUILTIN  // backward compatibility

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 3;
static const uint8_t SCL = 4;

static const uint8_t SS = 13;
static const uint8_t MOSI = 40;
static const uint8_t MISO = 41;
static const uint8_t SCK = 39;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 8;
static const uint8_t A3 = 9;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A6 = 14;
static const uint8_t A7 = 7;
static const uint8_t A8 = 15;
static const uint8_t A9 = 33;
static const uint8_t A10 = 27;
static const uint8_t A11 = 12;
static const uint8_t A12 = 13;
static const uint8_t A13 = 14;
static const uint8_t A14 = 15;
static const uint8_t A15 = 16;
static const uint8_t A16 = 17;
static const uint8_t A17 = 18;
static const uint8_t A18 = 19;
static const uint8_t A19 = 20;

static const uint8_t T1 = 2;
static const uint8_t T2 = 8;
static const uint8_t T3 = 9;
static const uint8_t T4 = 5;
static const uint8_t T5 = 6;
static const uint8_t T6 = 14;
static const uint8_t T7 = 7;
static const uint8_t T8 = 15;
static const uint8_t T9 = 33;
static const uint8_t T10 = 27;
static const uint8_t T11 = 12;
static const uint8_t T12 = 13;
static const uint8_t T13 = 14;
static const uint8_t T14 = 15;

static const uint8_t D5 = 14;
static const uint8_t D6 = 7;
static const uint8_t D9 = 15;
static const uint8_t D10 = 16;
static const uint8_t D11 = 17;
static const uint8_t D12 = 12;
static const uint8_t D13 = 13;
static const uint8_t D21 = 47;
#define UNPHONE_D_PINS_MAPPED

#endif /* Pins_Arduino_h */
