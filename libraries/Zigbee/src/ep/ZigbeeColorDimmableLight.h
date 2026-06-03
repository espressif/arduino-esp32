// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/* Class of Zigbee On/Off Light endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zha.h"
#include "ezbee/zcl/cluster/on_off_desc.h"
#include "ezbee/zcl/cluster/level_desc.h"
#include "ezbee/zcl/cluster/color_control_desc.h"

// Color capabilities bit flags (matching ZCL spec) - can be combined with bitwise OR
static constexpr uint16_t ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION = (1 << 0);  // Bit 0: Hue/saturation supported
static constexpr uint16_t ZIGBEE_COLOR_CAPABILITY_ENHANCED_HUE = (1 << 1);    // Bit 1: Enhanced hue supported
static constexpr uint16_t ZIGBEE_COLOR_CAPABILITY_COLOR_LOOP = (1 << 2);      // Bit 2: Color loop supported
static constexpr uint16_t ZIGBEE_COLOR_CAPABILITY_X_Y = (1 << 3);             // Bit 3: X/Y supported
static constexpr uint16_t ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP = (1 << 4);      // Bit 4: Color temperature supported

// Color mode enum values (matching ZCL spec)
enum ZigbeeColorMode {
  ZIGBEE_COLOR_MODE_HUE_SATURATION = 0x00,  // CurrentHue and CurrentSaturation
  ZIGBEE_COLOR_MODE_CURRENT_X_Y = 0x01,     // CurrentX and CurrentY // codespell:ignore currenty
  ZIGBEE_COLOR_MODE_TEMPERATURE = 0x02,     // ColorTemperature
};

// Callback function type definitions for better readability and type safety
// RGB callback: (state, red, green, blue, level)
typedef void (*ZigbeeColorLightRgbCallback)(bool state, uint8_t red, uint8_t green, uint8_t blue, uint8_t level);
// HSV callback: (state, hue, saturation, value) - value represents brightness (0-255)
typedef void (*ZigbeeColorLightHsvCallback)(bool state, uint8_t hue, uint8_t saturation, uint8_t value);
// Temperature callback: (state, level, color_temperature_in_mireds)
typedef void (*ZigbeeColorLightTempCallback)(bool state, uint8_t level, uint16_t color_temperature);

class ZigbeeColorDimmableLight : public ZigbeeEP {
public:
  ZigbeeColorDimmableLight(uint8_t endpoint);
  ~ZigbeeColorDimmableLight() {}

  // Must be called before starting Zigbee, by default XY are selected as color mode
  bool setLightColorCapabilities(uint16_t capabilities);

  [[deprecated("Use onLightChangeRgb() instead. This will be removed in a future major version.")]]
  void onLightChange(ZigbeeColorLightRgbCallback callback) {
    _on_light_change_rgb = callback;
  }
  void onLightChangeRgb(ZigbeeColorLightRgbCallback callback) {
    _on_light_change_rgb = callback;
  }
  void onLightChangeHsv(ZigbeeColorLightHsvCallback callback) {
    _on_light_change_hsv = callback;
  }
  void onLightChangeTemp(ZigbeeColorLightTempCallback callback) {
    _on_light_change_temp = callback;
  }
  void restoreLight() {
    lightChangedByMode();
  }

  bool setLightState(bool state);
  bool setLightLevel(uint8_t level);
  bool setLightColor(uint8_t red, uint8_t green, uint8_t blue);
  bool setLightColor(espRgbColor_t rgb_color);
  bool setLightColor(espHsvColor_t hsv_color);
  bool setLightColorTemperature(uint16_t color_temperature);
  bool setLight(bool state, uint8_t level, uint8_t red, uint8_t green, uint8_t blue);
  bool setLightColorTemperatureRange(uint16_t min_temp, uint16_t max_temp);

  bool getLightState() {
    return _current_state;
  }
  uint8_t getLightLevel() {
    return _current_level;
  }
  espRgbColor_t getLightColor() {
    return _current_color;
  }
  uint8_t getLightRed() {
    return _current_color.r;
  }
  uint8_t getLightGreen() {
    return _current_color.g;
  }
  uint8_t getLightBlue() {
    return _current_color.b;
  }
  uint16_t getLightColorTemperature() {
    return _current_color_temperature;
  }
  uint8_t getLightColorMode() {
    return _current_color_mode;
  }
  uint8_t getLightColorHue() {
    return _current_hsv.h;
  }
  uint8_t getLightColorSaturation() {
    return _current_hsv.s;
  }
  uint16_t getLightColorCapabilities() {
    return _color_capabilities;
  }

private:
  void zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) override;
  bool setLightColorMode(uint8_t color_mode);
  bool isColorModeSupported(uint8_t color_mode);

  uint16_t getCurrentColorX();
  uint16_t getCurrentColorY();
  uint8_t getCurrentColorHue();
  uint8_t getCurrentColorSaturation();
  uint16_t getCurrentColorTemperature();

  // Stack callback helpers (must not use Zigbee lock).
  uint16_t readColorAttributeU16(uint16_t attr_id);
  uint8_t readColorAttributeU8(uint16_t attr_id);
  void syncColorModeFromCallback(uint8_t color_mode);

  void lightChangedRgb();
  void lightChangedHsv();
  void lightChangedTemp();
  void lightChangedByMode();

  //callback function to be called on light change for RGB (State, R, G, B, Level)
  ZigbeeColorLightRgbCallback _on_light_change_rgb;
  //callback function to be called on light change for HSV (State, H, S, V, Level)
  ZigbeeColorLightHsvCallback _on_light_change_hsv;
  //callback function to be called on light change for TEMP (State, Level, Temperature)
  ZigbeeColorLightTempCallback _on_light_change_temp;

  bool _current_state;
  uint8_t _current_level;
  espRgbColor_t _current_color;
  espHsvColor_t _current_hsv;
  uint16_t _current_color_temperature;
  uint8_t _current_color_mode;

  uint16_t _color_capabilities;
};

#endif  // CONFIG_ZB_ENABLED
