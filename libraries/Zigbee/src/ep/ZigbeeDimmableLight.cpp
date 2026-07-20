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

#include "ZigbeeDimmableLight.h"
#if CONFIG_ZB_ENABLED
#include "ezbee/zha.h"
#include "ezbee/zcl/cluster/on_off_desc.h"
#include "ezbee/zcl/cluster/level_desc.h"

ZigbeeDimmableLight::ZigbeeDimmableLight(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_DIMMABLE_LIGHT_DEVICE_ID;
  _on_light_change = nullptr;

  // v2.x data model: the ZHA template builds the full endpoint descriptor (basic, identify, groups,
  // scenes, on/off, level clusters) instead of the v1 manual cluster-list factory.
  _ep_config = {.ep_id = endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_DIMMABLE_LIGHT_DEVICE_ID, .app_device_version = 0};

  // set default values
  _current_state = false;
  _current_level = 255;
    ezb_zha_dimmable_light_config_t light_cfg = EZB_ZHA_DIMMABLE_LIGHT_CONFIG();
    _ep_desc = ezb_zha_create_dimmable_light(_endpoint, &light_cfg);
}



// set attribute method -> method overridden in child class
void ZigbeeDimmableLight::zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) {
  // check the data and call right method
  if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_ON_OFF) {
    if (message->in.attribute.id == EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_BOOL) {
      if (_current_state != *(bool *)message->in.attribute.data.value) {
        _current_state = *(bool *)message->in.attribute.data.value;
        lightChanged();
      }
      return;
    } else {
      log_w("Received message ignored. Attribute ID: %u not supported for On/Off Light", message->in.attribute.id);
    }
  } else if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_LEVEL) {
    if (message->in.attribute.id == EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID && message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_UINT8) {
      if (_current_level != *(uint8_t *)message->in.attribute.data.value) {
        _current_level = *(uint8_t *)message->in.attribute.data.value;
        lightChanged();
      }
      return;
    } else {
      log_w("Received message ignored. Attribute ID: %u not supported for Level Control", message->in.attribute.id);
      // TODO: implement more attributes -> ezbee/zcl/cluster/level_desc.h
    }
  } else {
    log_w("Received message ignored. Cluster ID: %u not supported for dimmable Light", message->info.cluster_id);
  }
}

void ZigbeeDimmableLight::lightChanged() {
  if (_on_light_change) {
    _on_light_change(_current_state, _current_level);
  }
}

bool ZigbeeDimmableLight::setLight(bool state, uint8_t level) {
  ezb_zcl_status_t ret = EZB_ZCL_STATUS_SUCCESS;
  // Update all attributes
  _current_state = state;
  _current_level = level;
  lightChanged();

  log_v("Updating on/off light state to %d", state);
  ret = setClusterAttribute(EZB_ZCL_CLUSTER_ID_ON_OFF, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &_current_state, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set light state: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  ret = setClusterAttribute(EZB_ZCL_CLUSTER_ID_LEVEL, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID, &_current_level, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set light level: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeDimmableLight::setLightState(bool state) {
  return setLight(state, _current_level);
}

bool ZigbeeDimmableLight::setLightLevel(uint8_t level) {
  return setLight(_current_state, level);
}

#endif  // CONFIG_ZB_ENABLED
