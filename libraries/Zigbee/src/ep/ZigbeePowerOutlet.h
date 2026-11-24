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

/* Class of Zigbee On/Off Power outlet endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"

/**
 * @brief Smart power outlet endpoint for remote on/off control (Zigbee HA).
 */
class ZigbeePowerOutlet : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee power outlet endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeePowerOutlet(uint8_t endpoint);
  ~ZigbeePowerOutlet() {}

  /**
   * @brief Set a callback invoked when the power outlet state changes.
   * @param callback Function called with the new power state (true = on).
   */
  void onPowerOutletChange(void (*callback)(bool)) {
    _on_state_change = callback;
  }
  /**
   * @brief Restore the power outlet state and trigger any registered callbacks.
   */
  void restoreState() {
    stateChanged();
  }
  /**
   * @brief Set the power outlet state (on or off).
   * @param state true to turn on, false to turn off.
   * @return true if successful.
   */
  bool setState(bool state);
  /**
   * @brief Get the current power outlet state.
   * @return true if on, false if off.
   */
  bool getPowerOutletState() {
    return _current_state;
  }

private:
  void zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) override;
  //callback function to be called on outlet change
  void (*_on_state_change)(bool);
  void stateChanged();

  bool _current_state;
};

#endif  // CONFIG_ZB_ENABLED
