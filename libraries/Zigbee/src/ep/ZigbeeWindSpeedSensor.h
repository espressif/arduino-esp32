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

/* Class of Zigbee WindSpeed sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zcl/cluster/wind_speed_measurement_desc.h"

/**
 * @brief Wind speed sensor endpoint for measurement in m/s (Zigbee HA).
 */
class ZigbeeWindSpeedSensor : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee wind speed sensor endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeWindSpeedSensor(uint8_t endpoint);
  ~ZigbeeWindSpeedSensor() {}

  /**
   * @brief Set the wind speed measurement value in 0.01 m/s resolution.
   * @param value Wind speed in meters per second.
   * @return true if successful.
   */
  bool setWindSpeed(float value);

  /**
   * @brief Set the default (initial) wind speed in 0.01 m/s resolution.
   *
   * Used as the initial measured value in factory-reset mode. Must be called before
   * adding the endpoint to Zigbee. Only effective before commissioning.
   *
   * @param defaultValue Default wind speed in meters per second.
   * @return true if successful.
   */
  bool setDefaultValue(float defaultValue);

  /**
   * @brief Set the minimum and maximum measurement values.
   * @param min Minimum wind speed in meters per second.
   * @param max Maximum wind speed in meters per second.
   * @return true if successful.
   */
  bool setMinMaxValue(float min, float max);

  /**
   * @brief Set the tolerance value for measurements.
   * @param tolerance Tolerance in meters per second.
   * @return true if successful.
   */
  bool setTolerance(float tolerance);

  /**
   * @brief Set the reporting configuration for wind speed measurements.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum change required to trigger a report, in meters per second.
   * @return true if successful.
   */
  bool setReporting(uint16_t min_interval, uint16_t max_interval, float delta);
  /**
   * @brief Manually report the current wind speed value.
   * @return true if successful.
   */
  bool reportWindSpeed();

private:
  ezb_zcl_wind_speed_measurement_cluster_config_t _wind_speed_meas_cfg;
  uint16_t _tolerance;
};

#endif  // CONFIG_ZB_ENABLED
