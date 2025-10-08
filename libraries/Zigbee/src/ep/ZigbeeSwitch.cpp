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

#include "ZigbeeSwitch.h"
#if CONFIG_ZB_ENABLED

// Initialize the static instance pointer
ZigbeeSwitch *ZigbeeSwitch::_instance = nullptr;

ZigbeeSwitch::ZigbeeSwitch(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID;
  _instance = this;  // Set the static pointer to this instance
  _device = nullptr;

  esp_zb_on_off_switch_cfg_t switch_cfg = ESP_ZB_DEFAULT_ON_OFF_SWITCH_CONFIG();
  _cluster_list = esp_zb_on_off_switch_clusters_create(&switch_cfg);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_ON_OFF_SWITCH_DEVICE_ID, .app_device_version = 0};
}

void ZigbeeSwitch::bindCb(esp_zb_zdp_status_t zdo_status, void *user_ctx) {
  ZigbeeSwitch *instance = static_cast<ZigbeeSwitch *>(user_ctx);
  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    log_i("Bound successfully!");
    if (instance->_device) {
      zb_device_params_t *light = (zb_device_params_t *)instance->_device;
      log_i("The light originating from address(0x%x) on endpoint(%d)", light->short_addr, light->endpoint);
      log_d("Light bound to a switch on EP %d", instance->_endpoint);
      instance->_bound_devices.push_back(light);
    }
    instance->_is_bound = true;
  } else {
    instance->_device = nullptr;
  }
}

void ZigbeeSwitch::bindCbWrapper(esp_zb_zdp_status_t zdo_status, void *user_ctx) {
  ZigbeeSwitch *instance = static_cast<ZigbeeSwitch *>(user_ctx);
  if (instance) {
    log_d("bindCbWrapper on EP %d", instance->_endpoint);
    instance->bindCb(zdo_status, user_ctx);
  }
}

// Static wrapper for findCb
void ZigbeeSwitch::findCbWrapper(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx) {
  ZigbeeSwitch *instance = static_cast<ZigbeeSwitch *>(user_ctx);
  if (instance) {
    log_d("findCbWrapper on EP %d", instance->_endpoint);
    instance->findCb(zdo_status, addr, endpoint, user_ctx);
  }
}

void ZigbeeSwitch::findCb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx) {
  ZigbeeSwitch *instance = static_cast<ZigbeeSwitch *>(user_ctx);
  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    log_d("Found light endpoint");
    esp_zb_zdo_bind_req_param_t bind_req;
    memset(&bind_req, 0, sizeof(bind_req));
    zb_device_params_t *light = (zb_device_params_t *)malloc(sizeof(zb_device_params_t));
    light->endpoint = endpoint;
    light->short_addr = addr;
    esp_zb_ieee_address_by_short(light->short_addr, light->ieee_addr);
    log_d("Light found: short address(0x%x), endpoint(%d)", light->short_addr, light->endpoint);

    esp_zb_get_long_address(bind_req.src_address);
    bind_req.src_endp = instance->_endpoint;
    bind_req.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF;
    bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
    memcpy(bind_req.dst_address_u.addr_long, light->ieee_addr, sizeof(esp_zb_ieee_addr_t));
    bind_req.dst_endp = endpoint;
    bind_req.req_dst_addr = esp_zb_get_short_address();
    log_v("Try to bind On/Off");
    //save light params in the class
    instance->_device = light;

    log_d("Find callback on EP %d", instance->_endpoint);
    esp_zb_zdo_device_bind_req(&bind_req, ZigbeeSwitch::bindCbWrapper, this);
  } else {
    log_d("No light endpoint found");
  }
}

// find on_off light endpoint
void ZigbeeSwitch::findEndpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req) {
  uint16_t cluster_list[] = {ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF};
  esp_zb_zdo_match_desc_req_param_t on_off_req = {
    .dst_nwk_addr = cmd_req->dst_nwk_addr,
    .addr_of_interest = cmd_req->addr_of_interest,
    .profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    .num_in_clusters = 1,
    .num_out_clusters = 1,
    .cluster_list = cluster_list,
  };
  esp_zb_zdo_match_cluster(&on_off_req, ZigbeeSwitch::findCbWrapper, this);
}

