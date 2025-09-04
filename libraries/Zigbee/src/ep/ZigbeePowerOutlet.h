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

/* Class of Zigbee On/Off Power outlet endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeePowerOutlet : public ZigbeeEP {
public:
  ZigbeePowerOutlet(uint8_t endpoint);
  ~ZigbeePowerOutlet() {}

  // Use to set a cb function to be called on outlet change
  void onPowerOutletChange(void (*callback)(bool)) {
    _on_state_change = callback;
  }
  // Use to restore outlet state
  void restoreState() {
    stateChanged();
  }
  // Use to control outlet state
  bool setState(bool state);
  // Use to get outlet state
  bool getPowerOutletState() {
    return _current_state;
  }

private:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;
  //callback function to be called on outlet change
  void (*_on_state_change)(bool);
  void stateChanged();

  bool _current_state;
};

#endif  // CONFIG_ZB_ENABLED
