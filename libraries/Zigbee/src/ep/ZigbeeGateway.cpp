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

#include "ZigbeeGateway.h"
#if CONFIG_ZB_ENABLED

ZigbeeGateway::ZigbeeGateway(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_HOME_GATEWAY_DEVICE_ID;

  // v2.x data model: build the endpoint descriptor manually (Basic + Identify) instead of the v1
  // cluster-list factory. Registered as a Home Gateway device to preserve the v1 device id.
  ezb_af_ep_config_t ep_config = {
    .ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_HOME_GATEWAY_DEVICE_ID, .app_device_version = 0
  };
  _ep_desc = ezb_af_create_endpoint_desc(&ep_config);
  if (_ep_desc == nullptr) {
    log_e("Failed to create gateway endpoint descriptor");
    return;
  }
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_basic_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_identify_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
  // Preserve the v1 client-role Identify cluster (was esp_zb_zcl_attr_list_create at CLIENT role).
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_identify_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_CLIENT));

  _ep_config = ep_config;
}

#endif  // CONFIG_ZB_ENABLED
