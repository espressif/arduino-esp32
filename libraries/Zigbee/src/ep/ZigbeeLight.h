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
 * @brief Simple on/off light endpoint (Zigbee HA basic lighting control).
 */
class ZigbeeLight : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee light endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeLight(uint8_t endpoint);
  ~ZigbeeLight() {}

  /**
   * @brief Set a callback invoked when the light state changes.
   * @param callback Function called with the new light state (true = on).
   */
  void onLightChange(void (*callback)(bool)) {
    _on_light_change = callback;
  }
  /**
   * @brief Restore the light state and trigger any registered callbacks.
   */
  void restoreLight() {
    lightChanged();
  }
  /**
   * @brief Set the light state (on or off).
   * @param state true to turn on, false to turn off.
   * @return true if successful.
   */
  bool setLight(bool state);
  /**
   * @brief Get the current light state.
   * @return true if on, false if off.
   */
  bool getLightState() {
    return _current_state;
  }

private:
  void zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) override;
  //callback function to be called on light change
  void (*_on_light_change)(bool);
  void lightChanged();

  bool _current_state;
};

#endif  // CONFIG_ZB_ENABLED
