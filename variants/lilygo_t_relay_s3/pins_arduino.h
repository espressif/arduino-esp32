#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x1001

#define EXTERNAL_NUM_INTERRUPTS 46
#define NUM_DIGITAL_PINS        48
#define NUM_ANALOG_INPUTS       20

#define analogInputToDigitalPin(p)  (((p)<20)?(analogChannelToDigitalPin(p)):-1)
#define digitalPinToInterrupt(p)    (((p)<48)?(p):-1)
#define digitalPinHasPWM(p)         (p < 46)

static const uint8_t GREEN_LED_CH = 6
static const uint8_t RED_LED_CH = 7

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 16;
static const uint8_t SCL = 17;

static const uint8_t SS    = 10;
static const uint8_t MOSI  = 11;
static const uint8_t MISO  = 13;
static const uint8_t SCK   = 12;

// TFT Header
static const uint8_t TFT_RST    = 3;
static const uint8_t TFT_CS     = 8;
static const uint8_t TFT_BL     = 9;
static const uint8_t TFT_TP_CS  = 10;
static const uint8_t TFT_MOSI   = 11;
static const uint8_t TFT_MISO   = 12;
static const uint8_t TFT_CLK    = 13;
static const uint8_t TFT_TP_INT = 14;
static const uint8_t TFT_DC     = 46;

// Temperature Sensor
static const uint8_t DSDATA = 21; // Not Connected

// RTC
static const uint8_t RTC_CLK32 = 15; // Not Connected
static const uint8_t RTC_INT = 18;

// HT74HC595ARZ

static const uint8_t SHIFT_REG_CLK   = 5
static const uint8_t SHIFT_REG_LATCH = 6
static const uint8_t SHIFT_REG_DATA  = 7

// GPIO
static const uint8_t D1  = 1;
static const uint8_t D2  = 2;
static const uint8_t D3  = 3;
static const uint8_t D4  = 4;
// D5 - D7 -> HT74HC595ARZ
static const uint8_t D8  = 8;
static const uint8_t D9  = 9;
static const uint8_t D10 = 10;
static const uint8_t D11 = 11;
static const uint8_t D12 = 12;
static const uint8_t D13 = 13;
static const uint8_t D14 = 14;
static const uint8_t D15 = 15;
// D16 - D18 -> I2C RTC + RTC INT
// D19 - D20 -> USB
static const uint8_t D21 = 21;
// D22 - D34 -> N/A on Wroom S3 1U
// D35 - D37 -> OCTAL PS-RAM
static const uint8_t D38 = 38;
static const uint8_t D39 = 39;
static const uint8_t D40 = 40;
static const uint8_t D41 = 41;
static const uint8_t D42 = 42;
// D43 - D44 -> UART0
static const uint8_t D45 = 45;
static const uint8_t D46 = 46;
static const uint8_t D47 = 47;
static const uint8_t D48 = 48;

// Analog
static const uint8_t A0  = 1;
static const uint8_t A1  = 2;
static const uint8_t A2  = 3;
static const uint8_t A3  = 4;
static const uint8_t A7  = 8;
static const uint8_t A8  = 9;
static const uint8_t A9 = 10;
static const uint8_t A10 = 11;
static const uint8_t A11 = 12;
static const uint8_t A12 = 13;
static const uint8_t A13 = 14;
static const uint8_t A14 = 15;

// Touch
static const uint8_t T_1  = 1;
static const uint8_t T_2  = 2;
static const uint8_t T_3  = 3;
static const uint8_t T_4  = 4;
static const uint8_t T_8  = 8;
static const uint8_t T_9  = 9;
static const uint8_t T_10 = 10;
static const uint8_t T_11 = 11;
static const uint8_t T_12 = 12;
static const uint8_t T_13 = 13;
static const uint8_t T_14 = 14;

#endif /* Pins_Arduino_h */

