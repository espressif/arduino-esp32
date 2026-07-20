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

/* Class of Zigbee WindSpeed sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zcl/cluster/wind_speed_measurement_desc.h"

class ZigbeeWindSpeedSensor : public ZigbeeEP {
public:
  ZigbeeWindSpeedSensor(uint8_t endpoint);
  ~ZigbeeWindSpeedSensor() {}

  // Set the WindSpeed value in 0,01 m/s
  bool setWindSpeed(float value);

  // Set the default (initial) value for the wind speed sensor in 0,01 m/s
  // Must be called before adding the EP to Zigbee class. Only effective in factory reset mode (before commissioning)
  bool setDefaultValue(float defaultValue);

  // Set the min and max value for the WindSpeed sensor
  bool setMinMaxValue(float min, float max);

  // Set the tolerance value for the WindSpeed sensor
  bool setTolerance(float tolerance);

  // Set the reporting interval for WindSpeed measurement in seconds and delta
  bool setReporting(uint16_t min_interval, uint16_t max_interval, float delta);
  bool reportWindSpeed();

private:
  ezb_zcl_wind_speed_measurement_cluster_config_t _wind_speed_meas_cfg;
  uint16_t _tolerance;
};

#endif  // CONFIG_ZB_ENABLED
