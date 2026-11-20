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

/* Zigbee HA thermostat server (Thermostat cluster 0x0201). ZigbeeThermostat is the coordinator client. */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"

// Thermostat SystemMode (0x001C). 0x02 is not a mode.
enum ZigbeeHVACSystemMode {
  HVAC_SYSTEM_MODE_OFF = 0x00,
  HVAC_SYSTEM_MODE_AUTO = 0x01,
  HVAC_SYSTEM_MODE_COOL = 0x03,
  HVAC_SYSTEM_MODE_HEAT = 0x04,
  HVAC_SYSTEM_MODE_EMERGENCY_HEAT = 0x05,
  HVAC_SYSTEM_MODE_PRECOOLING = 0x06,
  HVAC_SYSTEM_MODE_FAN_ONLY = 0x07,
  HVAC_SYSTEM_MODE_DRY = 0x08,
  HVAC_SYSTEM_MODE_SLEEP = 0x09,
};

// Thermostat ControlSequenceOfOperation (0x001B).
enum ZigbeeHVACControlSequence {
  HVAC_SEQ_COOLING_ONLY = 0x00,
  HVAC_SEQ_COOLING_WITH_REHEAT = 0x01,
  HVAC_SEQ_HEATING_ONLY = 0x02,
  HVAC_SEQ_HEATING_WITH_REHEAT = 0x03,
  HVAC_SEQ_COOLING_AND_HEATING = 0x04,
  HVAC_SEQ_COOLING_AND_HEATING_WITH_REHEAT = 0x05,
};

// ThermostatRunningState (0x0029) bitmap. Combine with bitwise OR.
enum ZigbeeHVACRunningState {
  HVAC_RUNNING_OFF = 0x0000,
  HVAC_RUNNING_HEAT = 0x0001,
  HVAC_RUNNING_COOL = 0x0002,
  HVAC_RUNNING_FAN = 0x0004,
  HVAC_RUNNING_HEAT_2ND = 0x0008,
  HVAC_RUNNING_COOL_2ND = 0x0010,
  HVAC_RUNNING_FAN_2ND = 0x0020,
  HVAC_RUNNING_FAN_3RD = 0x0040,
};

class ZigbeeHVAC : public ZigbeeEP {
public:
  ZigbeeHVAC(uint8_t endpoint);
  ~ZigbeeHVAC() {}

  // Call after Zigbee.role(). Works before and after Zigbee.addEndpoint().
  // Temperatures are degrees Celsius. Occupied cooling must stay at least 2.5°C above occupied heating.
  bool setControlSequence(ZigbeeHVACControlSequence sequence);
  bool setSystemMode(ZigbeeHVACSystemMode mode);
  bool setOccupiedHeatingSetpoint(float celsius);
  bool setOccupiedCoolingSetpoint(float celsius);
  bool setLocalTemperature(float celsius);
  bool setRunningState(uint16_t running_state);

  ZigbeeHVACSystemMode getSystemMode() {
    return (ZigbeeHVACSystemMode)_system_mode;
  }
  ZigbeeHVACControlSequence getControlSequence() {
    return (ZigbeeHVACControlSequence)_control_sequence;
  }
  float getOccupiedHeatingSetpoint() {
    return _occupied_heating / 100.0f;
  }
  float getOccupiedCoolingSetpoint() {
    return _occupied_cooling / 100.0f;
  }
  float getLocalTemperature() {
    return _local_temperature / 100.0f;
  }
  uint16_t getRunningState() {
    return _running_state;
  }

  // Called when the coordinator writes SystemMode or an occupied setpoint, or sends SetpointRaiseOrLower.
  void onHVACChange(void (*callback)(ZigbeeHVACSystemMode mode, float heating_setpoint, float cooling_setpoint)) {
    _on_hvac_change = callback;
  }

private:
  void zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) override;
  void zbThermostatSetpoint(uint8_t mode, int16_t heat_centi, int16_t cool_centi) override;

  bool writeAttr(const char *api, uint16_t attr_id, void *value);
  void hvacChanged();

  void (*_on_hvac_change)(ZigbeeHVACSystemMode mode, float heating_setpoint, float cooling_setpoint);
  uint8_t _system_mode;
  uint8_t _control_sequence;
  int16_t _occupied_heating;
  int16_t _occupied_cooling;
  int16_t _local_temperature;
  int8_t _min_dead_band;
  uint16_t _running_state;
};

#endif  // CONFIG_ZB_ENABLED
