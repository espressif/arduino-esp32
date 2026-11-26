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

/* Class of Zigbee Window Covering endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"

/**
 * @brief Window covering types supported by the Zigbee Window Covering cluster.
 */
enum ZigbeeWindowCoveringType {
  ROLLERSHADE = 0x00,                  /**< Lift support. */
  ROLLERSHADE_2_MOTOR = 0x01,          /**< Lift support. */
  ROLLERSHADE_EXTERIOR = 0x02,         /**< Lift support. */
  ROLLERSHADE_EXTERIOR_2_MOTOR = 0x03, /**< Lift support. */
  DRAPERY = 0x04,                      /**< Lift support. */
  AWNING = 0x05,                       /**< Lift support. */
  SHUTTER = 0x06,                      /**< Tilt support. */
  BLIND_TILT_ONLY = 0x07,              /**< Tilt support. */
  BLIND_LIFT_AND_TILT = 0x08,          /**< Lift and tilt support. */
  PROJECTOR_SCREEN = 0x09,             /**< Lift support. */
};

/**
 * @brief Window covering endpoint for motorized blinds, shades, and similar devices (Zigbee HA).
 */
class ZigbeeWindowCovering : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee window covering endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeWindowCovering(uint8_t endpoint);
  ~ZigbeeWindowCovering() {}

  /**
   * @brief Set a callback invoked when the covering opens.
   * @param callback Function to call when the covering opens.
   */
  void onOpen(void (*callback)()) {
    _on_open = callback;
  }
  /**
   * @brief Set a callback invoked when the covering closes.
   * @param callback Function to call when the covering closes.
   */
  void onClose(void (*callback)()) {
    _on_close = callback;
  }
  /**
   * @brief Set a callback invoked when lift percentage changes.
   * @param callback Function called with the new lift percentage.
   */
  void onGoToLiftPercentage(void (*callback)(uint8_t)) {
    _on_go_to_lift_percentage = callback;
  }
  /**
   * @brief Set a callback invoked when tilt percentage changes.
   * @param callback Function called with the new tilt percentage.
   */
  void onGoToTiltPercentage(void (*callback)(uint8_t)) {
    _on_go_to_tilt_percentage = callback;
  }
  /**
   * @brief Set a callback invoked when the covering stops.
   * @param callback Function to call when the covering stops.
   */
  void onStop(void (*callback)()) {
    _on_stop = callback;
  }

  /**
   * @brief Set the window covering lift position.
   * @param lift_position Lift position.
   * @return true if successful.
   */
  bool setLiftPosition(uint16_t lift_position);
  /**
   * @brief Set the window covering lift position as a percentage.
   * @param lift_percentage Lift percentage (0-100, where 0 is fully closed and 100 is fully open).
   * @return true if successful.
   */
  bool setLiftPercentage(uint8_t lift_percentage);
  /**
   * @brief Set the window covering tilt position.
   * @param tilt_position Tilt position in degrees.
   * @return true if successful.
   */
  bool setTiltPosition(uint16_t tilt_position);
  /**
   * @brief Set the window covering tilt position as a percentage.
   * @param tilt_percentage Tilt percentage (0-100).
   * @return true if successful.
   */
  bool setTiltPercentage(uint8_t tilt_percentage);

  /**
   * @brief Set the window covering type.
   * @param covering_type Window covering type (see `ZigbeeWindowCoveringType`).
   * @return true if successful.
   */
  bool setCoveringType(ZigbeeWindowCoveringType covering_type);

  /**
   * @brief Set the window covering configuration status.
   * @param operational Operational status.
   * @param online Online status.
   * @param commands_reversed Commands reversed flag.
   * @param lift_closed_loop Lift closed-loop flag.
   * @param tilt_closed_loop Tilt closed-loop flag.
   * @param lift_encoder_controlled Lift encoder-controlled flag.
   * @param tilt_encoder_controlled Tilt encoder-controlled flag.
   * @return true if successful.
   */
  bool setConfigStatus(
    bool operational, bool online, bool commands_reversed, bool lift_closed_loop, bool tilt_closed_loop, bool lift_encoder_controlled,
    bool tilt_encoder_controlled
  );

  /**
   * @brief Set the window covering operation mode.
   * @param motor_reversed Motor reversed flag.
   * @param calibration_mode Calibration mode flag.
   * @param maintenance_mode Maintenance mode flag.
   * @param leds_on LEDs on flag.
   * @return true if successful.
   */
  bool setMode(bool motor_reversed, bool calibration_mode, bool maintenance_mode, bool leds_on);

  /**
   * @brief Set the motion limits for the window covering.
   * @param installed_open_limit_lift Installed open limit for lift.
   * @param installed_closed_limit_lift Installed closed limit for lift.
   * @param installed_open_limit_tilt Installed open limit for tilt.
   * @param installed_closed_limit_tilt Installed closed limit for tilt.
   * @return true if successful.
   */
  bool setLimits(
    uint16_t installed_open_limit_lift, uint16_t installed_closed_limit_lift, uint16_t installed_open_limit_tilt, uint16_t installed_closed_limit_tilt
  );

private:
  void zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) override;
  void zbWindowCoveringMovementCmd(const ezb_zcl_window_covering_movement_message_t *message) override;

  void open();
  void close();
  void goToLiftPercentage(uint8_t);
  void goToTiltPercentage(uint8_t);
  void stop();

  // callback function to be called on lift percentage change (lift percentage)
  void (*_on_open)();
  void (*_on_close)();
  void (*_on_go_to_lift_percentage)(uint8_t);
  void (*_on_go_to_tilt_percentage)(uint8_t);
  void (*_on_stop)();

  uint8_t _covering_type;
  uint8_t _config_status;
  uint8_t _mode;

  // Widows covering lift attributes
  uint8_t _current_lift_percentage;
  uint16_t _current_lift_position;
  uint16_t _installed_open_limit_lift;
  uint16_t _installed_closed_limit_lift;
  uint16_t _physical_closed_limit_lift;

  // Windows covering tilt attributes
  uint8_t _current_tilt_percentage;
  uint16_t _current_tilt_position;
  uint16_t _installed_open_limit_tilt;
  uint16_t _installed_closed_limit_tilt;
  uint16_t _physical_closed_limit_tilt;
};

#endif  // CONFIG_ZB_ENABLED
