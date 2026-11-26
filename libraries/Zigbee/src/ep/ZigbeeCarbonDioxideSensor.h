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

/* Class of Zigbee Carbon Dioxide sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zcl/cluster/carbon_dioxide_measurement_desc.h"

/**
 * @brief Carbon dioxide sensor endpoint (Zigbee HA CO2 measurement).
 */
class ZigbeeCarbonDioxideSensor : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee CO2 sensor endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeCarbonDioxideSensor(uint8_t endpoint);
  ~ZigbeeCarbonDioxideSensor() {}

  /**
   * @brief Set the CO2 concentration measurement value.
   * @param carbon_dioxide CO2 concentration in ppm.
   * @return true if successful.
   */
  bool setCarbonDioxide(float carbon_dioxide);

  /**
   * @brief Set the default (initial) CO2 value in ppm.
   *
   * Used as the initial measured value in factory-reset mode. Must be called before
   * adding the endpoint to Zigbee. Only effective before commissioning.
   *
   * @param defaultValue Default CO2 concentration in ppm.
   * @return true if successful.
   */
  bool setDefaultValue(float defaultValue);

  /**
   * @brief Set the minimum and maximum measurement values.
   * @param min Minimum CO2 concentration in ppm.
   * @param max Maximum CO2 concentration in ppm.
   * @return true if successful.
   */
  bool setMinMaxValue(float min, float max);

  /**
   * @brief Set the tolerance value for measurements.
   * @param tolerance Tolerance in ppm.
   * @return true if successful.
   */
  bool setTolerance(float tolerance);

  /**
   * @brief Set the reporting configuration for CO2 measurements.
   *
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum change required to trigger a report in ppm.
   * @return true if successful.
   * @note Delta reporting is currently not supported by the carbon dioxide sensor.
   */
  bool setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta);

  /**
   * @brief Manually report the current CO2 concentration value.
   * @return true if successful.
   */
  bool report();

private:
  ezb_zcl_carbon_dioxide_measurement_cluster_config_t _carbon_dioxide_meas_cfg;
  float _tolerance;
};

#endif  // CONFIG_ZB_ENABLED
