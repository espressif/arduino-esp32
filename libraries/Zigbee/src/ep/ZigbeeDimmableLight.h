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

/**
 * @brief Dimmable light endpoint with on/off and brightness control (Zigbee HA).
 */
class ZigbeeDimmableLight : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee dimmable light endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeDimmableLight(uint8_t endpoint);
  ~ZigbeeDimmableLight() {}

  /**
   * @brief Set a callback invoked when the light state or level changes.
   * @param callback Function called with state (true = on) and ZCL CurrentLevel (0-255).
   */
  void onLightChange(void (*callback)(bool, uint8_t)) {
    _on_light_change = callback;
  }
  /**
   * @brief Restore the light state and trigger any registered callbacks.
   */
  void restoreLight() {
    lightChanged();
  }

  /**
   * @brief Set only the light state without changing brightness.
   * @param state true to turn on, false to turn off.
   * @return true if successful.
   */
  bool setLightState(bool state);
  /**
   * @brief Set only the brightness level without changing on/off state.
   * @param level ZCL CurrentLevel (0-255, passed through unchanged; 255 is full brightness).
   * @return true if successful.
   */
  bool setLightLevel(uint8_t level);
  /**
   * @brief Set both the light state and brightness level.
   * @param state true to turn on, false to turn off.
   * @param level ZCL CurrentLevel (0-255, passed through unchanged; 255 is full brightness).
   * @return true if successful.
   */
  bool setLight(bool state, uint8_t level);

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

private:
  void zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) override;
  void lightChanged();
  // callback function to be called on light change (State, Level)
  void (*_on_light_change)(bool, uint8_t);

  bool _current_state;
  uint8_t _current_level;
};

#endif  // CONFIG_ZB_ENABLED
