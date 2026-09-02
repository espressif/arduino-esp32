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

/* Class of Zigbee Window Covering endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"

// Window covering types supported by Zigbee Window Covering cluster
enum ZigbeeWindowCoveringType {
  ROLLERSHADE = 0x00,                          // LIFT support
  ROLLERSHADE_2_MOTOR = 0x01,           // LIFT support
  ROLLERSHADE_EXTERIOR = 0x02,        // LIFT support
  ROLLERSHADE_EXTERIOR_2_MOTOR = 0x03,  // LIFT support
  DRAPERY = 0x04,                                  // LIFT support
  AWNING = 0x05,                                    // LIFT support
  SHUTTER = 0x06,                                  // TILT support
  BLIND_TILT_ONLY = 0x07,             // TILT support
  BLIND_LIFT_AND_TILT = 0x08,     // LIFT and TILT support
  PROJECTOR_SCREEN = 0x09,                // LIFT support
};

class ZigbeeWindowCovering : public ZigbeeEP {
public:
  ZigbeeWindowCovering(uint8_t endpoint);
  ~ZigbeeWindowCovering() {}

  // Set the callback functions for the window covering commands
  void onOpen(void (*callback)()) {
    _on_open = callback;
  }
  void onClose(void (*callback)()) {
    _on_close = callback;
  }
  void onGoToLiftPercentage(void (*callback)(uint8_t)) {
    _on_go_to_lift_percentage = callback;
  }
  void onGoToTiltPercentage(void (*callback)(uint8_t)) {
    _on_go_to_tilt_percentage = callback;
  }
  void onStop(void (*callback)()) {
    _on_stop = callback;
  }

  // Set the window covering position in centimeters or percentage (0-100)
  bool setLiftPosition(uint16_t lift_position);
  bool setLiftPercentage(uint8_t lift_percentage);
  bool setTiltPosition(uint16_t tilt_position);
  bool setTiltPercentage(uint8_t tilt_percentage);

  // Set the window covering type (see ZigbeeWindowCoveringType)
  bool setCoveringType(ZigbeeWindowCoveringType covering_type);

  // Set window covering config/status, for more info see ezb_zcl_window_covering_server_config_status_t
  bool setConfigStatus(
    bool operational, bool online, bool commands_reversed, bool lift_closed_loop, bool tilt_closed_loop, bool lift_encoder_controlled,
    bool tilt_encoder_controlled
  );

  // Set configuration mode of window covering, for more info see ezb_zcl_window_covering_server_mode_t
  bool setMode(bool motor_reversed, bool calibration_mode, bool maintenance_mode, bool leds_on);

  // Set limits of motion, for more info see ezb_zcl_window_covering_server_attr_t
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
