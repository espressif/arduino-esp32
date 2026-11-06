#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x1001

#define USB_MANUFACTURER "Waveshare"
#define USB_PRODUCT      "ESP32-S3-Touch-LCD-4.3C"
#define USB_SERIAL       ""

// Native USB
#define USB_DN 19
#define USB_DP 20

// ST7262 RGB display
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

// GT911 touch controller
#define WS_TP_SDA 8
#define WS_TP_SCL 9
#define WS_TP_RST -1
#define WS_TP_INT 4

// SD card in 1-bit SDMMC mode
#define BOARD_HAS_SDMMC
#define BOARD_HAS_1BIT_SDMMC
#define SDMMC_CMD 11
#define SDMMC_CLK 12
#define SDMMC_D0  13

// ES8311 codec and ES7210 ADC
#define I2S_MCLK 6
#define I2S_DOUT 15
#define I2S_LRCK 16
#define I2S_SDIN 43
#define I2S_SCLK 44

// I2C IO expander channels. These indexes are not ESP32 GPIO numbers.
#define WS_IO_EXT_SDA  8
#define WS_IO_EXT_SCL  9
#define WS_IO_EXT_ADDR 0x24

#define WS_EXIO_DI0    0
#define WS_EXIO_TP_RST 1
#define WS_EXIO_LCD_BL 2
#define WS_EXIO_PA_EN  3
#define WS_EXIO_SD_CS  4
#define WS_EXIO_DI1    5
#define WS_EXIO_DO0    6
#define WS_EXIO_DO1    7

// The board exposes native USB but no dedicated UART or SPI pins.
static const uint8_t TX = -1;
static const uint8_t RX = -1;

static const uint8_t SDA = 8;
static const uint8_t SCL = 9;

static const uint8_t SS = -1;
static const uint8_t MOSI = -1;
static const uint8_t MISO = -1;
static const uint8_t SCK = -1;

#endif /* Pins_Arduino_h */
