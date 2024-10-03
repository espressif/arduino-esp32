#include "ZigbeeColorDimmerSwitch.h"
#if SOC_IEEE802154_SUPPORTED

// Initialize the static instance pointer
ZigbeeColorDimmerSwitch *ZigbeeColorDimmerSwitch::_instance = nullptr;

ZigbeeColorDimmerSwitch::ZigbeeColorDimmerSwitch(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_COLOR_DIMMER_SWITCH_DEVICE_ID;
  _instance = this;  // Set the static pointer to this instance

  esp_zb_color_dimmable_switch_cfg_t switch_cfg = ESP_ZB_DEFAULT_COLOR_DIMMABLE_SWITCH_CONFIG();
  _cluster_list = esp_zb_color_dimmable_switch_clusters_create(&switch_cfg);

  _ep_config = {
    .endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_COLOR_DIMMER_SWITCH_DEVICE_ID, .app_device_version = 0
  };
}

void ZigbeeColorDimmerSwitch::calculateXY(uint8_t red, uint8_t green, uint8_t blue, uint16_t &x, uint16_t &y) {
  // Convert RGB to XYZ
  float r = (float)red / 255.0f;
  float g = (float)green / 255.0f;
  float b = (float)blue / 255.0f;

  float X, Y, Z;
  RGB_TO_XYZ(r, g, b, X, Y, Z);

  // Convert XYZ to xy chromaticity coordinates
  float color_x = X / (X + Y + Z);
  float color_y = Y / (X + Y + Z);

  // Convert normalized xy to 16-bit values
  x = (uint16_t)(color_x * 65535.0f);
  y = (uint16_t)(color_y * 65535.0f);
}

void ZigbeeColorDimmerSwitch::bindCb(esp_zb_zdp_status_t zdo_status, void *user_ctx) {
  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    log_i("Bound successfully!");
    if (user_ctx) {
      zb_device_params_t *light = (zb_device_params_t *)user_ctx;
      log_i("The light originating from address(0x%x) on endpoint(%d)", light->short_addr, light->endpoint);
      _instance->_bound_devices.push_back(light);
    }
    _is_bound = true;
  } else {
    log_e("Binding failed!");
  }
}

void ZigbeeColorDimmerSwitch::findCb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx) {
  if (zdo_status == ESP_ZB_ZDP_STATUS_SUCCESS) {
    log_d("Found light endpoint");
    esp_zb_zdo_bind_req_param_t bind_req;
    zb_device_params_t *light = (zb_device_params_t *)malloc(sizeof(zb_device_params_t));
    light->endpoint = endpoint;
    light->short_addr = addr;
    esp_zb_ieee_address_by_short(light->short_addr, light->ieee_addr);
    esp_zb_get_long_address(bind_req.src_address);
    bind_req.src_endp = _endpoint;
    bind_req.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ON_OFF;
    bind_req.dst_addr_mode = ESP_ZB_ZDO_BIND_DST_ADDR_MODE_64_BIT_EXTENDED;
    memcpy(bind_req.dst_address_u.addr_long, light->ieee_addr, sizeof(esp_zb_ieee_addr_t));
    bind_req.dst_endp = endpoint;
    bind_req.req_dst_addr = esp_zb_get_short_address();
    log_v("Try to bind on/off control of dimmable light");
    esp_zb_zdo_device_bind_req(&bind_req, bindCb, NULL);
    bind_req.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL;
    log_v("Try to bind level control of dimmable light");
    esp_zb_zdo_device_bind_req(&bind_req, bindCb, NULL);
    bind_req.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL;
    log_v("Try to bind color control of dimmable light");
    esp_zb_zdo_device_bind_req(&bind_req, bindCb, (void *)light);
  } else {
    log_v("No color dimmable light endpoint found");
  }
}

// find on_off light endpoint
void ZigbeeColorDimmerSwitch::findEndpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req) {
  uint16_t cluster_list[] = {ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                             ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL};
  esp_zb_zdo_match_desc_req_param_t color_dimmable_light_req = {
    .dst_nwk_addr = cmd_req->dst_nwk_addr,
    .addr_of_interest = cmd_req->addr_of_interest,
    .profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    .num_in_clusters = 3,
    .num_out_clusters = 3,
    .cluster_list = cluster_list,
  };
  esp_zb_zdo_match_cluster(&color_dimmable_light_req, findCb, NULL);
}

