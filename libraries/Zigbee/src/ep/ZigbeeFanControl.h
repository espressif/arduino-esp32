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

/* Class of Zigbee Pressure sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

// Custom Arduino-friendly enums for fan mode values
enum ZigbeeFanMode {
  FAN_MODE_OFF = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_OFF,
  FAN_MODE_LOW = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_LOW,
  FAN_MODE_MEDIUM = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_MEDIUM,
  FAN_MODE_HIGH = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_HIGH,
  FAN_MODE_ON = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_ON,
  FAN_MODE_AUTO = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_AUTO,
  FAN_MODE_SMART = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_SMART,
};

// Custom Arduino-friendly enums for fan mode sequence
enum ZigbeeFanModeSequence {
  FAN_MODE_SEQUENCE_LOW_MED_HIGH = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_SEQUENCE_LOW_MED_HIGH,
  FAN_MODE_SEQUENCE_LOW_HIGH = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_SEQUENCE_LOW_HIGH,
  FAN_MODE_SEQUENCE_LOW_MED_HIGH_AUTO = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_SEQUENCE_LOW_MED_HIGH_AUTO,
  FAN_MODE_SEQUENCE_LOW_HIGH_AUTO = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_SEQUENCE_LOW_HIGH_AUTO,
  FAN_MODE_SEQUENCE_ON_AUTO = ESP_ZB_ZCL_FAN_CONTROL_FAN_MODE_SEQUENCE_ON_AUTO,
};

class ZigbeeFanControl : public ZigbeeEP {
public:
  ZigbeeFanControl(uint8_t endpoint);
  ~ZigbeeFanControl() {}

  // Set the fan mode sequence value
  bool setFanModeSequence(ZigbeeFanModeSequence sequence);

  // Use to get fan mode
  ZigbeeFanMode getFanMode() {
    return _current_fan_mode;
  }

  // Use to get fan mode sequence
  ZigbeeFanModeSequence getFanModeSequence() {
    return _current_fan_mode_sequence;
  }

  // On fan mode change callback
  void onFanModeChange(void (*callback)(ZigbeeFanMode mode)) {
    _on_fan_mode_change = callback;
  }

private:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;
  //callback function to be called on fan mode change
  void (*_on_fan_mode_change)(ZigbeeFanMode mode);
  void fanModeChanged();

  ZigbeeFanMode _current_fan_mode;
  ZigbeeFanModeSequence _current_fan_mode_sequence;
};

#endif  // CONFIG_ZB_ENABLED
