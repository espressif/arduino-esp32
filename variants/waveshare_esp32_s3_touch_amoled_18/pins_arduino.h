
#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// BN: ESP32 Family Device
#define USB_VID 0x303a
#define USB_PID 0x8255

#define USB_MANUFACTURER "Waveshare"
#define USB_PRODUCT      "ESP32-S3-Touch-AMOLED-1.8"
#define USB_SERIAL       ""

// display for SH8601
#define LCD_SDIO0 4
#define LCD_SDIO1 5
#define LCD_SDIO2 6
#define LCD_SDIO3 7
#define LCD_SCLK 11
#define LCD_CS 12
#define LCD_WIDTH 368
#define LCD_HEIGHT 448

// TOUCH
#define TP_INT 21

// ES8311
#define I2S_MCK_IO 16
#define I2S_BCK_IO 9
#define I2S_DI_IO 10
#define I2S_WS_IO 45
#define I2S_DO_IO 8


#define MCLKPIN             16
#define BCLKPIN              9
#define WSPIN               45
#define DOPIN               10
#define DIPIN                8
#define PA                  46

// SD
const int SDMMC_CLK = 2;
const int SDMMC_CMD = 1;
const int SDMMC_DATA = 3;

// Onboard Electric buzzer & Custom buttons
// GPIO and PSRAM conflict, need to pay attention when using

// UART0 pins
static const uint8_t TX = 43;
static const uint8_t RX = 44;

// Def for I2C that shares the IMU I2C pins
static const uint8_t SDA = 15;
static const uint8_t SCL = 14;

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
