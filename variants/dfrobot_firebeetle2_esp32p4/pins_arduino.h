#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

// BOOT_MODE 35
// BOOT_MODE2 36 pullup

static const uint8_t LED_BUILTIN = 3;
#define BUILTIN_LED LED_BUILTIN  // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t TX = 37;
static const uint8_t RX = 38;

static const uint8_t SDA = 7;
static const uint8_t SCL = 8;

// Use GPIOs 36 or lower on the P4 DevKit to avoid LDO power issues with high numbered GPIOs.
static const uint8_t SS = 31;
static const uint8_t MOSI = 29;
static const uint8_t MISO = 30;
static const uint8_t SCK = 28;

static const uint8_t A0 = 20;
static const uint8_t A1 = 21;
static const uint8_t A2 = 22;
static const uint8_t A3 = 23;
static const uint8_t A4 = 51;
static const uint8_t A5 = 49;
static const uint8_t A6 = 50;
static const uint8_t A7 = 52;

static const uint8_t T0 = 4;
static const uint8_t T1 = 5;
static const uint8_t T2 = 7;
static const uint8_t T3 = 8;

//I2S Microphone
static const uint8_t MIC_I2S_CLK = 12;
static const uint8_t MIC_I2S_DATA = 9;

//SDMMC
#define BOARD_HAS_SDMMC
#define BOARD_SDMMC_SLOT           0
#define BOARD_SDMMC_POWER_CHANNEL  4
#define BOARD_SDMMC_POWER_PIN      45
#define BOARD_SDMMC_POWER_ON_LEVEL LOW

//WIFI - ESP32C6
#define BOARD_HAS_SDIO_ESP_HOSTED
#define BOARD_SDIO_ESP_HOSTED_CLK   18
#define BOARD_SDIO_ESP_HOSTED_CMD   19
#define BOARD_SDIO_ESP_HOSTED_D0    14
#define BOARD_SDIO_ESP_HOSTED_D1    15
#define BOARD_SDIO_ESP_HOSTED_D2    16
#define BOARD_SDIO_ESP_HOSTED_D3    17
#define BOARD_SDIO_ESP_HOSTED_RESET 54

#endif /* Pins_Arduino_h */
