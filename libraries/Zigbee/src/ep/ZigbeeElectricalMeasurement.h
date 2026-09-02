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

/* Class of Zigbee Electrical Measurement endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zcl/cluster/electrical_measurement_desc.h"

enum ZIGBEE_DC_MEASUREMENT_TYPE {
  ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE = 0x0001,
  ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT = 0x0002,
  ZIGBEE_DC_MEASUREMENT_TYPE_POWER = 0x0003,
};

enum ZIGBEE_AC_MEASUREMENT_TYPE {
  ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE = 0x0001,
  ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT = 0x0002,
  ZIGBEE_AC_MEASUREMENT_TYPE_POWER = 0x0003,
  ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR = 0x0004,
  ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY = 0x0005,
};

enum ZIGBEE_AC_PHASE_TYPE {
  ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC = 0x0000,
  ZIGBEE_AC_PHASE_TYPE_A = 0x0001,
  ZIGBEE_AC_PHASE_TYPE_B = 0x0002,
  ZIGBEE_AC_PHASE_TYPE_C = 0x0003,
};

class ZigbeeElectricalMeasurement : public ZigbeeEP {
public:
  ZigbeeElectricalMeasurement(uint8_t endpoint);
  ~ZigbeeElectricalMeasurement() {}

  /**
   * @brief DC measurement methods
   */
  // Add a DC measurement type
  bool addDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type);
  // Set the DC measurement value for the given measurement type
  bool setDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, int16_t value);
  // Set the DC min and max value for the given measurement type
  bool setDCMinMaxValue(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, int16_t min, int16_t max);
  // Set the DC multiplier and divisor for the given measurement type
  bool setDCMultiplierDivisor(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, uint16_t multiplier, uint16_t divisor);
  // Set the DC reporting interval for the given measurement type in seconds and delta (measurement change)
  bool setDCReporting(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, uint16_t min_interval, uint16_t max_interval, int16_t delta);
  // Report the DC measurement value for the given measurement type
  bool reportDC(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type);

  /**
   * @brief AC measurement methods
   */
  // Add an AC measurement type for selected phase type
  bool addACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type);
  // Set the AC measurement value for the given measurement type and phase type (uint16_t for voltage, current and frequency, int16_t for power)
  bool setACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type, int32_t value);
  // Set the AC min and max value for the given measurement type and phase type (uint16_t for voltage, current and frequency, int16_t for power)
  bool setACMinMaxValue(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type, int32_t min, int32_t max);
  // Set the AC multiplier and divisor for the given measurement type (common for all phases)
  bool setACMultiplierDivisor(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, uint16_t multiplier, uint16_t divisor);
  // Set the AC power factor for the given phase type (-100 to 100 %)
  bool setACPowerFactor(ZIGBEE_AC_PHASE_TYPE phase_type, int8_t power_factor);
  // Set the AC reporting interval for the given measurement type and phase type in seconds and delta (measurement change - uint16_t for voltage, current and frequency, int16_t for power)
  bool
    setACReporting(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type, uint16_t min_interval, uint16_t max_interval, int32_t delta);
  // Report the AC measurement value for the given measurement type and phase type
  bool reportAC(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type);

private:
  ezb_zcl_electrical_measurement_cluster_config_t _electrical_measurement_cfg;
  bool ac_volt_mult_div_set = false;
  bool ac_current_mult_div_set = false;
  bool ac_power_mult_div_set = false;

  int16_t _dc_measurement = 0;
  int16_t _dc_min = -32767;
  int16_t _dc_max = 32767;
  uint16_t _dc_multiplier = 1;
  uint16_t _dc_divisor = 1;

  uint16_t _ac_u16_measurement = 0;
  uint16_t _ac_u16_min = 0;
  uint16_t _ac_u16_max = 0xffff;
  int16_t _ac_s16_measurement = 0;
  int16_t _ac_s16_min = -32767;
  int16_t _ac_s16_max = 32767;
  int8_t _ac_power_factor = 100;
  uint16_t _ac_multiplier = 1;
  uint16_t _ac_divisor = 1;
};

#endif  // CONFIG_ZB_ENABLED
