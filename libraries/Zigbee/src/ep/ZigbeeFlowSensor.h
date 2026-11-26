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

/* Class of Zigbee Flow sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zcl/cluster/flow_measurement_desc.h"

/**
 * @brief Flow sensor endpoint for liquid and gas flow measurement (Zigbee HA).
 */
class ZigbeeFlowSensor : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee flow sensor endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeFlowSensor(uint8_t endpoint);
  ~ZigbeeFlowSensor() {}

  /**
   * @brief Set the flow rate measurement value in 0.1 m³/h resolution.
   * @param value Flow rate in m³/h.
   * @return true if successful.
   */
  bool setFlow(float value);

  /**
   * @brief Set the default (initial) flow value in 0.1 m³/h resolution.
   *
   * Used as the initial measured value in factory-reset mode. Must be called before
   * adding the endpoint to Zigbee. Only effective before commissioning.
   *
   * @param defaultValue Default flow rate in m³/h.
   * @return true if successful.
   */
  bool setDefaultValue(float defaultValue);

  /**
   * @brief Set the minimum and maximum measurement values.
   * @param min Minimum flow rate in m³/h.
   * @param max Maximum flow rate in m³/h.
   * @return true if successful.
   */
  bool setMinMaxValue(float min, float max);

  /**
   * @brief Set the tolerance value for measurements.
   * @param tolerance Tolerance in m³/h.
   * @return true if successful.
   */
  bool setTolerance(float tolerance);

  /**
   * @brief Set the reporting configuration for flow rate measurements.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum change required to trigger a report, in m³/h.
   * @return true if successful.
   */
  bool setReporting(uint16_t min_interval, uint16_t max_interval, float delta);

  /**
   * @brief Manually report the current flow rate value.
   * @return true if successful.
   */
  bool report();

private:
  ezb_zcl_flow_measurement_cluster_config_t _flow_meas_cfg;
  uint16_t _tolerance;
};

#endif  // CONFIG_ZB_ENABLED
