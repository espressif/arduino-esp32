/*
 * WT9932P4C61-TINY pin definitions.
 * Copyright (c) 2026 Wireless-Tag.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define WT_BOARD_NAME          "WT9932P4C61-TINY"
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
#define WT_SD_USE_SPI     1
#define WT_SD_SLOT        1
#define WT_SD_CLK         43
#define WT_SD_CMD         44
#define WT_SD_D0          39
#define WT_SD_D1          40
#define WT_SD_D2          41
#define WT_SD_D3          42
#define WT_SD_LDO_CHANNEL -1
#define WT_SD_LDO_MV      3300
#define WT_SD_SPI_SCK     WT_SD_CLK
#define WT_SD_SPI_MOSI    WT_SD_CMD
#define WT_SD_SPI_MISO    WT_SD_D0
#define WT_SD_SPI_CS      WT_SD_D3

#define WT_LCD_BACKLIGHT_PIN 26
#define WT_LCD_RESET_PIN     27
#define WT_CAMERA_POWER_PIN  0
#define WT_CAMERA_SDA        7
#define WT_CAMERA_SCL        8
#define WT_TOUCH_ADDRESS     0x55
#define WT_CAMERA_ADDRESS    0x30

// ESP32-C61 SDIO wiring only; this does not enable ESP-Hosted.
#define WT_HOSTED_CLK   18
#define WT_HOSTED_CMD   19
#define WT_HOSTED_D0    14
#define WT_HOSTED_D1    15
#define WT_HOSTED_D2    16
#define WT_HOSTED_D3    17
#define WT_HOSTED_RESET 13

#endif /* Pins_Arduino_h */
