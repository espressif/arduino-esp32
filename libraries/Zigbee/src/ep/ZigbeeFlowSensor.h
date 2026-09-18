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
#include "ezbee/zcl/cluster/flow_measurement_desc.h"

class ZigbeeFlowSensor : public ZigbeeEP {
public:
  ZigbeeFlowSensor(uint8_t endpoint);
  ~ZigbeeFlowSensor() {}

  // Set the flow value in 0,1 m3/h
  bool setFlow(float value);

  // Set the default (initial) value for the flow sensor in 0,1 m3/h
  // Must be called before adding the EP to Zigbee class. Only effective in factory reset mode (before commissioning)
  bool setDefaultValue(float defaultValue);

  // Set the min and max value for the flow sensor in 0,1 m3/h
  bool setMinMaxValue(float min, float max);

  // Set the tolerance value for the flow sensor in 0,01 m3/h
  bool setTolerance(float tolerance);

  // Set the reporting interval for flow measurement in seconds and delta (temp change in 0,1 m3/h)
  bool setReporting(uint16_t min_interval, uint16_t max_interval, float delta);

  // Report the flow value
  bool report();

private:
  ezb_zcl_flow_measurement_cluster_config_t _flow_meas_cfg;
  uint16_t _tolerance;
};

#endif  // CONFIG_ZB_ENABLED
