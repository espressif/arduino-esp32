#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_MANUFACTURER "Waveshare"
#define USB_PRODUCT      "ESP32-S3-Touch-LCD-7C-BOX"
#define USB_SERIAL       ""

// RGB display
#define WS_LCD_B3 14
#define WS_LCD_B4 38
#define WS_LCD_B5 18
#define WS_LCD_B6 17
#define WS_LCD_B7 10

#define WS_LCD_G2 39
#define WS_LCD_G3 0
#define WS_LCD_G4 45
#define WS_LCD_G5 9
#define WS_LCD_G6 8
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
#define WS_LCD_RST   -1
#define WS_LCD_BL    -1

// GT911 touch controller
#define WS_TP_SDA 47
#define WS_TP_SCL 48
#define WS_TP_RST -1
#define WS_TP_INT 4

// CH32V006 I2C IO expander channels; these are not ESP32 GPIO numbers.
#define WS_IO_EXPANDER_ADDR 0x24
#define WS_EXIO_LCD_RST     0
#define WS_EXIO_TP_RST      1
#define WS_EXIO_LCD_BL      2
#define WS_EXIO_PA_CTRL     3
#define WS_EXIO_SD_CS       4
#define WS_EXIO_LCD_VDD_EN  5
#define WS_EXIO_STATUS_LED  6

// P1 expansion header
#define WS_EXIO_DI0     7
#define WS_EXIO_DI1     8
#define WS_EXIO_DI2     9
#define WS_EXIO_DI3     10
#define WS_EXIO_DO0     11
#define WS_EXIO_DO1     12
#define WS_EXIO_DO2     13
#define WS_EXIO_DO3     14
#define WS_EXIO_RTC_INT 15

// UART0 pins are shared with the I2S audio interface.
static const uint8_t TX = 43;
static const uint8_t RX = 44;

// Shared I2C bus for touch, IO expander, RTC, fuel gauge, and audio codecs.
static const uint8_t SDA = 47;
static const uint8_t SCL = 48;

// The SD card CS signal is WS_EXIO_SD_CS, not an ESP32 GPIO.
static const int8_t SS = -1;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 13;
static const uint8_t SCK = 12;

// 1-bit SDMMC shares the card's SPI nets. Drive EXIO4 high, then pass these
// pins to SD_MMC.setPins() and enable one-bit mode when mounting the card.
#define SDMMC_CMD 11
#define SDMMC_CLK 12
#define SDMMC_D0  13

// ES8389 and ES7210 audio interface
#define I2S_MCLK  6
#define I2S_BCLK  44
#define I2S_LRCLK 16
#define I2S_DOUT  15
#define I2S_DIN   43

// Native USB; the boot button is shared with LCD G3.
#define USB_DN 19
#define USB_DP 20
static const uint8_t BOOT_BTN = 0;

#endif /* Pins_Arduino_h */
