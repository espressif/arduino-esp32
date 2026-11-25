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

/* Class of Zigbee Electrical Measurement endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zcl/cluster/electrical_measurement_desc.h"

/**
 * @brief DC electrical measurement types.
 */
enum ZIGBEE_DC_MEASUREMENT_TYPE {
  ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE = 0x0001, /**< DC voltage. */
  ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT = 0x0002, /**< DC current. */
  ZIGBEE_DC_MEASUREMENT_TYPE_POWER = 0x0003,   /**< DC power. */
};

/**
 * @brief AC electrical measurement types.
 */
enum ZIGBEE_AC_MEASUREMENT_TYPE {
  ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE = 0x0001,      /**< AC voltage. */
  ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT = 0x0002,      /**< AC current. */
  ZIGBEE_AC_MEASUREMENT_TYPE_POWER = 0x0003,        /**< AC power (signed). */
  ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR = 0x0004, /**< AC power factor. */
  ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY = 0x0005,    /**< AC frequency. */
};

/**
 * @brief AC phase selection for multi-phase measurements.
 */
enum ZIGBEE_AC_PHASE_TYPE {
  ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC = 0x0000, /**< Phase not specified. */
  ZIGBEE_AC_PHASE_TYPE_A = 0x0001,            /**< Phase A. */
  ZIGBEE_AC_PHASE_TYPE_B = 0x0002,            /**< Phase B. */
  ZIGBEE_AC_PHASE_TYPE_C = 0x0003,            /**< Phase C. */
};

/**
 * @brief Electrical measurement endpoint for AC/DC voltage, current, and power (Zigbee HA).
 */
class ZigbeeElectricalMeasurement : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee electrical measurement endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeElectricalMeasurement(uint8_t endpoint);
  ~ZigbeeElectricalMeasurement() {}

  /**
   * @brief Add a DC measurement type to the endpoint.
   * @param measurement_type DC measurement type (`ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE`,
   *                         `ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT`, or `ZIGBEE_DC_MEASUREMENT_TYPE_POWER`).
   * @return true if successful.
   */
  bool addDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type);
  /**
   * @brief Set the DC measurement value for a measurement type.
   * @param measurement_type DC measurement type.
   * @param value Measurement value.
   * @return true if successful.
   */
  bool setDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, int16_t value);
  /**
   * @brief Set the minimum and maximum values for a DC measurement type.
   * @param measurement_type DC measurement type.
   * @param min Minimum value.
   * @param max Maximum value.
   * @return true if successful.
   */
  bool setDCMinMaxValue(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, int16_t min, int16_t max);
  /**
   * @brief Set the multiplier and divisor for scaling DC measurements.
   * @param measurement_type DC measurement type.
   * @param multiplier Multiplier value.
   * @param divisor Divisor value.
   * @return true if successful.
   */
  bool setDCMultiplierDivisor(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, uint16_t multiplier, uint16_t divisor);
  /**
   * @brief Set the reporting configuration for DC measurements.
   * @param measurement_type DC measurement type.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum change required to trigger a report.
   * @return true if successful.
   */
  bool setDCReporting(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, uint16_t min_interval, uint16_t max_interval, int16_t delta);
  /**
   * @brief Manually report a DC measurement value.
   * @param measurement_type DC measurement type.
   * @return true if successful.
   */
  bool reportDC(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type);

  /**
   * @brief Add an AC measurement type for a specific phase.
   * @param measurement_type AC measurement type (voltage, current, power, power factor, or frequency).
   * @param phase_type Phase (`ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC`, `_A`, `_B`, or `_C`).
   * @return true if successful.
   */
  bool addACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type);
  /**
   * @brief Set the AC measurement value for a measurement type and phase.
   *
   * For power (`ZIGBEE_AC_MEASUREMENT_TYPE_POWER`) the value is `int16_t`; for other
   * types (voltage, current, power factor, frequency) it is `uint16_t`.
   *
   * @param measurement_type AC measurement type.
   * @param phase_type Phase type.
   * @param value Measurement value.
   * @return true if successful.
   */
  bool setACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type, int32_t value);
  /**
   * @brief Set the minimum and maximum values for an AC measurement type and phase.
   *
   * For power, min/max are `int16_t`; for other types they are `uint16_t`.
   *
   * @param measurement_type AC measurement type.
   * @param phase_type Phase type.
   * @param min Minimum value.
   * @param max Maximum value.
   * @return true if successful.
   */
  bool setACMinMaxValue(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type, int32_t min, int32_t max);
  /**
   * @brief Set the multiplier and divisor for scaling AC measurements (common for all phases).
   * @param measurement_type AC measurement type.
   * @param multiplier Multiplier value.
   * @param divisor Divisor value.
   * @return true if successful.
   */
  bool setACMultiplierDivisor(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, uint16_t multiplier, uint16_t divisor);
  /**
   * @brief Set the power factor for a specific phase.
   * @param phase_type Phase type.
   * @param power_factor Power factor (-100 to 100).
   * @return true if successful.
   */
  bool setACPowerFactor(ZIGBEE_AC_PHASE_TYPE phase_type, int8_t power_factor);
  /**
   * @brief Set the reporting configuration for AC measurements.
   *
   * For power, delta is `int16_t`; for other types it is `uint16_t`.
   *
   * @param measurement_type AC measurement type.
   * @param phase_type Phase type.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum change required to trigger a report.
   * @return true if successful.
   */
  bool
    setACReporting(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type, uint16_t min_interval, uint16_t max_interval, int32_t delta);
  /**
   * @brief Manually report an AC measurement value.
   * @param measurement_type AC measurement type.
   * @param phase_type Phase type.
   * @return true if successful.
   */
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
