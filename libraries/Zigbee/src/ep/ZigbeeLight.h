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
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeLight : public ZigbeeEP {
public:
  ZigbeeLight(uint8_t endpoint);
  ~ZigbeeLight() {}

  // Use to set a cb function to be called on light change
  void onLightChange(void (*callback)(bool)) {
    _on_light_change = callback;
  }
  // Use to restore light state
  void restoreLight() {
    lightChanged();
  }
  // Use to control light state
  bool setLight(bool state);
  // Use to get light state
  bool getLightState() {
    return _current_state;
  }

private:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;
  //callback function to be called on light change
  void (*_on_light_change)(bool);
  void lightChanged();

  bool _current_state;
};

#endif  // CONFIG_ZB_ENABLED
