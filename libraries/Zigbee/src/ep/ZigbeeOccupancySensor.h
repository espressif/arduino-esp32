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

/* Class of Zigbee Occupancy sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zcl/cluster/occupancy_sensing_desc.h"

/**
 * @brief Occupancy sensor endpoint for presence detection (Zigbee HA).
 *
 * Supports occupied/unoccupied reporting and multiple sensor types (PIR, ultrasonic, etc.).
 */
class ZigbeeOccupancySensor : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee occupancy sensor endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeOccupancySensor(uint8_t endpoint);
  ~ZigbeeOccupancySensor() {}

  /**
   * @brief Set the occupancy state.
   * @param occupied true if occupied, false if unoccupied.
   * @return true if successful.
   */
  bool setOccupancy(bool occupied);

  /**
   * @brief Set the sensor type.
   * @param sensor_type Sensor type identifier (see `ezb_zcl_occupancy_sensing_server_occupancy_sensor_type_t`).
   * @return true if successful.
   */
  bool setSensorType(uint8_t sensor_type);

  /**
   * @brief Manually report the current occupancy state.
   * @return true if successful.
   */
  bool report();

private:
  ezb_zcl_occupancy_sensing_cluster_config_t _occupancy_meas_cfg;
};

#endif  // CONFIG_ZB_ENABLED
