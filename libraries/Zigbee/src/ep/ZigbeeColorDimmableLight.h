// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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

// Color capabilities bit flags (matching ZCL spec) - can be combined with bitwise OR
static constexpr uint16_t ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION = (1 << 0);  // Bit 0: Hue/saturation supported
static constexpr uint16_t ZIGBEE_COLOR_CAPABILITY_ENHANCED_HUE = (1 << 1);    // Bit 1: Enhanced hue supported
static constexpr uint16_t ZIGBEE_COLOR_CAPABILITY_COLOR_LOOP = (1 << 2);      // Bit 2: Color loop supported
static constexpr uint16_t ZIGBEE_COLOR_CAPABILITY_X_Y = (1 << 3);             // Bit 3: X/Y supported
static constexpr uint16_t ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP = (1 << 4);      // Bit 4: Color temperature supported

/**
 * @brief Active color mode (matching ZCL Color Control cluster).
 */
enum ZigbeeColorMode {
  ZIGBEE_COLOR_MODE_HUE_SATURATION = 0x00,                              /**< CurrentHue and CurrentSaturation. */
  ZIGBEE_COLOR_MODE_CURRENT_X_Y = 0x01, /**< CurrentX and CurrentY. */  // codespell:ignore currenty
  ZIGBEE_COLOR_MODE_TEMPERATURE = 0x02,                                 /**< ColorTemperature. */
};

/**
 * @brief Callback for RGB/XY color mode changes.
 * @param state Light state (true = on).
 * @param red Red component (0-255).
 * @param green Green component (0-255).
 * @param blue Blue component (0-255).
 * @param level ZCL CurrentLevel (0-255).
 */
using ZigbeeColorLightRgbCallback = void (*)(bool state, uint8_t red, uint8_t green, uint8_t blue, uint8_t level);
/**
 * @brief Callback for HSV color mode changes.
 * @param state Light state (true = on).
 * @param hue Hue (0-254).
 * @param saturation Saturation (0-254).
 * @param value ZCL CurrentLevel (0-255).
 */
using ZigbeeColorLightHsvCallback = void (*)(bool state, uint8_t hue, uint8_t saturation, uint8_t value);
/**
 * @brief Callback for color-temperature mode changes.
 * @param state Light state (true = on).
 * @param level ZCL CurrentLevel (0-255).
 * @param color_temperature Color temperature in mireds.
 */
using ZigbeeColorLightTempCallback = void (*)(bool state, uint8_t level, uint16_t color_temperature);

/**
 * @brief Color dimmable light endpoint with RGB/XY, HSV, and color-temperature modes (Zigbee HA).
 *
 * By default only XY (RGB) mode is enabled. Call `setLightColorCapabilities()` before
 * starting Zigbee to enable additional modes. Set methods validate that the required
 * capability is enabled and return false otherwise. Color mode switches automatically
 * based on which attributes are set or received.
 */
