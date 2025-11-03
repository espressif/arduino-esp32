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
  _device_id = ESP_ZB_HA_THERMOSTAT_DEVICE_ID;  //There is no FAN_CONTROL_DEVICE_ID in the Zigbee spec

  //Create basic analog sensor clusters without configuration
  _cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(_cluster_list, esp_zb_basic_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(_cluster_list, esp_zb_identify_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_fan_control_cluster(_cluster_list, esp_zb_fan_control_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  _ep_config = {
    .endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_HEATING_COOLING_UNIT_DEVICE_ID, .app_device_version = 0
  };
}

bool ZigbeeFanControl::setFanModeSequence(ZigbeeFanModeSequence sequence) {
  esp_zb_attribute_list_t *fan_control_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_FAN_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_err_t ret = esp_zb_cluster_update_attr(fan_control_cluster, ESP_ZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_SEQUENCE_ID, (void *)&sequence);
  if (ret != ESP_OK) {
    log_e("Failed to set min value: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  _current_fan_mode_sequence = sequence;
  _current_fan_mode = FAN_MODE_OFF;
  // Set initial fan mode to OFF
  ret = esp_zb_cluster_update_attr(fan_control_cluster, ESP_ZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_ID, (void *)&_current_fan_mode);
  if (ret != ESP_OK) {
    log_e("Failed to set fan mode: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

//set attribute method -> method overridden in child class
void ZigbeeFanControl::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
  //check the data and call right method
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_FAN_CONTROL) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_FAN_CONTROL_FAN_MODE_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_8BIT_ENUM) {
      _current_fan_mode = *(ZigbeeFanMode *)message->attribute.data.value;
      fanModeChanged();
    } else {
      log_w("Received message ignored. Attribute ID: %d not supported for Fan Control", message->attribute.id);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %d not supported for Fan Control", message->info.cluster);
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
