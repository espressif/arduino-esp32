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
#include "ezbee/zha.h"
#include "ezbee/zcl/cluster/basic_desc.h"
#include "ezbee/zcl/cluster/identify_desc.h"
#include "ezbee/zcl/cluster/occupancy_sensing_desc.h"

// clang-format off
#define ZIGBEE_DEFAULT_OCCUPANCY_SENSOR_CONFIG()                                                             \
  {                                                                                                          \
    .basic_cfg =                                                                                             \
      {                                                                                                      \
        .zcl_version = EZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,                                              \
        .power_source = EZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,                                            \
      },                                                                                                     \
    .identify_cfg =                                                                                          \
      {                                                                                                      \
        .identify_time = EZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,                                       \
      },                                                                                                     \
    .occupancy_meas_cfg =                                                                                    \
      {                                                                                                      \
        .occupancy = EZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_UNOCCUPIED,                                         \
        .occupancy_sensor_type = EZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_PIR,                        \
        .occupancy_sensor_type_bitmap = (1 << EZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_PIR),          \
      },                                                                                                     \
  }
// clang-format on

typedef struct zigbee_occupancy_sensor_cfg_s {
  ezb_zcl_basic_cluster_config_t basic_cfg;
  ezb_zcl_identify_cluster_config_t identify_cfg;
  ezb_zcl_occupancy_sensing_cluster_config_t occupancy_meas_cfg;
} zigbee_occupancy_sensor_cfg_t;

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
};

#endif  // CONFIG_ZB_ENABLED