class ZigbeeColorDimmableLight : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee color dimmable light endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeColorDimmableLight(uint8_t endpoint);
  ~ZigbeeColorDimmableLight() {}

  /**
   * @brief Configure which color modes this light supports.
   *
   * Must be called before `Zigbee.addEndpoint()`. After registration, runtime changes
   * use `setClusterAttribute()`. Combine flags with bitwise OR. Default: XY (RGB) only.
   *
   * @param capabilities Bit flags (`ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION`,
   *                     `ZIGBEE_COLOR_CAPABILITY_X_Y`, `ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP`,
   *                     `ZIGBEE_COLOR_CAPABILITY_ENHANCED_HUE`, `ZIGBEE_COLOR_CAPABILITY_COLOR_LOOP`).
   * @return true if successful.
   */
  bool setLightColorCapabilities(uint16_t capabilities);

  /**
   * @brief Set the legacy RGB light-change callback.
   *
   * Use `onLightChangeRgb()` instead.
   *
   * @param callback Function called with state, red, green, blue, and ZCL CurrentLevel (0-255).
   */
  [[deprecated("Use onLightChangeRgb() instead. This will be removed in a future major version.")]]
  void onLightChange(ZigbeeColorLightRgbCallback callback) {
    _on_light_change_rgb = callback;
  }
  /**
   * @brief Set the callback for RGB/XY color mode changes.
   * @param callback Function called with state, red, green, blue, and ZCL CurrentLevel (0-255).
   */
  void onLightChangeRgb(ZigbeeColorLightRgbCallback callback) {
    _on_light_change_rgb = callback;
  }
  /**
   * @brief Set the callback for HSV color mode changes.
   * @param callback Function called with state, hue (0-254), saturation (0-254), and ZCL CurrentLevel (0-255).
   */
  void onLightChangeHsv(ZigbeeColorLightHsvCallback callback) {
    _on_light_change_hsv = callback;
  }
  /**
   * @brief Set the callback for color-temperature mode changes.
   * @param callback Function called with state, ZCL CurrentLevel (0-255), and temperature in mireds.
   */
  void onLightChangeTemp(ZigbeeColorLightTempCallback callback) {
    _on_light_change_temp = callback;
  }
  /**
   * @brief Restore the light to its last known state using the callback for the current color mode.
   */
  void restoreLight() {
    lightChangedByMode();
  }

  /**
   * @brief Set the light on/off state.
   * @param state true to turn on, false to turn off.
   * @return true if successful.
   */
  bool setLightState(bool state);
  /**
   * @brief Set the light brightness level.
   * @param level ZCL CurrentLevel (0-255, passed through unchanged; 255 is full brightness).
   * @return true if successful.
   */
  bool setLightLevel(uint8_t level);
  /**
   * @brief Set the light color using RGB values.
   *
   * Requires `ZIGBEE_COLOR_CAPABILITY_X_Y`. Switches to XY mode.
   *
   * @param red Red component (0-255).
   * @param green Green component (0-255).
   * @param blue Blue component (0-255).
   * @return true if successful, false if XY capability is not enabled.
   */
  bool setLightColor(uint8_t red, uint8_t green, uint8_t blue);
  /**
   * @brief Set the light color using an RGB structure.
   *
   * Requires `ZIGBEE_COLOR_CAPABILITY_X_Y`. Switches to XY mode.
   *
   * @param rgb_color RGB color structure.
   * @return true if successful, false if XY capability is not enabled.
   */
  bool setLightColor(espRgbColor_t rgb_color);
  /**
   * @brief Set the light color using HSV values.
   *
   * Requires `ZIGBEE_COLOR_CAPABILITY_HUE_SATURATION`. Switches to HSV mode.
   *
   * @param hsv_color HSV color structure.
   * @return true if successful, false if HSV capability is not enabled.
   */
  bool setLightColor(espHsvColor_t hsv_color);
  /**
   * @brief Set the light color temperature in mireds.
   *
   * Requires `ZIGBEE_COLOR_CAPABILITY_COLOR_TEMP`. Switches to temperature mode.
   * mireds = 1000000 / Kelvin (e.g. 250 mireds ≈ 4000 K).
   *
   * @param color_temperature Color temperature in mireds.
   * @return true if successful, false if color-temperature capability is not enabled.
   */
  bool setLightColorTemperature(uint16_t color_temperature);
  /**
   * @brief Set all light parameters at once (RGB mode).
   *
   * Requires `ZIGBEE_COLOR_CAPABILITY_X_Y`. Switches to XY mode.
   *
   * @param state Light state (true/false).
   * @param level ZCL CurrentLevel (0-255).
   * @param red Red component (0-255).
   * @param green Green component (0-255).
   * @param blue Blue component (0-255).
   * @return true if successful, false if XY capability is not enabled.
   */
  bool setLight(bool state, uint8_t level, uint8_t red, uint8_t green, uint8_t blue);
  /**
   * @brief Set the minimum and maximum color temperature range supported by the hardware.
   * @param min_temp Minimum color temperature in mireds.
   * @param max_temp Maximum color temperature in mireds.
   * @return true if successful.
   */
  bool setLightColorTemperatureRange(uint16_t min_temp, uint16_t max_temp);

  /**
   * @brief Get the current light state.
   * @return true if on, false if off.
   */
  bool getLightState() {
    return _current_state;
  }
  /**
   * @brief Get the current brightness level.
   * @return ZCL CurrentLevel (0-255).
   */
  uint8_t getLightLevel() {
    return _current_level;
  }
  /**
   * @brief Get the current RGB color.
   * @return Current RGB color structure.
   */
  espRgbColor_t getLightColor() {
    return _current_color;
  }
  /**
   * @brief Get the current red component.
   * @return Red (0-255).
   */
  uint8_t getLightRed() {
    return _current_color.r;
  }
  /**
   * @brief Get the current green component.
   * @return Green (0-255).
   */
  uint8_t getLightGreen() {
    return _current_color.g;
  }
  /**
   * @brief Get the current blue component.
   * @return Blue (0-255).
   */
  uint8_t getLightBlue() {
    return _current_color.b;
  }
  /**
   * @brief Get the current color temperature.
   * @return Color temperature in mireds.
   */
  uint16_t getLightColorTemperature() {
    return _current_color_temperature;
  }
  /**
   * @brief Get the current active color mode.
   * @return `ZIGBEE_COLOR_MODE_HUE_SATURATION` (0x00), `ZIGBEE_COLOR_MODE_CURRENT_X_Y` (0x01),
   *         or `ZIGBEE_COLOR_MODE_TEMPERATURE` (0x02).
   */
  uint8_t getLightColorMode() {
    return _current_color_mode;
  }
  /**
   * @brief Get the current hue value (HSV mode).
   * @return Hue (0-254).
   */
  uint8_t getLightColorHue() {
    return _current_hsv.h;
  }
  /**
   * @brief Get the current saturation value (HSV mode).
   * @return Saturation (0-254).
   */
  uint8_t getLightColorSaturation() {
    return _current_hsv.s;
  }
  /**
   * @brief Get the currently configured color capabilities.
   * @return Color capabilities bit flags.
   */
  uint16_t getLightColorCapabilities() {
    return _color_capabilities;
  }

private:
  void zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) override;
  void addOnOffEffectAttributes();
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
  void syncOnOffFromDataModel();
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
  uint8_t _on_off_value;
  uint8_t _current_level;
  espRgbColor_t _current_color;
  espHsvColor_t _current_hsv;
  uint16_t _current_color_temperature;
  uint8_t _current_color_mode;

  uint16_t _color_capabilities;
  uint16_t _color_temp_physical_min_mireds;
  uint16_t _color_temp_physical_max_mireds;
};

#endif  // CONFIG_ZB_ENABLED
