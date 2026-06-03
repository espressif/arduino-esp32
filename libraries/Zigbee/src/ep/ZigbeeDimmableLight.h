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
#include "ezbee/zha.h"
#include "ezbee/zcl/cluster/on_off_desc.h"
#include "ezbee/zcl/cluster/level_desc.h"

class ZigbeeDimmableLight : public ZigbeeEP {
public:
  ZigbeeDimmableLight(uint8_t endpoint);
  ~ZigbeeDimmableLight() {}

  void onLightChange(void (*callback)(bool, uint8_t)) {
    _on_light_change = callback;
  }
  void restoreLight() {
    lightChanged();
  }

  bool setLightState(bool state);
  bool setLightLevel(uint8_t level);
  bool setLight(bool state, uint8_t level);

  bool getLightState() {
    return _current_state;
  }
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
