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

/* Class of Zigbee Pressure sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zcl/cluster/pressure_measurement_desc.h"

/**
 * @brief Pressure sensor endpoint for atmospheric/barometric pressure in hPa (Zigbee HA).
 */
class ZigbeePressureSensor : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee pressure sensor endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeePressureSensor(uint8_t endpoint);
  ~ZigbeePressureSensor() {}

  /**
   * @brief Set the pressure measurement value.
   * @param value Pressure in hPa.
   * @return true if successful.
   */
  bool setPressure(int16_t value);

  /**
   * @brief Set the default (initial) pressure value in 1 hPa.
   *
   * Used as the initial measured value in factory-reset mode. Must be called before
   * adding the endpoint to Zigbee. Only effective before commissioning.
   *
   * @param defaultValue Default pressure in hPa.
   * @return true if successful.
   */
  bool setDefaultValue(int16_t defaultValue);

  /**
   * @brief Set the minimum and maximum measurement values.
   * @param min Minimum pressure in hPa.
   * @param max Maximum pressure in hPa.
   * @return true if successful.
   */
  bool setMinMaxValue(int16_t min, int16_t max);

  /**
   * @brief Set the tolerance value for measurements.
   * @param tolerance Tolerance in hPa.
   * @return true if successful.
   */
  bool setTolerance(uint16_t tolerance);

  /**
   * @brief Set the reporting configuration for pressure measurements.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum change required to trigger a report in hPa.
   * @return true if successful.
   */
  bool setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta);

  /**
   * @brief Manually report the current pressure value.
   * @return true if successful.
   */
  bool report();

private:
  ezb_zcl_pressure_measurement_cluster_config_t _pressure_meas_cfg;
  uint16_t _tolerance;
};

#endif  // CONFIG_ZB_ENABLED
