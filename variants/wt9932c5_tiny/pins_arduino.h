/*
 * WT9932C5-TINY pin definitions.
 * Copyright (c) 2026 Wireless-Tag.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>
#include "soc/soc_caps.h"

#define WT_BOARD_NAME "WT9932C5-TINY"
#define WT_RGB_PIN 6
#define WT_BUTTON_PIN 28
#define WT_BUTTON_ACTIVE_LEVEL LOW
#define PIN_BUTTON1 WT_BUTTON_PIN
#define PIN_RGB_LED WT_RGB_PIN

// Use the virtual RGB pin with pinMode() and digitalWrite(), including Blink.
static const uint8_t LED_BUILTIN = SOC_GPIO_PIN_COUNT + PIN_RGB_LED;
#define LED_BUILTIN LED_BUILTIN
#define BUILTIN_LED LED_BUILTIN
#define RGB_BUILTIN LED_BUILTIN
#define RGB_BRIGHTNESS 32

// GPIO numbers; UART, I2C and SPI defaults are software conventions.
static const uint8_t TX = 11;
static const uint8_t RX = 12;

static const uint8_t SDA = 0;
static const uint8_t SCL = 1;

static const uint8_t SS = 7;
static const uint8_t MOSI = 8;
static const uint8_t MISO = 9;
static const uint8_t SCK = 10;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;

#define WT_HAS_DSI 0
#define WT_HAS_CSI 0
#define WT_HAS_DVP 0
#define WT_HAS_SDMMC 0
#define WT_SD_USE_SPI 0

#define WT_CAMERA_POWER_PIN -1

#endif /* Pins_Arduino_h */
