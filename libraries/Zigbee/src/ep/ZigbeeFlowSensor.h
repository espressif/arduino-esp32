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

/* Class of Zigbee Flow sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

// clang-format off
#define ZIGBEE_DEFAULT_FLOW_SENSOR_CONFIG()                               \
  {                                                                       \
    .basic_cfg =                                                          \
      {                                                                   \
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,        \
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,      \
      },                                                                  \
    .identify_cfg =                                                       \
      {                                                                   \
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE, \
      },                                                                  \
    .flow_meas_cfg =                                                      \
      {                                                                   \
        .measured_value = 0,                                              \
        .min_value = 0,                                                   \
        .max_value = 0x7FFF,                                              \
      },                                                                  \
  }
// clang-format on

typedef struct zigbee_flow_sensor_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;
  esp_zb_identify_cluster_cfg_t identify_cfg;
  esp_zb_flow_meas_cluster_cfg_t flow_meas_cfg;
} zigbee_flow_sensor_cfg_t;

class ZigbeeFlowSensor : public ZigbeeEP {
public:
  ZigbeeFlowSensor(uint8_t endpoint);
  ~ZigbeeFlowSensor() {}

  // Set the flow value in 0,1 m3/h
  bool setFlow(float value);

  // Set the min and max value for the flow sensor in 0,1 m3/h
  bool setMinMaxValue(float min, float max);

  // Set the tolerance value for the flow sensor in 0,01 m3/h
  bool setTolerance(float tolerance);

  // Set the reporting interval for flow measurement in seconds and delta (temp change in 0,1 m3/h)
  bool setReporting(uint16_t min_interval, uint16_t max_interval, float delta);

  // Report the flow value
  bool report();
};

#endif  // CONFIG_ZB_ENABLED
