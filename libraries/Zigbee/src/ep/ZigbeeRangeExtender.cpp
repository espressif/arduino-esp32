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

#include "ZigbeeRangeExtender.h"
#if CONFIG_ZB_ENABLED
#include "ezbee/zha.h"

ZigbeeRangeExtender::ZigbeeRangeExtender(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_RANGE_EXTENDER_DEVICE_ID;

  // v2.x data model: build the endpoint descriptor manually (Basic + Identify) instead of the v1
  // cluster-list factory. Registered as a Range Extender device to preserve the v1 device id.
  ezb_af_ep_config_t ep_config = {
    .ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_RANGE_EXTENDER_DEVICE_ID, .app_device_version = 0
  };
  _ep_config = ep_config;
    _ep_desc = ezb_af_create_endpoint_desc(&_ep_config);
    if (_ep_desc == nullptr) {
    log_e("Failed to create range extender endpoint descriptor");
    }
    ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_basic_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
    ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_identify_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
    ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_identify_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_CLIENT));
}



#endif  // CONFIG_ZB_ENABLED
