
// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ZigbeeTemperatureDimmableLight.h"
#if CONFIG_ZB_ENABLED

ZigbeeTemperatureDimmableLight::ZigbeeTemperatureDimmableLight(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID;  // Use the correct device ID for color temperature light

  zigbee_temperature_dimmable_light_cfg_t light_cfg = ZIGBEE_DEFAULT_TEMPERATURE_DIMMABLE_LIGHT_CONFIG();
  _cluster_list = zigbee_temperature_dimmable_light_clusters_create(&light_cfg);

  //Add support for Temperature Color Control cluster
  esp_zb_attribute_list_t *color_cluster = esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  uint16_t color_attr = ESP_ZB_ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_DEF_VALUE;
  uint16_t min_temp = ESP_ZB_ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_MIREDS_DEFAULT_VALUE;
  uint16_t max_temp = ESP_ZB_ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_MIREDS_DEFAULT_VALUE;
  uint8_t color_mode = ESP_ZB_ZCL_COLOR_CONTROL_COLOR_MODE_DEFAULT_VALUE;

  esp_zb_color_control_cluster_add_attr(color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_ID, &color_attr);
  esp_zb_color_control_cluster_add_attr(color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_MIREDS_ID, &min_temp);
  esp_zb_color_control_cluster_add_attr(color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_MIREDS_ID, &max_temp);
  esp_zb_color_control_cluster_add_attr(color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_MODE_ID, &color_mode);

  _ep_config = {
    .endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID, .app_device_version = 0
  };

  // set default values
  _current_state = false;
  _current_level = 255;
  _current_color_temperature = ESP_ZB_ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_DEF_VALUE;  // 6500K
}

bool ZigbeeTemperatureDimmableLight::setMinMaxTemperature(uint16_t min_temp, uint16_t max_temp) {
  esp_zb_attribute_list_t *color_cluster = esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_err_t ret = esp_zb_cluster_update_attr(color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_MIREDS_ID, &min_temp);
  if (ret != ESP_OK) {
    log_e("Failed to set min value: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  ret = esp_zb_cluster_update_attr(color_cluster, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_MIREDS_ID, &max_temp);
  if (ret != ESP_OK) {
    log_e("Failed to set max value: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

void ZigbeeTemperatureDimmableLight::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
      if (_current_state != *(bool *)message->attribute.data.value) {
        _current_state = *(bool *)message->attribute.data.value;
        lightChanged();
      }
      return;
    } else {
      log_w("Received message ignored. Attribute ID: %d not supported for Temperature Dimmable Light", message->attribute.id);
    }
  } else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
      if (_current_level != *(uint8_t *)message->attribute.data.value) {
        _current_level = *(uint8_t *)message->attribute.data.value;
        lightChanged();
      }
      return;
    } else {
      log_w("Received message ignored. Attribute ID: %d not supported for Temperature Dimmable Light", message->attribute.id);
    }
  } else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
      if (_current_color_temperature != *(uint16_t *)message->attribute.data.value) {
        _current_color_temperature = *(uint16_t *)message->attribute.data.value;
        lightChanged();
      }
      return;

    } else {
      log_w("Received message ignored. Attribute ID: %d not supported for Temperature Dimmable Light", message->attribute.id);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %d not supported for Temperature Dimmable Light", message->info.cluster);
  }
}

void ZigbeeTemperatureDimmableLight::lightChanged() {
  if (_on_light_change) {
    _on_light_change(_current_state, _current_level, _current_color_temperature);
  }
}

bool ZigbeeTemperatureDimmableLight::setLight(bool state, uint8_t level, uint16_t mireds) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  _current_state = state;
  _current_level = level;
  _current_color_temperature = mireds;
  lightChanged();

  log_v("Updating light state: %d, level: %d, color temperature: %d", state, level, mireds);
  /* Update light clusters */
  esp_zb_lock_acquire(portMAX_DELAY);
  //set on/off state
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &_current_state, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set light state: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
  //set level
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID, &_current_level, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set light level: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
  //set color temperature
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_COLOR_TEMPERATURE_ID,
    &_current_color_temperature, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set light color temperature: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
unlock_and_return:
  esp_zb_lock_release();
  return ret == ESP_ZB_ZCL_STATUS_SUCCESS;
}

bool ZigbeeTemperatureDimmableLight::setLightState(bool state) {
  return setLight(state, _current_level, _current_color_temperature);
}

bool ZigbeeTemperatureDimmableLight::setLightLevel(uint8_t level) {
  return setLight(_current_state, level, _current_color_temperature);
}

bool ZigbeeTemperatureDimmableLight::setColorTemperature(uint16_t mireds) {
  return setLight(_current_state, _current_level, mireds);
}

esp_zb_cluster_list_t *ZigbeeTemperatureDimmableLight::zigbee_temperature_dimmable_light_clusters_create(zigbee_temperature_dimmable_light_cfg_t *light_cfg) {
  esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_basic_cluster_create(&light_cfg->basic_cfg);
  esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_identify_cluster_create(&light_cfg->identify_cfg);
  esp_zb_attribute_list_t *esp_zb_groups_cluster = esp_zb_groups_cluster_create(&light_cfg->groups_cfg);
  esp_zb_attribute_list_t *esp_zb_scenes_cluster = esp_zb_scenes_cluster_create(&light_cfg->scenes_cfg);
  esp_zb_attribute_list_t *esp_zb_on_off_cluster = esp_zb_on_off_cluster_create(&light_cfg->on_off_cfg);
  esp_zb_attribute_list_t *esp_zb_level_cluster = esp_zb_level_cluster_create(&light_cfg->level_cfg);
  esp_zb_attribute_list_t *esp_zb_color_cluster = esp_zb_color_control_cluster_create(&light_cfg->color_cfg);

  esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_groups_cluster(esp_zb_cluster_list, esp_zb_groups_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_scenes_cluster(esp_zb_cluster_list, esp_zb_scenes_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_on_off_cluster(esp_zb_cluster_list, esp_zb_on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_level_cluster(esp_zb_cluster_list, esp_zb_level_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_color_control_cluster(esp_zb_cluster_list, esp_zb_color_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  return esp_zb_cluster_list;
}

#endif  // CONFIG_ZB_ENABLED