// Methods to control the light
void ZigbeeColorDimmerSwitch::lightToggle() {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;
    log_i("Sending 'light toggle' command");
    //esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    //esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeColorDimmerSwitch::lightToggle(uint16_t group_addr) {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = group_addr;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;
    log_i("Sending 'light toggle' command to group address 0x%x", group_addr);
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeColorDimmerSwitch::lightToggle(uint8_t endpoint, uint16_t short_addr) {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_endpoint = endpoint;
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = short_addr;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_TOGGLE_ID;
    log_i("Sending 'light toggle' command to endpoint %d, address 0x%x", endpoint, short_addr);
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeColorDimmerSwitch::lightOn() {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_ON_ID;
    log_i("Sending 'light on' command");
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeColorDimmerSwitch::lightOn(uint16_t group_addr) {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = group_addr;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_ON_ID;
    log_i("Sending 'light on' command to group address 0x%x", group_addr);
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeColorDimmerSwitch::lightOn(uint8_t endpoint, uint16_t short_addr) {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_endpoint = endpoint;
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = short_addr;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_ON_ID;
    log_i("Sending 'light on' command to endpoint %d, address 0x%x", endpoint, short_addr);
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeColorDimmerSwitch::lightOff() {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID;
    log_i("Sending 'light off' command");
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeColorDimmerSwitch::lightOff(uint16_t group_addr) {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = group_addr;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID;
    log_i("Sending 'light off' command to group address 0x%x", group_addr);
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeColorDimmerSwitch::lightOff(uint8_t endpoint, uint16_t short_addr) {
  if (_is_bound) {
    esp_zb_zcl_on_off_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_endpoint = endpoint;
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = short_addr;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd_req.on_off_cmd_id = ESP_ZB_ZCL_CMD_ON_OFF_OFF_ID;
    log_i("Sending 'light off' command to endpoint %d, address 0x%x", endpoint, short_addr);
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeColorDimmerSwitch::lightOffWithEffect(uint8_t effect_id, uint8_t effect_variant) {
  if (_is_bound) {
    esp_zb_zcl_on_off_off_with_effect_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    cmd_req.effect_id = effect_id;
    cmd_req.effect_variant = effect_variant;
    log_i("Sending 'light off with effect' command");
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_off_with_effect_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeColorDimmerSwitch::lightOnWithSceneRecall() {
  if (_is_bound) {
    esp_zb_zcl_on_off_on_with_recall_global_scene_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    log_i("Sending 'light on with scene recall' command");
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_on_with_recall_global_scene_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeColorDimmerSwitch::lightOnWithTimedOff(uint8_t on_off_control, uint16_t time_on, uint16_t time_off) {
  if (_is_bound) {
    esp_zb_zcl_on_off_on_with_timed_off_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    cmd_req.on_off_control = on_off_control;  //TODO: Test how it works, then maybe change API
    cmd_req.on_time = time_on;
    cmd_req.off_wait_time = time_off;
    log_i("Sending 'light on with time off' command");
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_on_off_on_with_timed_off_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeColorDimmerSwitch::setLightLevel(uint8_t level) {
  if (_is_bound) {
    esp_zb_zcl_move_to_level_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    cmd_req.level = level;
    cmd_req.transition_time = 0xffff;
    log_i("Sending 'set light level' command");
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_level_move_to_level_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeColorDimmerSwitch::setLightLevel(uint8_t level, uint16_t group_addr) {
  if (_is_bound) {
    esp_zb_zcl_move_to_level_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = group_addr;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT;
    cmd_req.level = level;
    cmd_req.transition_time = 0xffff;
    log_i("Sending 'set light level' command to group address 0x%x", group_addr);
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_level_move_to_level_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeColorDimmerSwitch::setLightLevel(uint8_t level, uint8_t endpoint, uint16_t short_addr) {
  if (_is_bound) {
    esp_zb_zcl_move_to_level_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_endpoint = endpoint;
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = short_addr;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd_req.level = level;
    cmd_req.transition_time = 0xffff;
    log_i("Sending 'set light level' command to endpoint %d, address 0x%x", endpoint, short_addr);
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_level_move_to_level_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeColorDimmerSwitch::setLightColor(uint8_t red, uint8_t green, uint8_t blue) {
  if (_is_bound) {
    //Convert RGB to XY
    uint16_t color_x, color_y;
    calculateXY(red, green, blue, color_x, color_y);

    esp_zb_zcl_color_move_to_color_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    cmd_req.color_x = color_x;
    cmd_req.color_y = color_y;
    cmd_req.transition_time = 0;
    log_i("Sending 'set light color' command");
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_color_move_to_color_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeColorDimmerSwitch::setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint16_t group_addr) {
  if (_is_bound) {
    //Convert RGB to XY
    uint16_t color_x, color_y;
    calculateXY(red, green, blue, color_x, color_y);

    esp_zb_zcl_color_move_to_color_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = group_addr;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_GROUP_ENDP_NOT_PRESENT;
    cmd_req.color_x = color_x;
    cmd_req.color_y = color_y;
    cmd_req.transition_time = 0;
    log_i("Sending 'set light color' command to group address 0x%x", group_addr);
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_color_move_to_color_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

void ZigbeeColorDimmerSwitch::setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t endpoint, uint16_t short_addr) {
  if (_is_bound) {
    //Convert RGB to XY
    uint16_t color_x, color_y;
    calculateXY(red, green, blue, color_x, color_y);

    esp_zb_zcl_color_move_to_color_cmd_t cmd_req;
    cmd_req.zcl_basic_cmd.src_endpoint = _endpoint;
    cmd_req.zcl_basic_cmd.dst_endpoint = endpoint;
    cmd_req.zcl_basic_cmd.dst_addr_u.addr_short = short_addr;
    cmd_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    cmd_req.color_x = color_x;
    cmd_req.color_y = color_y;
    cmd_req.transition_time = 0;
    log_i("Sending 'set light color' command to endpoint %d, address 0x%x", endpoint, short_addr);
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_color_move_to_color_cmd_req(&cmd_req);
    esp_zb_lock_release();
  } else {
    log_e("Light not bound");
  }
}

#endif  //SOC_IEEE802154_SUPPORTED
