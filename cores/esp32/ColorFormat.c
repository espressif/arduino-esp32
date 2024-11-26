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

#include "ColorFormat.h"

#include <math.h>

// define a clamp macro to substitute the std::clamp macro which is available from C++17 onwards
#define clamp(a, min, max) ((a) < (min) ? (min) : ((a) > (max) ? (max) : (a)))

const espHsvColor_t HSV_BLACK = {0, 0, 0};
const espHsvColor_t HSV_WHITE = {0, 0, 254};
const espHsvColor_t HSV_RED = {0, 254, 254};
const espHsvColor_t HSV_YELLOW = {42, 254, 254};
const espHsvColor_t HSV_GREEN = {84, 254, 254};
const espHsvColor_t HSV_CYAN = {127, 254, 254};
const espHsvColor_t HSV_BLUE = {169, 254, 254};
const espHsvColor_t HSV_MAGENTA = {211, 254, 254};

const espRgbColor_t RGB_BLACK = {0, 0, 0};
const espRgbColor_t RGB_WHITE = {255, 255, 255};
const espRgbColor_t RGB_RED = {255, 0, 0};
const espRgbColor_t RGB_YELLOW = {255, 255, 0};
const espRgbColor_t RGB_GREEN = {0, 255, 0};
const espRgbColor_t RGB_CYAN = {0, 255, 255};
const espRgbColor_t RGB_BLUE = {0, 0, 255};
const espRgbColor_t RGB_MAGENTA = {255, 0, 255};

// main color temperature values
const espCtColor_t COOL_WHITE_COLOR_TEMPERATURE = {142};
const espCtColor_t DAYLIGHT_WHITE_COLOR_TEMPERATURE = {181};
const espCtColor_t WHITE_COLOR_TEMPERATURE = {250};
const espCtColor_t SOFT_WHITE_COLOR_TEMPERATURE = {370};
const espCtColor_t WARM_WHITE_COLOR_TEMPERATURE = {454};

espRgbColor_t espHsvToRgbColor(uint16_t h, uint8_t s, uint8_t v) {
  espHsvColor_t hsv = {h, s, v};
  return espHsvColorToRgbColor(hsv);
}

espRgbColor_t espHsvColorToRgbColor(espHsvColor_t hsv) {
  espRgbColor_t rgb;

  uint8_t region, p, q, t;
  uint32_t h, s, v, remainder;

  if (hsv.s == 0) {
    rgb.r = rgb.g = rgb.b = hsv.v;
  } else {
    h = hsv.h;
    s = hsv.s;
    v = hsv.v;

    region = h / 43;
    remainder = (h - (region * 43)) * 6;
    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    switch (region) {
      case 0:  rgb.r = v, rgb.g = t, rgb.b = p; break;
      case 1:  rgb.r = q, rgb.g = v, rgb.b = p; break;
      case 2:  rgb.r = p, rgb.g = v, rgb.b = t; break;
      case 3:  rgb.r = p, rgb.g = q, rgb.b = v; break;
      case 4:  rgb.r = t, rgb.g = p, rgb.b = v; break;
      case 5:
      default: rgb.r = v, rgb.g = p, rgb.b = q; break;
    }
  }
  return rgb;
}

espHsvColor_t espRgbToHsvColor(uint8_t r, uint8_t g, uint8_t b) {
  espRgbColor_t rgb = {r, g, b};
  return espRgbColorToHsvColor(rgb);
}

espHsvColor_t espRgbColorToHsvColor(espRgbColor_t rgb) {
  espHsvColor_t hsv;
  uint8_t rgbMin, rgbMax;

  rgbMin = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
  rgbMax = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);

  hsv.v = rgbMax;
  if (hsv.v == 0) {
    hsv.h = 0;
    hsv.s = 0;
    return hsv;
  }

  hsv.s = 255 * (rgbMax - rgbMin) / hsv.v;
  if (hsv.s == 0) {
    hsv.h = 0;
    return hsv;
  }
  if (rgbMax == rgb.r) {
    hsv.h = 0 + 43 * (rgb.g - rgb.b) / (rgbMax - rgbMin);
  } else if (rgbMax == rgb.g) {
    hsv.h = 85 + 43 * (rgb.b - rgb.r) / (rgbMax - rgbMin);
  } else {
    hsv.h = 171 + 43 * (rgb.r - rgb.g) / (rgbMax - rgbMin);
  }
  return hsv;
}

espRgbColor_t espXYColorToRgbColor(uint8_t Level, espXyColor_t xy) {
  return espXYToRgbColor(Level, xy.x, xy.y);
}

