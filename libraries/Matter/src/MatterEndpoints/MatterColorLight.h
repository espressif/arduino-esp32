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

#pragma once
#include <sdkconfig.h>
#ifdef CONFIG_ESP_MATTER_ENABLE_DATA_MODEL

#include <Matter.h>
#include <MatterEndPoint.h>

/**
 * @brief Matter RGB color light with HSV control and no color temperature.
 *
 * Matter 1.5 has no Color Light (0x0102) device type, so this endpoint is
 * advertised as an Extended Color Light (0x010D) with Hue/Saturation and XY
 * only. Color Temperature is not in the data model. Use
 * @ref MatterEnhancedColorLight when the endpoint must also include color
 * temperature.
 *
 * Brightness is HSV value; there is no separate brightness or color-temperature
 * API. Changing the Color Control feature set after an upgrade requires
 * recommissioning so the controller reloads the data model.
 */
class MatterColorLight : public MatterEndPoint {
public:
  /**
   * @brief Create a new Matter color light endpoint.
   */
  MatterColorLight();
  /**
   * @brief Destroy the color light endpoint object.
   */
  ~MatterColorLight();
  /**
   * @brief Initialize the Matter color light endpoint.
   * @param initialState Initial on/off state (default false = off).
   * @param colorHSV Initial HSV color (default: red 12% intensity HSV(0, 254, 31)).
   * @return true if successful, false otherwise.
   */
  virtual bool begin(bool initialState = false, espHsvColor_t colorHSV = {0, 254, 31});
  /**
   * @brief Stop processing Matter light events.
   */
  void end();

  /**
   * @brief Set the on/off state of the light.
   * @param newState New state (true = on, false = off).
   * @return true if successful, false otherwise.
   */
  bool setOnOff(bool newState);
  /**
   * @brief Get the current on/off state.
   * @return true if the light is on, false if off.
   */
  bool getOnOff();
  /**
   * @brief Toggle the on/off state.
   * @return true if successful, false otherwise.
   */
  bool toggle();

  /**
   * @brief Set the color using RGB values.
   * @param rgbColor RGB color with red, green, and blue values (0–255 each).
   * @return true if successful, false otherwise.
   */
  bool setColorRGB(espRgbColor_t rgbColor);
  /**
   * @brief Get the current color as RGB values.
   * @return Current RGB color.
   */
  espRgbColor_t getColorRGB();
  /**
   * @brief Set the color using HSV values.
   * @param hsvColor HSV color: hue (0–254, where 254 is 360°), saturation (0–254),
   *                 and value/brightness (0–254). Do not pass degrees in the 0–360 range.
   * @return true if successful, false otherwise.
   */
  bool setColorHSV(espHsvColor_t hsvColor);
  /**
   * @brief Get the current color as HSV values.
   * @return Current HSV color.
   */
  espHsvColor_t getColorHSV();

  /**
   * @brief Callback invoked when the on/off state changes.
   * @param newState New on/off state.
   * @return true if the change was handled successfully.
   */
  using EndPointOnOffCB = std::function<bool(bool)>;
  /**
   * @brief Register a callback for on/off state changes from the Matter controller.
   * @param onChangeCB Function to call when on/off state changes.
   */
  void onChangeOnOff(EndPointOnOffCB onChangeCB) {
    _onChangeOnOffCB = onChangeCB;
  }
  /**
   * @brief Callback invoked when HSV color changes.
   * @param newColor New HSV color.
   * @return true if the change was handled successfully.
   */
  using EndPointRGBColorCB = std::function<bool(espHsvColor_t)>;
  /**
   * @brief Register a callback for color changes from the Matter controller.
   * @param onChangeCB Function to call when color changes.
   */
  void onChangeColorHSV(EndPointRGBColorCB onChangeCB) {
    _onChangeColorCB = onChangeCB;
  }

  /**
   * @brief Callback invoked when any light parameter changes.
   * @param newState New on/off state.
   * @param newColor New HSV color.
   * @return true if the change was handled successfully.
   */
  using EndPointCB = std::function<bool(bool, espHsvColor_t)>;
  /**
   * @brief Register a callback for any parameter change from the Matter controller.
   * @param onChangeCB Function to call when state changes.
   */
  void onChange(EndPointCB onChangeCB) {
    _onChangeCB = onChangeCB;
  }

  /**
   * @brief Apply the current Matter Light internal state to the physical light.
   *
   * Set a user callback with @ref onChange to handle the physical light.
   */
  void updateAccessory();

  /**
   * @brief Boolean conversion — current on/off state of the light.
   * @return true if the light is on.
   */
  operator bool();
  /**
   * @brief Assignment — turn the light on or off.
   * @param state true to turn on, false to turn off.
   */
  void operator=(bool state);

  /**
   * @brief Called by the Matter internal event processor when an attribute changes.
   *
   * Can be overridden by the application if necessary.
   *
   * @param endpoint_id Endpoint ID where the attribute changed.
   * @param cluster_id Cluster ID of the changed attribute.
   * @param attribute_id Attribute ID that changed.
   * @param val Pointer to the new attribute value.
   * @return true if the change was handled successfully.
   */
  bool attributeChangeCB(uint16_t endpoint_id, uint32_t cluster_id, uint32_t attribute_id, esp_matter_attr_val_t *val);

protected:
  bool started = false;
  bool onOffState = false;             // default initial state is off, but it can be changed by begin(bool)
  espHsvColor_t colorHSV = {0, 0, 0};  // default initial color HSV is black, but it can be changed by begin(bool, espHsvColor_t)
  EndPointOnOffCB _onChangeOnOffCB = NULL;
  EndPointRGBColorCB _onChangeColorCB = NULL;
  EndPointCB _onChangeCB = NULL;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
