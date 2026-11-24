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
 * @brief Matter on/off plugin unit (power outlet / relay) endpoint.
 *
 * Simple on/off control for power outlets, relays, and other on/off devices.
 * Compatible with Home Assistant, Apple Home, Amazon Alexa, and Google Home.
 */
class MatterOnOffPlugin : public MatterEndPoint {
public:
  /**
   * @brief Create a new Matter on/off plugin endpoint.
   */
  MatterOnOffPlugin();
  /**
   * @brief Destroy the on/off plugin endpoint object.
   */
  ~MatterOnOffPlugin();
  /**
   * @brief Initialize the Matter on/off plugin endpoint.
   * @param initialState Initial on/off state (true = on, false = off, default false).
   * @return true if successful, false otherwise.
   */
  virtual bool begin(bool initialState = false);
  /**
   * @brief Stop processing Matter plugin events.
   */
  void end();

  /**
   * @brief Set the on/off state of the plugin.
   * @param newState New state (true = on, false = off).
   * @return true if successful, false otherwise.
   */
  bool setOnOff(bool newState);
  /**
   * @brief Get the current on/off state of the plugin.
   * @return true if the plugin is on, false if off.
   */
  bool getOnOff();
  /**
   * @brief Toggle the on/off state of the plugin.
   * @return true if successful, false otherwise.
   */
  bool toggle();

  /**
   * @brief Callback invoked when the plugin state changes.
   * @param newState New on/off state (true = on, false = off).
   * @return true if the change was handled successfully.
   */
  using EndPointCB = std::function<bool(bool)>;
  /**
   * @brief Register a callback for plugin state changes from the Matter controller.
   * @param onChangeCB Function to call when state changes.
   */
  void onChange(EndPointCB onChangeCB) {
    _onChangeCB = onChangeCB;
  }

  /**
   * @brief Register a callback for on/off state changes (same as @ref onChange).
   * @param onChangeCB Function to call when state changes.
   */
  void onChangeOnOff(EndPointCB onChangeCB) {
    _onChangeOnOffCB = onChangeCB;
  }

  /**
   * @brief Apply the current Matter Plugin internal state to the accessory.
   *
   * Calls the registered @ref onChange callback with the current state.
   */
  void updateAccessory();

  /**
   * @brief Boolean conversion — current on/off state of the plugin.
   * @return true if the plugin is on.
   */
  operator bool();
  /**
   * @brief Assignment — turn the plugin on or off.
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
  bool onOffState = false;  // default initial state is off, but it can be changed by begin(bool)
  EndPointCB _onChangeCB = NULL;
  EndPointCB _onChangeOnOffCB = NULL;
};
#endif /* CONFIG_ESP_MATTER_ENABLE_DATA_MODEL */