espRgbColor_t espXYToRgbColor(uint8_t Level, uint16_t current_X, uint16_t current_Y) {
  // convert xyY color space to RGB

  // https://www.easyrgb.com/en/math.php
  // https://en.wikipedia.org/wiki/SRGB
  // refer https://en.wikipedia.org/wiki/CIE_1931_color_space#CIE_xy_chromaticity_diagram_and_the_CIE_xyY_color_space

  // The current_X/current_Y attribute contains the current value of the normalized chromaticity value of x/y.
  // The value of x/y shall be related to the current_X/current_Y attribute by the relationship
  // x = current_X/65536
  // y = current_Y/65536
  // z = 1-x-y

  espRgbColor_t rgb;

  float x, y, z;
  float X, Y, Z;
  float r, g, b;

  x = ((float)current_X) / 65535.0f;
  y = ((float)current_Y) / 65535.0f;

  z = 1.0f - x - y;

  // Calculate XYZ values

  // Y - given brightness in 0 - 1 range
  Y = ((float)Level) / 254.0f;
  X = (Y / y) * x;
  Z = (Y / y) * z;

  // X, Y and Z input refer to a D65/2° standard illuminant.
  // sR, sG and sB (standard RGB) output range = 0 ÷ 255
  // convert XYZ to RGB - CIE XYZ to sRGB
  X = X / 100.0f;
  Y = Y / 100.0f;
  Z = Z / 100.0f;

  r = (X * 3.2406f) - (Y * 1.5372f) - (Z * 0.4986f);
  g = -(X * 0.9689f) + (Y * 1.8758f) + (Z * 0.0415f);
  b = (X * 0.0557f) - (Y * 0.2040f) + (Z * 1.0570f);

  // apply gamma 2.2 correction
  r = (r <= 0.0031308f ? 12.92f * r : (1.055f) * pow(r, (1.0f / 2.4f)) - 0.055f);
  g = (g <= 0.0031308f ? 12.92f * g : (1.055f) * pow(g, (1.0f / 2.4f)) - 0.055f);
  b = (b <= 0.0031308f ? 12.92f * b : (1.055f) * pow(b, (1.0f / 2.4f)) - 0.055f);

  // Round off
  r = clamp(r, 0, 1);
  g = clamp(g, 0, 1);
  b = clamp(b, 0, 1);

  // these rgb values are in  the range of 0 to 1, convert to limit of HW specific LED
  rgb.r = (uint8_t)(r * 255);
  rgb.g = (uint8_t)(g * 255);
  rgb.b = (uint8_t)(b * 255);

  return rgb;
}

espXyColor_t espRgbToXYColor(uint8_t r, uint8_t g, uint8_t b) {
  espRgbColor_t rgb = {r, g, b};
  return espRgbColorToXYColor(rgb);
}

espXyColor_t espRgbColorToXYColor(espRgbColor_t rgb) {
  // convert RGB to xy color space

  // https://www.easyrgb.com/en/math.php
  // https://en.wikipedia.org/wiki/SRGB
  // refer https://en.wikipedia.org/wiki/CIE_1931_color_space#CIE_xy_chromaticity_diagram_and_the_CIE_xyY_color_space

  espXyColor_t xy;

  float r, g, b;
  float X, Y, Z;
  float x, y;

  r = ((float)rgb.r) / 255.0f;
  g = ((float)rgb.g) / 255.0f;
  b = ((float)rgb.b) / 255.0f;

  // convert RGB to XYZ - sRGB to CIE XYZ
  r = (r <= 0.04045f ? r / 12.92f : pow((r + 0.055f) / 1.055f, 2.4f));
  g = (g <= 0.04045f ? g / 12.92f : pow((g + 0.055f) / 1.055f, 2.4f));
  b = (b <= 0.04045f ? b / 12.92f : pow((b + 0.055f) / 1.055f, 2.4f));

  // https://gist.github.com/popcorn245/30afa0f98eea1c2fd34d
  X = r * 0.649926f + g * 0.103455f + b * 0.197109f;
  Y = r * 0.234327f + g * 0.743075f + b * 0.022598f;
  Z = r * 0.0000000f + g * 0.053077f + b * 1.035763f;

  // sR, sG and sB (standard RGB) input range = 0 ÷ 255
  // X, Y and Z output refer to a D65/2° standard illuminant.
  X = r * 0.4124564f + g * 0.3575761f + b * 0.1804375f;
  Y = r * 0.2126729f + g * 0.7151522f + b * 0.0721750f;
  Z = r * 0.0193339f + g * 0.1191920f + b * 0.9503041f;

  // Calculate xy values
  x = X / (X + Y + Z);
  y = Y / (X + Y + Z);

  // convert to 0-65535 range
  xy.x = (uint16_t)(x * 65535);
  xy.y = (uint16_t)(y * 65535);
  return xy;
}

espRgbColor_t espCTToRgbColor(uint16_t ct) {
  espCtColor_t ctColor = {ct};
  return espCTColorToRgbColor(ctColor);
}

espRgbColor_t espCTColorToRgbColor(espCtColor_t ct) {
  espRgbColor_t rgb = {0, 0, 0};
  float r, g, b;

  if (ct.ctMireds == 0) {
    return rgb;
  }
  // Algorithm credits to Tanner Helland: https://tannerhelland.com/2012/09/18/convert-temperature-rgb-algorithm-code.html

  // Convert Mireds to centiKelvins. k = 1,000,000/mired
  float ctCentiKelvin = 10000 / ct.ctMireds;

  // Red
  if (ctCentiKelvin <= 66) {
    r = 255;
  } else {
    r = 329.698727446f * pow(ctCentiKelvin - 60, -0.1332047592f);
  }

  // Green
  if (ctCentiKelvin <= 66) {
    g = 99.4708025861f * log(ctCentiKelvin) - 161.1195681661f;
  } else {
    g = 288.1221695283f * pow(ctCentiKelvin - 60, -0.0755148492f);
  }

  // Blue
  if (ctCentiKelvin >= 66) {
    b = 255;
  } else {
    if (ctCentiKelvin <= 19) {
      b = 0;
    } else {
      b = 138.5177312231 * log(ctCentiKelvin - 10) - 305.0447927307;
    }
  }
  rgb.r = (uint8_t)clamp(r, 0, 255);
  rgb.g = (uint8_t)clamp(g, 0, 255);
  rgb.b = (uint8_t)clamp(b, 0, 255);

  return rgb;
}
