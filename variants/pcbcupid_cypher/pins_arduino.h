#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// RGB LED
static const uint8_t LED_BUILTIN = 1;
#define BUILTIN_LED LED_BUILTIN
#define LED_BUILTIN LED_BUILTIN

static const uint8_t LED_BUILTIN_R = 1;
static const uint8_t LED_BUILTIN_G = 2;
static const uint8_t LED_BUILTIN_B = 3;

// UART1
static const uint8_t TX = 21;
static const uint8_t RX = 22;

// I2C
static const uint8_t SDA = 4;
static const uint8_t SCL = 5;

// SPI / TFT
static const uint8_t SS = 16;
static const uint8_t MISO = -1;
static const uint8_t MOSI = 11;
static const uint8_t SCK = 12;

static const uint8_t TFT_DC = 15;
static const uint8_t TFT_RST = 14;

// ADC / RGB (shared pins)
static const uint8_t A1 = 1;
static const uint8_t A2 = 2;
static const uint8_t A3 = 3;

// SD_MMC
#define BOARD_HAS_SDMMC

static const uint8_t SDMMC_CMD = 35;
static const uint8_t SDMMC_CLK = 36;
static const uint8_t SDMMC_D0 = 37;
static const uint8_t SDMMC_D1 = 38;
static const uint8_t SDMMC_D2 = 39;
static const uint8_t SDMMC_D3 = 40;

// Boot
static const uint8_t D0 = 0;

#endif /* Pins_Arduino_h */
