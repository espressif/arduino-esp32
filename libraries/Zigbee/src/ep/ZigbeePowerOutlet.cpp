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

#include "ZigbeePowerOutlet.h"
#if CONFIG_ZB_ENABLED
#include "ezbee/zha.h"
#include "ezbee/zcl/cluster/on_off_desc.h"

ZigbeePowerOutlet::ZigbeePowerOutlet(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_MAINS_POWER_OUTLET_DEVICE_ID;
  _on_state_change = nullptr;

  // v2.x data model: the ZHA template builds the full endpoint descriptor (basic, identify, groups,
  // scenes, on/off clusters) instead of the v1 manual cluster-list factory.
  _ep_config = {
    .ep_id = endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_MAINS_POWER_OUTLET_DEVICE_ID, .app_device_version = 0
  };
  log_v("Outlet endpoint created %u", _endpoint);
    ezb_zha_mains_power_outlet_config_t outlet_cfg = EZB_ZHA_MAINS_POWER_OUTLET_CONFIG();
    _ep_desc = ezb_zha_create_mains_power_outlet(_endpoint, &outlet_cfg);
}



//set attribute method -> method overridden in child class
void ZigbeePowerOutlet::zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) {
  //check the data and call right method
  if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_ON_OFF) {
    if (message->in.attribute.id == EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_BOOL) {
      _current_state = *(bool *)message->in.attribute.data.value;
      stateChanged();
    } else {
      log_w("Received message ignored. Attribute ID: %u not supported for On/Off Outlet", message->in.attribute.id);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %u not supported for On/Off Outlet", message->info.cluster_id);
  }
}

void ZigbeePowerOutlet::stateChanged() {
  if (_on_state_change) {
    _on_state_change(_current_state);
  } else {
    log_w("No callback function set for outlet change");
  }
}

bool ZigbeePowerOutlet::setState(bool state) {
  _current_state = state;
  stateChanged();

  log_v("Updating on/off outlet state to %d", state);
  /* Update on/off outlet state */
  ezb_zcl_status_t ret =
    setClusterAttribute(EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &_current_state, false);

  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set outlet state: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

#endif  // CONFIG_ZB_ENABLED
