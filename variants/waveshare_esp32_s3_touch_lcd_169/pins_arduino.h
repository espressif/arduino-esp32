
#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// BN: ESP32 Family Device
#define USB_VID 0x303a
#define USB_PID 0x821e

#define USB_MANUFACTURER "Waveshare"
#define USB_PRODUCT      "ESP32-S3-Touch-LCD-1.69"
#define USB_SERIAL       ""

// display for ST7789V2
#define WS_LCD_DC  4
#define WS_LCD_CS  5
#define WS_LCD_SCL 6
#define WS_LCD_SDA 7
#define WS_LCD_RST 8
#define WS_LCD_BL  15

// Touch for CST816T
#define WS_TP_SCL 10
#define WS_TP_SDA 11
#define WS_TP_RST 13
#define WS_TP_INT 14

// Onboard RTC for PCF85063
#define WS_RTC_SCL     10
#define WS_RTC_SDA     11
#define WS_RTC_ADDRESS 0x51
#define WS_RTC_INT     41

// Onboard  QMI8658 IMU
#define WS_QMI8658_SDA     11
#define WS_QMI8658_SCL     10
#define WS_QMI8658_ADDRESS 0x6B
#define WS_QMI8658_INT1    38

// Onboard Electric buzzer & Custom buttons
// GPIO and PSRAM conflict, need to pay attention when using
#define WS_BUZZ    33  // Please pull down the level when using
#define WS_SYS_OUT 36
#define WS_SYS_EN  35

// Partial voltage measurement method
#define WS_BAT_ADC 1

// UART0 pins
static const uint8_t TX = 43;
static const uint8_t RX = 44;

// Def for I2C that shares the IMU I2C pins
static const uint8_t SDA = 11;
static const uint8_t SCL = 10;

// Mapping based on the ESP32S3 data sheet - alternate for SPI2
static const uint8_t SS = 34;    // FSPICS0
static const uint8_t MOSI = 35;  // FSPID
static const uint8_t MISO = 37;  // FSPIQ
static const uint8_t SCK = 36;   // FSPICLK

// Mapping based on the ESP32S3 data sheet - alternate for OUTPUT
static const uint8_t OUTPUT_IO2 = 2;
static const uint8_t OUTPUT_IO3 = 3;
static const uint8_t OUTPUT_IO17 = 17;
static const uint8_t OUTPUT_IO18 = 18;

// Analog capable pins on the header
static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A6 = 7;

// GPIO capable pins on the header
static const uint8_t D0 = 7;
static const uint8_t D1 = 6;
static const uint8_t D2 = 5;
static const uint8_t D3 = 4;
static const uint8_t D4 = 3;
static const uint8_t D5 = 2;
static const uint8_t D6 = 1;
static const uint8_t D7 = 44;
static const uint8_t D8 = 43;
static const uint8_t D9 = 40;
static const uint8_t D10 = 39;
static const uint8_t D11 = 38;
static const uint8_t D12 = 37;
static const uint8_t D13 = 36;
static const uint8_t D14 = 35;
static const uint8_t D15 = 34;
static const uint8_t D16 = 33;

// Touch input capable pins on the header
static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T4 = 4;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;

#endif /* Pins_Arduino_h */
