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

/* Class of Zigbee Carbon Dioxide sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zcl/cluster/carbon_dioxide_measurement_desc.h"

class ZigbeeCarbonDioxideSensor : public ZigbeeEP {
public:
  ZigbeeCarbonDioxideSensor(uint8_t endpoint);
  ~ZigbeeCarbonDioxideSensor() {}

  // Set the carbon dioxide value in ppm
  bool setCarbonDioxide(float carbon_dioxide);

  // Set the default (initial) value for the carbon dioxide sensor in ppm
  // Must be called before adding the EP to Zigbee class. Only effective in factory reset mode (before commissioning)
  bool setDefaultValue(float defaultValue);

  // Set the min and max value for the carbon dioxide sensor in ppm
  bool setMinMaxValue(float min, float max);

  // Set the tolerance value for the carbon dioxide sensor in ppm
  bool setTolerance(float tolerance);

  // Set the reporting interval for carbon dioxide measurement in seconds and delta (carbon dioxide change in ppm)
  // NOTE: Delta reporting is currently not supported by the carbon dioxide sensor
  bool setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta);

  // Report the carbon dioxide value
  bool report();

private:
  ezb_zcl_carbon_dioxide_measurement_cluster_config_t _carbon_dioxide_meas_cfg;
  float _tolerance;
};

#endif  // CONFIG_ZB_ENABLED
