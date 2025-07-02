#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x303a
#define USB_PID 0x1001

// The pin assignments in this file are based on u-blox EVK-NORA-W1, a Arduino compatible board.
// For your own module design you can freely chose the pins available on the module module pins

static const uint8_t TX = 43;
static const uint8_t RX = 44;
static const uint8_t RTS = 45;
static const uint8_t CTS = 6;
static const uint8_t DTR = 1;
static const uint8_t DSR = 7;

static const uint8_t SW1 = 46;
static const uint8_t SW2 = 0;  // BOOT
static const uint8_t SW3 = 47;
static const uint8_t SW4 = 48;

static const uint8_t LED_RED = 5;
static const uint8_t LED_GREEN = 2;
static const uint8_t LED_BLUE = 8;
#define BUILTIN_LED LED_BLUE  // backward compatibility
#define LED_BUILTIN LED_BLUE

static const uint8_t SS = 34;
static const uint8_t MOSI = 35;
static const uint8_t MISO = 37;
static const uint8_t SCK = 36;

static const uint8_t A0 = 11;
static const uint8_t A1 = 12;
static const uint8_t A2 = 13;
static const uint8_t A3 = 5;
static const uint8_t A4 = 15;
static const uint8_t A5 = 16;

static const uint8_t D0 = 44;  // RX0
static const uint8_t D1 = 43;  // TX0
static const uint8_t D2 = 46;
static const uint8_t D3 = 4;
static const uint8_t D4 = 3;
static const uint8_t D5 = 2;
static const uint8_t D6 = 14;
static const uint8_t D7 = 10;

static const uint8_t D8 = 33;
static const uint8_t D9 = 38;
static const uint8_t D10 = 34;  // SS
static const uint8_t D11 = 35;  // MOSI
static const uint8_t D12 = 37;  // MISO
static const uint8_t D13 = 36;  // SCK
static const uint8_t SDA1 = 21;
static const uint8_t SCL1 = 0;

static const uint8_t D14 = 45;  // RTS
static const uint8_t D15 = 6;   // CTS
static const uint8_t D16 = 1;   // DTR
static const uint8_t D17 = 7;   // DSR
static const uint8_t D18 = 47;
static const uint8_t D19 = 48;
static const uint8_t SDA = 18;
static const uint8_t SCL = 17;

static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T4 = 4;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;
static const uint8_t T10 = 10;
static const uint8_t T11 = 11;
static const uint8_t T12 = 12;
static const uint8_t T13 = 13;
static const uint8_t T14 = 14;

#endif /* Pins_Arduino_h */
