#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

#define USB_VID 0x303a
#define USB_PID 0x1001

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 41;
static const uint8_t SCL = 40;

static const uint8_t SS    = 10;
static const uint8_t MOSI  = 11;
static const uint8_t MISO  = 13;
static const uint8_t SCK   = 12;

static const uint8_t A8 = 9;
static const uint8_t A9 = 10;
static const uint8_t A10 = 11;
static const uint8_t A11 = 12;
static const uint8_t A12 = 13;
static const uint8_t A13 = 14;

static const uint8_t T9 = 9;
static const uint8_t T10 = 10;
static const uint8_t T11 = 11;
static const uint8_t T12 = 12;
static const uint8_t T13 = 13;
static const uint8_t T14 = 14;

// Wire1 for ES7210 MIC ADC, ES8311 I2S DAC, ICM-42607-P IMU and TT21100 + ST7789 or GT911 + ILI9341 (Display + Touch Panel)
#define I2C_SDA     8
#define I2C_SCL    18

#define ES7210_ADDR    0x40 //MIC ADC
#define ES8311_ADDR    0x18 //I2S DAC
#define TT21100_ADDR   0x24 //Touch Panel option 1
#define GT911_ADDR     0x5D //Touch Panel option 2
#define GT911_ADDR_2   0x14 // Backup Address for GT911

// Display Driver ILI9341 or ST7789
#define TFT_DC      4
#define TFT_CS      5
#define TFT_MOSI    6
#define TFT_CLK     7
#define TFT_MISO    -1
#define TFT_BL     47
#define TFT_RST    48

#define I2S_LRCK   45
#define I2S_MCLK    2
#define I2S_SCLK   17
#define I2S_SDIN   16
#define I2S_DOUT   15

#define PA_PIN     46 //Audio Amp Power
#define MUTE_PIN    1 //MUTE Button
#define TS_IRQ      3 //Touch Screen IRQ

// SDCARD Slot
#define BOARD_HAS_SDMMC
#define SDMMC_D0     9 // SDMMC Data0 /   SDCard SPI MISO
#define SDMMC_D1    13 // SDMMC Data1
#define SDMMC_D2    42 // SDMMC Data2
#define SDMMC_D3    12 // SDMMC Data3 / SDCard SPI CS
#define SDMMC_CMD   14 // SDMMC CMD   / SDCard SPI MOSI
#define SDMMC_CLK   11 // SDMMC CLK   / SDCard SPI SCK
#define SDMMC_POWER 43 // Controls SDMMC Power
#define BOARD_MAX_SDMMC_FREQ SDMMC_FREQ_DEFAULT

// 320x240 LCD
#define BOARD_HAS_SPI_LCD
#define LCD_MODEL  ST7789
#define LCD_WIDTH  240
#define LCD_HEIGHT 320 // *RAM height is actually 320!
#define LCD_MISO    -1 // LCD Does not use MISO.
#define LCD_DC       4 // Used to switch data and command status.
#define LCD_CS       5 // used to enable LCD, low level to enable.
#define LCD_MOSI     6 // LCD SPI MOSI.
#define LCD_CLK      7 // LCD SPI Clock.
#define LCD_RST     48 // used to reset LCD, low level to reset.
#define LCD_BL      47 // LCD backlight control.


#endif /* Pins_Arduino_h */
