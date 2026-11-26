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

/* Class of Zigbee Fan Control endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"

/**
 * @brief Fan speed and operation modes.
 */
enum ZigbeeFanMode {
  FAN_MODE_OFF = 0x00,    /**< Fan is off. */
  FAN_MODE_LOW = 0x01,    /**< Low speed. */
  FAN_MODE_MEDIUM = 0x02, /**< Medium speed. */
  FAN_MODE_HIGH = 0x03,   /**< High speed. */
  FAN_MODE_ON = 0x04,     /**< Fan is on (full speed). */
  FAN_MODE_AUTO = 0x05,   /**< Automatic mode. */
  FAN_MODE_SMART = 0x06,  /**< Smart mode. */
};

/**
 * @brief Sequences of fan modes available to the user.
 */
enum ZigbeeFanModeSequence {
  FAN_MODE_SEQUENCE_LOW_MED_HIGH = 0x00,      /**< Low -> Medium -> High. */
  FAN_MODE_SEQUENCE_LOW_HIGH = 0x01,          /**< Low -> High. */
  FAN_MODE_SEQUENCE_LOW_MED_HIGH_AUTO = 0x02, /**< Low -> Medium -> High -> Auto. */
  FAN_MODE_SEQUENCE_LOW_HIGH_AUTO = 0x03,     /**< Low -> High -> Auto. */
  FAN_MODE_SEQUENCE_ON_AUTO = 0x04,           /**< On -> Auto. */
};

/**
 * @brief Fan control endpoint for speed and sequence (Zigbee HA Fan Control cluster).
 */
class ZigbeeFanControl : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee fan control endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeFanControl(uint8_t endpoint);
  ~ZigbeeFanControl() {}

  /**
   * @brief Set the fan mode sequence and initialize fan control.
   * @param sequence Fan mode sequence to set.
   * @return true if successful.
   */
  bool setFanModeSequence(ZigbeeFanModeSequence sequence);

  /**
   * @brief Get the current fan mode.
   * @return Current fan mode.
   */
  ZigbeeFanMode getFanMode() {
    return _current_fan_mode;
  }

  /**
   * @brief Get the current fan mode sequence.
   * @return Current fan mode sequence.
   */
  ZigbeeFanModeSequence getFanModeSequence() {
    return _current_fan_mode_sequence;
  }

  /**
   * @brief Set a callback invoked when the fan mode changes.
   * @param callback Function called with the new fan mode.
   */
  void onFanModeChange(void (*callback)(ZigbeeFanMode mode)) {
    _on_fan_mode_change = callback;
  }

private:
  void zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) override;
  //callback function to be called on fan mode change
  void (*_on_fan_mode_change)(ZigbeeFanMode mode);
  void fanModeChanged();

  ZigbeeFanMode _current_fan_mode;
  ZigbeeFanModeSequence _current_fan_mode_sequence;
};

#endif  // CONFIG_ZB_ENABLED
