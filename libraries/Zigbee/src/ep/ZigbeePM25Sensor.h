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

/* Class of Zigbee PM2.5 sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zcl/cluster/pm2_5_measurement_desc.h"

/**
 * @brief PM2.5 air quality sensor endpoint (Zigbee HA particulate matter measurement).
 */
class ZigbeePM25Sensor : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee PM2.5 sensor endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeePM25Sensor(uint8_t endpoint);
  ~ZigbeePM25Sensor() {}

  /**
   * @brief Set the PM2.5 concentration measurement value.
   * @param pm25 PM2.5 concentration in μg/m³.
   * @return true if successful.
   */
  bool setPM25(float pm25);

  /**
   * @brief Set the default (initial) PM2.5 value in μg/m³.
   *
   * Used as the initial measured value in factory-reset mode. Must be called before
   * adding the endpoint to Zigbee. Only effective before commissioning.
   *
   * @param defaultValue Default PM2.5 concentration in μg/m³.
   * @return true if successful.
   */
  bool setDefaultValue(float defaultValue);

  /**
   * @brief Set the minimum and maximum measurement values.
   * @param min Minimum PM2.5 concentration in μg/m³.
   * @param max Maximum PM2.5 concentration in μg/m³.
   * @return true if successful.
   */
  bool setMinMaxValue(float min, float max);

  /**
   * @brief Set the tolerance value for measurements.
   * @param tolerance Tolerance in μg/m³.
   * @return true if successful.
   */
  bool setTolerance(float tolerance);

  /**
   * @brief Set the reporting configuration for PM2.5 measurements.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum change required to trigger a report in μg/m³.
   * @return true if successful.
   */
  bool setReporting(uint16_t min_interval, uint16_t max_interval, float delta);

  /**
   * @brief Manually report the current PM2.5 concentration value.
   * @return true if successful.
   */
  bool report();

private:
  ezb_zcl_pm2_5_measurement_cluster_config_t _pm2_5_meas_cfg;
  float _tolerance;
};

#endif  // CONFIG_ZB_ENABLED