// Methods to control the light
void ZigbeeSwitch::lightToggle() {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    memset(&cmd_req, 0, sizeof(cmd_req));
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;
    log_v("Sending 'light toggle' command");
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeSwitch::lightToggle(uint16_t group_addr) {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    memset(&cmd_req, 0, sizeof(cmd_req));
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = group_addr;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;
    log_v("Sending 'light toggle' command to group address 0x%x", group_addr);
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeSwitch::lightToggle(uint8_t endpoint, uint16_t short_addr) {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    memset(&cmd_req, 0, sizeof(cmd_req));
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_endpoint = endpoint;
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = short_addr;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;
    log_v("Sending 'light toggle' command to endpoint %d, address 0x%x", endpoint, short_addr);
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeSwitch::lightToggle(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr) {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    memset(&cmd_req, 0, sizeof(cmd_req));
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_endpoint = endpoint;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;
    memcpy(cmd_req.zcl_basic_cmd.dst_addr_u.addr_long, ieee_addr, sizeof(esp_zb_ieee_addr_t));
    log_v(
      "Sending 'light toggle' command to endpoint %d, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6], ieee_addr[5],
      ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
    );
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeSwitch::lightOn() {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    memset(&cmd_req, 0, sizeof(cmd_req));
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_ON_ID;
    log_v("Sending 'light on' command");
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeSwitch::lightOn(uint16_t group_addr) {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    memset(&cmd_req, 0, sizeof(cmd_req));
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = group_addr;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_ON_ID;
    log_v("Sending 'light on' command to group address 0x%x", group_addr);
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeSwitch::lightOn(uint8_t endpoint, uint16_t short_addr) {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    memset(&cmd_req, 0, sizeof(cmd_req));
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_endpoint = endpoint;
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = short_addr;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_ON_ID;
    log_v("Sending 'light on' command to endpoint %d, address 0x%x", endpoint, short_addr);
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeSwitch::lightOn(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr) {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    memset(&cmd_req, 0, sizeof(cmd_req));
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_endpoint = endpoint;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_ON_ID;
    memcpy(cmd_req.zcl_basic_cmd.dst_addr_u.addr_long, ieee_addr, sizeof(esp_zb_ieee_addr_t));
    log_v(
      "Sending 'light on' command to endpoint %d, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6], ieee_addr[5],
      ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
    );
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeSwitch::lightOff() {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    memset(&cmd_req, 0, sizeof(cmd_req));
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID;
    log_v("Sending 'light off' command");
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeSwitch::lightOff(uint16_t group_addr) {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    memset(&cmd_req, 0, sizeof(cmd_req));
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = group_addr;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID;
    log_v("Sending 'light off' command to group address 0x%x", group_addr);
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeSwitch::lightOff(uint8_t endpoint, uint16_t short_addr) {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    memset(&cmd_req, 0, sizeof(cmd_req));
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_endpoint = endpoint;
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = short_addr;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID;
    log_v("Sending 'light off' command to endpoint %d, address 0x%x", endpoint, short_addr);
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeSwitch::lightOff(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr) {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    memset(&cmd_req, 0, sizeof(cmd_req));
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_endpoint = endpoint;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID;
    memcpy(cmd_req.zcl_basic_cmd.dst_addr_u.addr_long, ieee_addr, sizeof(esp_zb_ieee_addr_t));
    log_v(
      "Sending 'light off' command to endpoint %d, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6], ieee_addr[5],
      ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
    );
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeSwitch::lightOffWithEffect(uint8_t effect_id, uint8_t effect_variant) {
  if (_is_bound) {
    esp_zb_zcl_on_off_off_with_effect_cmd_t cmd_req;
    memset(&cmd_req, 0, sizeof(cmd_req));
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    cmd_req.effect_id = effect_id;
    cmd_req.effect_variant = effect_variant;
    log_v("Sending 'light off with effect' command");
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_off_with_effect_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeSwitch::lightOnWithSceneRecall() {
  if (_is_bound) {
    esp_zb_zcl_on_off_on_with_recall_global_scene_cmd_t cmd_req;
    memset(&cmd_req, 0, sizeof(cmd_req));
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    log_v("Sending 'light on with scene recall' command");
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_on_with_recall_global_scene_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeSwitch::lightOnWithTimedOff(uint8_t on_off_control, uint16_t time_on, uint16_t time_off) {
  if (_is_bound) {
    esp_zb_zcl_on_off_on_with_timed_off_cmd_t cmd_req;
    memset(&cmd_req, 0, sizeof(cmd_req));
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    cmd_req.on_off_control = on_off_control;  //TODO: Test how it works, then maybe change API
    cmd_req.on_time = time_on;
    cmd_req.off_wait_time = time_off;
    log_v("Sending 'light on with time off' command");
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_on_with_timed_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeSwitch::getLightState() {
  if (_is_bound) {
    esp_zb_zcl_read_attr_cmd_t read_req;
    memset(&read_req, 0, sizeof(read_req));
    read_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    read_req.zcl_basic_cmd.src_endpoint = _endpoint;
    read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF;
    read_req.attr_number = 1;
    uint16_t attr_id = ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID;
    read_req.attr_field = &attr_id;
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_read_attr_cmd_req(&read_req);
    esp_zb_lock_release();
  }
}

void ZigbeeSwitch::getLightState(uint16_t group_addr) {
  if (_is_bound) {
    esp_zb_zcl_read_attr_cmd_t read_req;
    memset(&read_req, 0, sizeof(read_req));
    read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT;
    read_req.zcl_basic_cmd.src_endpoint = _endpoint;
    read_req.zcl_basic_cmd.dst_addr_u.addr_short = group_addr;
    read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF;
    read_req.attr_number = 1;
    uint16_t attr_id = ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID;
    read_req.attr_field = &attr_id;
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_read_attr_cmd_req(&read_req);
    esp_zb_lock_release();
  }
}

void ZigbeeSwitch::getLightState(uint8_t endpoint, uint16_t short_addr) {
  if (_is_bound) {
    esp_zb_zcl_read_attr_cmd_t read_req;
    memset(&read_req, 0, sizeof(read_req));
    read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    read_req.zcl_basic_cmd.src_endpoint = _endpoint;
    read_req.zcl_basic_cmd.dst_endpoint = endpoint;
    read_req.zcl_basic_cmd.dst_addr_u.addr_short = short_addr;
    read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF;
    read_req.attr_number = 1;
    uint16_t attr_id = ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID;
    read_req.attr_field = &attr_id;
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_read_attr_cmd_req(&read_req);
    esp_zb_lock_release();
  }
}

void ZigbeeSwitch::getLightState(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr) {
  if (_is_bound) {
    esp_zb_zcl_read_attr_cmd_t read_req;
    memset(&read_req, 0, sizeof(read_req));
    read_req.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
    read_req.zcl_basic_cmd.src_endpoint = _endpoint;
    read_req.zcl_basic_cmd.dst_endpoint = endpoint;
    memcpy(read_req.zcl_basic_cmd.dst_addr_u.addr_long, ieee_addr, sizeof(esp_zb_ieee_addr_t));
    read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF;
    read_req.attr_number = 1;
    uint16_t attr_id = ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID;
    read_req.attr_field = &attr_id;
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_read_attr_cmd_req(&read_req);
    esp_zb_lock_release();
  }
}

void ZigbeeSwitch::zbAttributeRead(uint16_t cluster_id, const esp_zb_zcl_attribute_t *attribute, uint8_t src_endpoint, esp_zb_zcl_addr_t src_address) {
  if (cluster_id == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
    if (attribute->id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && attribute->data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
      bool light_state = attribute->data.value ? *(bool *)attribute->data.value : false;
      if (_on_light_state_change) {
        _on_light_state_change(light_state);
      }
      if (_on_light_state_change_with_source) {
        _on_light_state_change_with_source(light_state, src_endpoint, src_address);
      }
    }
  }
}
#endif  // CONFIG_ZB_ENABLED
