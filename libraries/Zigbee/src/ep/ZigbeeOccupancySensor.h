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

/* Class of Zigbee Occupancy sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zcl/cluster/occupancy_sensing_desc.h"

class ZigbeeOccupancySensor : public ZigbeeEP {
public:
  ZigbeeOccupancySensor(uint8_t endpoint);
  ~ZigbeeOccupancySensor() {}

  // Set the occupancy value. True for occupied, false for unoccupied
  bool setOccupancy(bool occupied);

  // Set the sensor type, see ezb_zcl_occupancy_sensing_server_occupancy_sensor_type_t
  bool setSensorType(uint8_t sensor_type);

  // Report the occupancy value
  bool report();

private:
  ezb_zcl_occupancy_sensing_cluster_config_t _occupancy_meas_cfg;
};

#endif  // CONFIG_ZB_ENABLED
