
#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// BN: ESP32 Family Device
#define USB_VID 0x303a
#define USB_PID 0x8237

#define USB_MANUFACTURER "Waveshare"
#define USB_PRODUCT      "ESP32-S3-Touch-LCD-5"
#define USB_SERIAL       ""

// display for ST7262
#define WS_LCD_B3 14
#define WS_LCD_B4 38
#define WS_LCD_B5 18
#define WS_LCD_B6 17
#define WS_LCD_B7 10

#define WS_LCD_G2 39
#define WS_LCD_G3 0
#define WS_LCD_G4 45
#define WS_LCD_G5 48
#define WS_LCD_G6 47
#define WS_LCD_G7 21

#define WS_LCD_R3 1
#define WS_LCD_R4 2
#define WS_LCD_R5 42
#define WS_LCD_R6 41
#define WS_LCD_R7 40

#define WS_LCD_VSYNC 3
#define WS_LCD_HSYNC 46
#define WS_LCD_PCLK  7
#define WS_LCD_DE    5

// Touch for gt911
#define WS_TP_SDA 8
#define WS_TP_SCL 9
#define WS_TP_RST -1
#define WS_TP_INT 4

//RS485
#define WS_RS485_RXD 43
#define WS_RS485_TXD 44

//CAN
#define WS_CAN_RXD 15
#define WS_CAN_TXD 16

//Onboard CH422G IO expander
#define WS_CH422G_SDA 8
#define WS_CH422G_SCL 9

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
