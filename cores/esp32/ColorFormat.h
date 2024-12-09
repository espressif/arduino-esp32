/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#pragma once

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

struct RgbColor_t {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct HsvColor_t {
  uint16_t h;
  uint8_t s;
  uint8_t v;
};

struct XyColor_t {
  uint16_t x;
  uint16_t y;
};

struct CtColor_t {
  uint16_t ctMireds;
};

typedef struct RgbColor_t espRgbColor_t;
typedef struct HsvColor_t espHsvColor_t;
typedef struct XyColor_t espXyColor_t;
typedef struct CtColor_t espCtColor_t;

espRgbColor_t espXYToRgbColor(uint8_t Level, uint16_t current_X, uint16_t current_Y);
espRgbColor_t espXYColorToRgb(uint8_t Level, espXyColor_t xy);
espXyColor_t espRgbColorToXYColor(espRgbColor_t rgb);
espXyColor_t espRgbToXYColor(uint8_t r, uint8_t g, uint8_t b);
espRgbColor_t espHsvColorToRgbColor(espHsvColor_t hsv);
espRgbColor_t espHsvToRgbColor(uint16_t h, uint8_t s, uint8_t v);
espRgbColor_t espCTColorToRgbColor(espCtColor_t ct);
espRgbColor_t espCTToRgbColor(uint16_t ct);
espHsvColor_t espRgbColorToHsvColor(espRgbColor_t rgb);
espHsvColor_t espRgbToHsvColor(uint8_t r, uint8_t g, uint8_t b);

extern const espHsvColor_t HSV_BLACK, HSV_WHITE, HSV_RED, HSV_YELLOW, HSV_GREEN, HSV_CYAN, HSV_BLUE, HSV_MAGENTA;
extern const espCtColor_t COOL_WHITE_COLOR_TEMPERATURE, DAYLIGHT_WHITE_COLOR_TEMPERATURE, WHITE_COLOR_TEMPERATURE, SOFT_WHITE_COLOR_TEMPERATURE,
  WARM_WHITE_COLOR_TEMPERATURE;
extern const espRgbColor_t RGB_BLACK, RGB_WHITE, RGB_RED, RGB_YELLOW, RGB_GREEN, RGB_CYAN, RGB_BLUE, RGB_MAGENTA;

#ifdef __cplusplus
}
#endif
