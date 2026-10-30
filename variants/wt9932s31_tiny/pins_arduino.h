/*
 * WT9932S31-TINY pin definitions.
 * Copyright (c) 2026 Wireless-Tag.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define WT_BOARD_NAME          "WT9932S31-TINY"
#define WT_RGB_PIN             19
#define WT_BUTTON_PIN          61
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
static const uint8_t TX = 58;
static const uint8_t RX = 59;

static const uint8_t SDA = 2;
static const uint8_t SCL = 3;

static const uint8_t SS = 37;
static const uint8_t MOSI = 38;
static const uint8_t MISO = 39;
static const uint8_t SCK = 40;

static const uint8_t A0 = 42;
static const uint8_t A1 = 43;
static const uint8_t A2 = 44;
static const uint8_t A3 = 45;
static const uint8_t A4 = 46;
static const uint8_t A5 = 47;

#define WT_HAS_DSI        0
#define WT_HAS_CSI        0
#define WT_HAS_DVP        1
#define WT_HAS_SDMMC      1
#define WT_SD_USE_SPI     0
#define WT_SD_SLOT        0
#define WT_SD_CLK         24
#define WT_SD_CMD         25
#define WT_SD_D0          20
#define WT_SD_D1          21
#define WT_SD_D2          22
#define WT_SD_D3          23
#define WT_SD_LDO_CHANNEL 1
#define WT_SD_LDO_MV      1800

#define WT_CAMERA_POWER_PIN -1

// The SD pads require 1.8 V. No generic SD_MMC power rail is enabled here.
#define WT_DVP_SDA_PIN   0
#define WT_DVP_SCL_PIN   1
#define WT_DVP_XCLK_PIN  55
#define WT_DVP_PCLK_PIN  54
#define WT_DVP_VSYNC_PIN 56
#define WT_DVP_HREF_PIN  57
#define WT_DVP_D0_PIN    46
#define WT_DVP_D1_PIN    47
#define WT_DVP_D2_PIN    48
#define WT_DVP_D3_PIN    49
#define WT_DVP_D4_PIN    50
#define WT_DVP_D5_PIN    51
#define WT_DVP_D6_PIN    52
#define WT_DVP_D7_PIN    53

#endif /* Pins_Arduino_h */
