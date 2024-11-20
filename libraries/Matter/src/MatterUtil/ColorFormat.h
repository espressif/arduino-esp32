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

struct RgbColor_t {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct HsvColor_t {
  int16_t h;
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

RgbColor_t XYToRgb(uint8_t Level, uint16_t currentX, uint16_t currentY);
RgbColor_t HsvToRgb(HsvColor_t hsv);
RgbColor_t CTToRgb(CtColor_t ct);
HsvColor_t RgbToHsv(RgbColor_t rgb);
