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
 * @brief Matter dimmable light endpoint with on/off and brightness control.
 *
 * Arduino brightness input is 0–255; Matter CurrentLevel is 1–254 (255 is the
 * nullable null sentinel). Compatible with Home Assistant, Apple Home, Amazon
 * Alexa, and Google Home.
 */
class MatterDimmableLight : public MatterEndPoint {
public:
  /**
   * @brief Arduino full-scale brightness input (255). Matter stores this as 254.
   */
  static const uint8_t MAX_BRIGHTNESS = 255;

  /**
   * @brief Create a new Matter dimmable light endpoint.
   */
  MatterDimmableLight();
  /**
   * @brief Destroy the dimmable light endpoint object.
   */
  ~MatterDimmableLight();
  /**
   * @brief Initialize the Matter dimmable light endpoint.
   * @param initialState Initial on/off state (true = on, false = off, default false).
   * @param brightness Initial brightness (0–255, default 64 = 25%). 0 is stored as 1; 255 is stored as 254.
   * @return true if successful, false otherwise.
   */
  virtual bool begin(bool initialState = false, uint8_t brightness = 64);
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
   * @brief Get the current on/off state of the light.
   * @return true if the light is on, false if off.
   */
  bool getOnOff();
  /**
   * @brief Toggle the on/off state of the light.
   * @return true if successful, false otherwise.
   */
  bool toggle();

  /**
   * @brief Set the brightness level of the light.
   *
   * On/off is a separate attribute. 0 clamps to 1 and 255 clamps to 254.
   *
   * @param newBrightness Brightness (0–255).
   * @return true if successful, false otherwise.
   */
  bool setBrightness(uint8_t newBrightness);
  /**
   * @brief Get the current brightness level of the light.
   * @return Brightness level (1–254).
   */
  uint8_t getBrightness();

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
   * @brief Callback invoked when brightness changes.
   * @param newBrightness New brightness level (1–254).
   * @return true if the change was handled successfully.
   */
  using EndPointBrightnessCB = std::function<bool(uint8_t)>;
  /**
   * @brief Register a callback for brightness changes from the Matter controller.
   * @param onChangeCB Function to call when brightness changes.
   */
  void onChangeBrightness(EndPointBrightnessCB onChangeCB) {
    _onChangeBrightnessCB = onChangeCB;
  }

  /**
   * @brief Callback invoked when any light parameter changes.
   * @param newState New on/off state.
   * @param newBrightness New brightness level (1–254).
   * @return true if the change was handled successfully.
   */
  using EndPointCB = std::function<bool(bool, uint8_t)>;
  /**
   * @brief Register a callback for any parameter change from the Matter controller.
   * @param onChangeCB Function to call when state changes.
   */
  void onChange(EndPointCB onChangeCB) {
    _onChangeCB = onChangeCB;
  }

  /**
   * @brief Apply the current Matter Light internal state to the accessory.
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
  bool onOffState = false;      // default initial state is off, but it can be changed by begin(bool)
  uint8_t brightnessLevel = 0;  // default initial brightness is 0, but it can be changed by begin(bool, uint8_t)
  EndPointOnOffCB _onChangeOnOffCB = NULL;
  EndPointBrightnessCB _onChangeBrightnessCB = NULL;
  EndPointCB _onChangeCB = NULL;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
