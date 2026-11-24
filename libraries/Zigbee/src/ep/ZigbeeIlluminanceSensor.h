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

/* Class of Zigbee Illuminance sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zcl/cluster/illuminance_measurement_desc.h"

/**
 * @brief Illuminance sensor endpoint for ambient light measurement in lux (Zigbee HA).
 */
class ZigbeeIlluminanceSensor : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee illuminance sensor endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeIlluminanceSensor(uint8_t endpoint);
  ~ZigbeeIlluminanceSensor() {}

  /**
   * @brief Set the illuminance measurement value.
   * @param value Illuminance in lux.
   * @return true if successful.
   */
  bool setIlluminance(uint16_t value);

  /**
   * @brief Set the default (initial) illuminance value.
   *
   * Used as the initial measured value in factory-reset mode. Must be called before
   * adding the endpoint to Zigbee. Only effective before commissioning.
   *
   * @param defaultValue Default illuminance in lux.
   * @return true if successful.
   */
  bool setDefaultValue(uint16_t defaultValue);

  /**
   * @brief Set the minimum and maximum measurement values.
   * @param min Minimum illuminance in lux.
   * @param max Maximum illuminance in lux.
   * @return true if successful.
   */
  bool setMinMaxValue(uint16_t min, uint16_t max);

  /**
   * @brief Set the tolerance value for measurements.
   * @param tolerance Tolerance in lux.
   * @return true if successful.
   */
  bool setTolerance(uint16_t tolerance);

  /**
   * @brief Set the reporting configuration for illuminance measurements.
   * @param min_interval Minimum reporting interval in seconds.
   * @param max_interval Maximum reporting interval in seconds.
   * @param delta Minimum change required to trigger a report in lux.
   * @return true if successful.
   */
  bool setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta);

  /**
   * @brief Manually report the current illuminance value.
   * @return true if successful.
   */
  bool report();

private:
  ezb_zcl_illuminance_measurement_cluster_config_t _illuminance_meas_cfg;
  uint16_t _tolerance;
};

#endif  // CONFIG_ZB_ENABLED
