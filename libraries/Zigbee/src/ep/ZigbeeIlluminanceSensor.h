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

/* Class of Zigbee Illuminance sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ezbee/zcl/cluster/illuminance_measurement_desc.h"

class ZigbeeIlluminanceSensor : public ZigbeeEP {
public:
  ZigbeeIlluminanceSensor(uint8_t endpoint);
  ~ZigbeeIlluminanceSensor() {}

  // Set the illuminance value
  bool setIlluminance(uint16_t value);

  // Set the default (initial) value for the illuminance sensor
  // Must be called before adding the EP to Zigbee class. Only effective in factory reset mode (before commissioning)
  bool setDefaultValue(uint16_t defaultValue);

  // Set the min and max value for the illuminance sensor
  bool setMinMaxValue(uint16_t min, uint16_t max);

  // Set the tolerance value for the illuminance sensor
  bool setTolerance(uint16_t tolerance);

  // Set the reporting interval for illuminance measurement in seconds and delta
  bool setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta);

  // Report the illuminance value
  bool report();

private:
  ezb_zcl_illuminance_measurement_cluster_config_t _illuminance_meas_cfg;
  uint16_t _tolerance;
};

#endif  // CONFIG_ZB_ENABLED
