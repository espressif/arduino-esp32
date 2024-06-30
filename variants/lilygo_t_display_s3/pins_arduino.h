#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x303a
#define USB_PID 0x1001

static const uint8_t BUTTON_1 = 0;
static const uint8_t BUTTON_2 = 14;
static const uint8_t BAT_VOLT = 4;

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 18;
static const uint8_t SCL = 17;

static const uint8_t SS = 10;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 13;
static const uint8_t SCK = 12;

static const uint8_t TP_RESET = 21;
static const uint8_t TP_INIT = 16;

// ST7789 IPS TFT 170x320
static const uint8_t LCD_BL = 38;
static const uint8_t LCD_D0 = 39;
static const uint8_t LCD_D1 = 40;
static const uint8_t LCD_D2 = 41;
static const uint8_t LCD_D3 = 42;
static const uint8_t LCD_D4 = 45;
static const uint8_t LCD_D5 = 46;
static const uint8_t LCD_D6 = 47;
static const uint8_t LCD_D7 = 48;
static const uint8_t LCD_WR = 8;
static const uint8_t LCD_RD = 9;
static const uint8_t LCD_DC = 7;
static const uint8_t LCD_CS = 6;
static const uint8_t LCD_RES = 5;
static const uint8_t LCD_POWER_ON = 15;

// P1
static const uint8_t PIN_43 = 43;
static const uint8_t PIN_44 = 44;
static const uint8_t PIN_18 = 18;
static const uint8_t PIN_17 = 17;
static const uint8_t PIN_21 = 21;
static const uint8_t PIN_16 = 16;

// P2
static const uint8_t PIN_1 = 1;
static const uint8_t PIN_2 = 2;
static const uint8_t PIN_3 = 3;
static const uint8_t PIN_10 = 10;
static const uint8_t PIN_11 = 11;
static const uint8_t PIN_12 = 12;
static const uint8_t PIN_13 = 13;

// Analog
static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A9 = 10;
static const uint8_t A10 = 11;
static const uint8_t A11 = 12;
static const uint8_t A12 = 13;
static const uint8_t A15 = 16;
static const uint8_t A16 = 17;
static const uint8_t A17 = 18;

// Touch
static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T10 = 10;
static const uint8_t T11 = 11;
static const uint8_t T12 = 12;
static const uint8_t T13 = 13;

#endif /* Pins_Arduino_h */
