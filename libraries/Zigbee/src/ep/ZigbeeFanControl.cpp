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

#include "ZigbeeFanControl.h"
#if CONFIG_ZB_ENABLED

ZigbeeFanControl::ZigbeeFanControl(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_THERMOSTAT_DEVICE_ID;  //There is no FAN_CONTROL_DEVICE_ID in the Zigbee spec
  _on_fan_mode_change = nullptr;

  // v2.x data model: build the endpoint descriptor manually (Basic + Identify + FanControl server).
  // NOTE(zb-v2): the v1 device id mismatch is preserved: _device_id is Thermostat while the endpoint
  // descriptor is registered as a Heating/Cooling Unit (matching the v1 _ep_config.app_device_id).
  ezb_af_ep_config_t ep_config = {
    .ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_HEATING_COOLING_UNIT_DEVICE_ID, .app_device_version = 0
  };
  _ep_desc = ezb_af_create_endpoint_desc(&ep_config);
  if (_ep_desc == nullptr) {
    log_e("Failed to create fan control endpoint descriptor");
    return;
  }
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_basic_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_identify_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_fan_control_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));

  _ep_config = ep_config;
}

bool ZigbeeFanControl::setFanModeSequence(ZigbeeFanModeSequence sequence) {
  ezb_zcl_cluster_desc_t fan_control_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_FAN_CONTROL, EZB_ZCL_CLUSTER_SERVER);
  uint8_t seq_value = (uint8_t)sequence;
  ezb_err_t ret = ezb_zcl_fan_control_cluster_desc_add_attr(fan_control_cluster, EZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_SEQUENCE_ID, (void *)&seq_value);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set fan mode sequence: 0x%x", ret);
    return false;
  }
  _current_fan_mode_sequence = sequence;
  _current_fan_mode = FAN_MODE_OFF;
  // Set initial fan mode to OFF
  uint8_t mode_value = (uint8_t)_current_fan_mode;
  ret = ezb_zcl_fan_control_cluster_desc_add_attr(fan_control_cluster, EZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_ID, (void *)&mode_value);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set fan mode: 0x%x", ret);
    return false;
  }
  return true;
}

//set attribute method -> method overridden in child class
void ZigbeeFanControl::zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) {
  //check the data and call right method
  if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_FAN_CONTROL) {
    // NOTE(zb-v2): v1 ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM is EZB_ZCL_ATTR_TYPE_ENUM8 in v2.x.
    if (message->in.attribute.id == EZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_ID && message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_ENUM8) {
      if (message->in.attribute.data.value != nullptr && message->in.attribute.data.size >= 1) {
        uint8_t raw_mode = *(const uint8_t *)message->in.attribute.data.value;
        if (raw_mode <= FAN_MODE_SMART) {
          _current_fan_mode = (ZigbeeFanMode)raw_mode;
          fanModeChanged();
        } else {
          log_w("Fan mode value out of range: %u, ignoring", raw_mode);
        }
      } else {
        log_w("Invalid fan mode attribute: value=%p size=%u", message->in.attribute.data.value, message->in.attribute.data.size);
      }
    } else {
      log_w("Received message ignored. Attribute ID: %u not supported for Fan Control", message->in.attribute.id);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %u not supported for Fan Control", message->info.cluster_id);
  }
}

void ZigbeeFanControl::fanModeChanged() {
  if (_on_fan_mode_change) {
    _on_fan_mode_change(_current_fan_mode);
  } else {
    log_w("No callback function set for fan mode change");
  }
}

#endif  // CONFIG_ZB_ENABLED
