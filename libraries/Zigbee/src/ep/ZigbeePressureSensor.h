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

/* Class of Zigbee Pressure sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zha.h"
#include "ezbee/zcl/cluster/basic_desc.h"
#include "ezbee/zcl/cluster/identify_desc.h"
#include "ezbee/zcl/cluster/pressure_measurement_desc.h"

// clang-format off
#define ZIGBEE_DEFAULT_PRESSURE_SENSOR_CONFIG()                                                            \
    {                                                                                                      \
        .basic_cfg =                                                                                       \
            {                                                                                              \
                .zcl_version = EZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,                                    \
                .power_source = EZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,                                  \
            },                                                                                             \
        .identify_cfg =                                                                                    \
            {                                                                                              \
                .identify_time = EZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,                             \
            },                                                                                             \
        .pressure_meas_cfg =                                                                               \
            {                                                                                              \
                .measured_value = EZB_ZCL_PRESSURE_MEASUREMENT_MEASURED_VALUE_DEFAULT_VALUE,               \
                .min_measured_value = EZB_ZCL_PRESSURE_MEASUREMENT_MIN_MEASURED_VALUE_DEFAULT_VALUE,       \
                .max_measured_value = EZB_ZCL_PRESSURE_MEASUREMENT_MAX_MEASURED_VALUE_DEFAULT_VALUE,       \
            },                                                                                             \
    }
// clang-format on

typedef struct zigbee_pressure_sensor_cfg_s {
  ezb_zcl_basic_cluster_config_t basic_cfg;
  ezb_zcl_identify_cluster_config_t identify_cfg;
  ezb_zcl_pressure_measurement_cluster_config_t pressure_meas_cfg;
} zigbee_pressure_sensor_cfg_t;

class ZigbeePressureSensor : public ZigbeeEP {
public:
  ZigbeePressureSensor(uint8_t endpoint);
  ~ZigbeePressureSensor() {}

  // Set the pressure value in 1 hPa
  bool setPressure(int16_t value);

  // Set the default (initial) value for the pressure sensor in 1 hPa
  // Must be called before adding the EP to Zigbee class. Only effective in factory reset mode (before commissioning)
  bool setDefaultValue(int16_t defaultValue);

  // Set the min and max value for the pressure sensor in 1 hPa
  bool setMinMaxValue(int16_t min, int16_t max);

  // Set the tolerance value for the pressure sensor in 1 hPa
  bool setTolerance(uint16_t tolerance);

  // Set the reporting interval for pressure measurement in seconds and delta (pressure change in 1 hPa)
  bool setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta);

  // Report the pressure value
  bool report();
};

#endif  // CONFIG_ZB_ENABLED
