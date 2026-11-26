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
 * @brief Matter color temperature light with brightness and tunable white (no RGB wheel).
 *
 * Arduino brightness input is 0–255; Matter CurrentLevel is 1–254. Color
 * temperature is 100–500 mireds (higher mireds are warmer). Compatible with
 * Home Assistant, Apple Home, Amazon Alexa, and Google Home.
 */
class MatterColorTemperatureLight : public MatterEndPoint {
public:
  /**
   * @brief Arduino full-scale brightness input (255). Matter stores this as 254.
   */
  static const uint8_t MAX_BRIGHTNESS = 255;
  /**
   * @brief Maximum color temperature in mireds (500 = warm white, about 2000 K).
   */
  static const uint16_t MAX_COLOR_TEMPERATURE = 500;
  /**
   * @brief Minimum color temperature in mireds (100 = cool white, about 10000 K).
   */
  static const uint16_t MIN_COLOR_TEMPERATURE = 100;

  /**
   * @brief Create a new Matter color temperature light endpoint.
   */
  MatterColorTemperatureLight();
  /**
   * @brief Destroy the color temperature light endpoint object.
   */
  ~MatterColorTemperatureLight();
  /**
   * @brief Initialize the Matter color temperature light endpoint.
   * @param initialState Initial on/off state (default false = off).
   * @param brightness Initial brightness (0–255, default 64 = 25%). 0 is stored as 1; 255 is stored as 254.
   * @param colorTemperature Initial color temperature in mireds (100–500, default 370 = Soft White).
   * @return true if successful, false otherwise.
   */
  virtual bool begin(bool initialState = false, uint8_t brightness = 64, uint16_t colorTemperature = 370);
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
   * @brief Set the brightness level.
   * @param newBrightness Brightness (0–255). 0 clamps to 1 and 255 clamps to 254.
   * @return true if successful, false otherwise.
   */
  bool setBrightness(uint8_t newBrightness);
  /**
   * @brief Get the current brightness level.
   * @return Brightness level (1–254).
   */
  uint8_t getBrightness();

  /**
   * @brief Set the color temperature.
   *
   * Color temperature is measured in mireds (`mireds = 1000000 / Kelvin`).
   * Lower values (100–200) are cool white; higher values (400–500) are warm white.
   * Values outside 100–500 are clamped. ColorTempPhysicalMin/MaxMireds are advertised as 100/500.
   *
   * @param newTemperature Color temperature in mireds.
   * @return true if successful, false otherwise.
   */
  bool setColorTemperature(uint16_t newTemperature);
  /**
   * @brief Get the current color temperature.
   * @return Color temperature in mireds.
   */
  uint16_t getColorTemperature();

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
   * @brief Callback invoked when color temperature changes.
   * @param newTemperature New color temperature in mireds.
   * @return true if the change was handled successfully.
   */
  using EndPointTemperatureCB = std::function<bool(uint16_t)>;
  /**
   * @brief Register a callback for color temperature changes from the Matter controller.
   * @param onChangeCB Function to call when color temperature changes.
   */
  void onChangeColorTemperature(EndPointTemperatureCB onChangeCB) {
    _onChangeTemperatureCB = onChangeCB;
  }

  /**
   * @brief Callback invoked when any light parameter changes.
   * @param newState New on/off state.
   * @param newBrightness New brightness level (1–254).
   * @param newTemperature New color temperature in mireds.
   * @return true if the change was handled successfully.
   */
  using EndPointCB = std::function<bool(bool, uint8_t, uint16_t)>;
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
  uint8_t brightnessLevel = 0;         // default initial brightness is 0, but it can be changed by begin(bool, uint8_t)
  uint16_t colorTemperatureLevel = 0;  // default initial color temperature is 0, but it can be changed by begin(bool, uint8_t, uint16_t)
  EndPointOnOffCB _onChangeOnOffCB = NULL;
  EndPointBrightnessCB _onChangeBrightnessCB = NULL;
  EndPointTemperatureCB _onChangeTemperatureCB = NULL;
  EndPointCB _onChangeCB = NULL;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
