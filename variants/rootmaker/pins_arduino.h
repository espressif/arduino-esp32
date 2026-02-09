#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// Some boards have too low voltage on this pin (board design bug)
// Use different pin with 3V and connect with 45
// and change this setup for the chosen pin (for example 38)
#define PIN_RGB_LED 45
// BUILTIN_LED can be used in new Arduino API digitalWrite() like in Blink.ino
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_RGB_LED;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN
// RGB_BUILTIN and RGB_BRIGHTNESS can be used in new Arduino API rgbLedWrite()
#define RGB_BUILTIN    LED_BUILTIN
#define RGB_BRIGHTNESS 7

// UART0
static const uint8_t TX = 43;
static const uint8_t RX = 44;

// 6Dsensor + LED
static const uint8_t SEN_INT = 42;
static const uint8_t SDA = 41;
static const uint8_t SCL = 40;

// LCD
static const uint8_t LCD_SDA = 9;
static const uint8_t LCD_BL  = 7;
static const uint8_t LCD_CS  = 5;
static const uint8_t LCD_RS  = 3;
static const uint8_t LCD_SCK = 12;
static const uint8_t LCD_RST = 14;

// TP
static const uint8_t TP_INT = 1;
static const uint8_t TP_SDA = 41;
static const uint8_t TP_SCL = 40;
static const uint8_t TP_RST = 2;

// SPI
static const uint8_t SS   = 29;
static const uint8_t MOSI = 31;
static const uint8_t MISO = 32;
static const uint8_t SCK  = 30;
static const uint8_t WP   = 28;
static const uint8_t HD   = 27;

// 4个扩展接口
static const uint8_t PORT1_SDA = 15;
static const uint8_t PORT1_SCL = 16;

static const uint8_t PORT2_SDA = 19;
static const uint8_t PORT2_SCL = 20;

static const uint8_t PORT3_SDA = 13;
static const uint8_t PORT3_SCL = 14;

static const uint8_t PORT4_SDA = 17;
static const uint8_t PORT4_SCL = 18;

// F_BUS
static const uint8_t F_BUS7  = 45;
static const uint8_t F_BUS8  = 41;
static const uint8_t F_BUS9  = 40;
static const uint8_t F_BUS10 = 35;
static const uint8_t F_BUS11 = 33;
static const uint8_t F_BUS12 = 39;
static const uint8_t F_BUS13 = 38;
static const uint8_t F_BUS14 = 37;
static const uint8_t F_BUS15 = 36;
static const uint8_t F_BUS16 = 34;
static const uint8_t F_BUS17 = 21;

static const uint8_t F_BUS24 = 20;
static const uint8_t F_BUS25 = 19;
static const uint8_t F_BUS26 = 18;
static const uint8_t F_BUS27 = 17;
static const uint8_t F_BUS28 = 16;
static const uint8_t F_BUS29 = 15;
static const uint8_t F_BUS30 = 14;
static const uint8_t F_BUS31 = 13;
static const uint8_t F_BUS32 = 48;
static const uint8_t F_BUS33 = 47;
static const uint8_t F_BUS34 = 46;

// S_BUS
static const uint8_t S_BUS1  = 48;
static const uint8_t S_BUS2  = 47;
static const uint8_t S_BUS3  = 46;
static const uint8_t S_BUS4  = 38;
static const uint8_t S_BUS5  = 39;
static const uint8_t S_BUS6  = 33;
static const uint8_t S_BUS7  = 35;
static const uint8_t S_BUS8  = 40;
static const uint8_t S_BUS9  = 41;
static const uint8_t S_BUS10 = 45;
static const uint8_t S_BUS11 = 37;
static const uint8_t S_BUS12 = 36;
static const uint8_t S_BUS13 = 34;
static const uint8_t S_BUS14 = 21;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A6 = 7;
static const uint8_t A7 = 8;
static const uint8_t A8 = 9;
static const uint8_t A9 = 10;
static const uint8_t A10 = 11;
static const uint8_t A11 = 12;
static const uint8_t A12 = 13;
static const uint8_t A13 = 14;
static const uint8_t A14 = 15;
static const uint8_t A15 = 16;
static const uint8_t A16 = 17;
static const uint8_t A17 = 18;
static const uint8_t A18 = 19;
static const uint8_t A19 = 20;

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
