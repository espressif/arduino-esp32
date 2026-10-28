/*
 * WT9932P4X-TINY pin definitions.
 * Copyright (c) 2026 Wireless-Tag.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define WT_BOARD_NAME          "WT9932P4X-TINY"
#define WT_RGB_PIN             51
#define WT_BUTTON_PIN          35
#define WT_BUTTON_ACTIVE_LEVEL LOW
#define PIN_BUTTON1            WT_BUTTON_PIN
#define PIN_RGB_LED            WT_RGB_PIN

// Use the virtual RGB pin with pinMode() and digitalWrite(), including Blink.
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_RGB_LED;
#define LED_BUILTIN    LED_BUILTIN
#define BUILTIN_LED    LED_BUILTIN
#define RGB_BUILTIN    LED_BUILTIN
#define RGB_BRIGHTNESS 32

// GPIO numbers; UART, I2C and SPI defaults are software conventions.
static const uint8_t TX = 37;
static const uint8_t RX = 38;

static const uint8_t SDA = 7;
static const uint8_t SCL = 8;

static const uint8_t SS = 5;
static const uint8_t MOSI = 32;
static const uint8_t MISO = 33;
static const uint8_t SCK = 36;

static const uint8_t A0 = 16;
static const uint8_t A1 = 17;
static const uint8_t A2 = 18;
static const uint8_t A3 = 19;
static const uint8_t A4 = 20;
static const uint8_t A5 = 21;

#define WT_HAS_DSI        1
#define WT_HAS_CSI        1
#define WT_HAS_DVP        0
#define WT_HAS_SDMMC      1
#define WT_SD_USE_SPI     0
#define WT_SD_SLOT        0
#define WT_SD_CLK         43
#define WT_SD_CMD         44
#define WT_SD_D0          39
#define WT_SD_D1          40
#define WT_SD_D2          41
#define WT_SD_D3          42
#define WT_SD_LDO_CHANNEL 4
#define WT_SD_LDO_MV      3300
#define BOARD_HAS_SDMMC
#define BOARD_SDMMC_SLOT          WT_SD_SLOT
#define BOARD_SDMMC_POWER_CHANNEL WT_SD_LDO_CHANNEL
#define SDMMC_CLK                 WT_SD_CLK
#define SDMMC_CMD                 WT_SD_CMD
#define SDMMC_D0                  WT_SD_D0
#define SDMMC_D1                  WT_SD_D1
#define SDMMC_D2                  WT_SD_D2
#define SDMMC_D3                  WT_SD_D3

// GPIO39-48 use the 3.3 V VO4 rail on these boards.
#define BOARD_PERIMAN_IO_LDO_AUTO        1
#define BOARD_PERIMAN_IO_LDO0_CHANNEL    4
#define BOARD_PERIMAN_IO_LDO0_GPIO_MIN   39
#define BOARD_PERIMAN_IO_LDO0_GPIO_MAX   48
#define BOARD_PERIMAN_IO_LDO0_VOLTAGE_MV 3300

#define WT_LCD_BACKLIGHT_PIN 26
#define WT_LCD_RESET_PIN     27
#define WT_CAMERA_POWER_PIN  0
#define WT_CAMERA_SDA        7
#define WT_CAMERA_SCL        8
#define WT_TOUCH_ADDRESS     0x55
#define WT_CAMERA_ADDRESS    0x30

#endif /* Pins_Arduino_h */
