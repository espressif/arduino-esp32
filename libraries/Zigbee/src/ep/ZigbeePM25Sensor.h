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

/* Class of Zigbee PM2.5 sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zcl/cluster/pm2_5_measurement_desc.h"

class ZigbeePM25Sensor : public ZigbeeEP {
public:
  ZigbeePM25Sensor(uint8_t endpoint);
  ~ZigbeePM25Sensor() {}

  // Set the PM2.5 value in 0.1 µg/m³
  bool setPM25(float pm25);

  // Set the default (initial) value for the PM2.5 sensor in 0.1 µg/m³
  // Must be called before adding the EP to Zigbee class. Only effective in factory reset mode (before commissioning)
  bool setDefaultValue(float defaultValue);

  // Set the min and max value for the PM2.5 sensor in 0.1 µg/m³
  bool setMinMaxValue(float min, float max);

  // Set the tolerance value for the PM2.5 sensor in 0.1 µg/m³
  bool setTolerance(float tolerance);

  // Set the reporting interval for PM2.5 measurement in seconds and delta (PM2.5 change in 0.1 µg/m³)
  bool setReporting(uint16_t min_interval, uint16_t max_interval, float delta);

  // Report the PM2.5 value
  bool report();

private:
  ezb_zcl_pm2_5_measurement_cluster_config_t _pm2_5_meas_cfg;
  float _tolerance;
};

#endif  // CONFIG_ZB_ENABLED
