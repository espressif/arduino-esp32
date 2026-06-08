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

#include "ZigbeeColorDimmerSwitch.h"
#if CONFIG_ZB_ENABLED
#include "ezbee/zha.h"
#include "ezbee/zcl/cluster/on_off.h"
#include "ezbee/zcl/cluster/level.h"
#include "ezbee/zcl/cluster/color_control.h"

// Initialize the static instance pointer
ZigbeeColorDimmerSwitch *ZigbeeColorDimmerSwitch::_instance = nullptr;

// v2.x addressing helpers: build the cluster command control block for each destination style.
// In v2.x there is no address_mode enum; the destination is expressed through ezb_address_t.
static ezb_zcl_cluster_cmd_ctrl_t make_cmd_ctrl_bound(uint8_t src_ep) {
  ezb_zcl_cluster_cmd_ctrl_t c = {};
  ezb_address_set_none(&c.dst_addr);  // no explicit destination -> routed to bound devices
  c.src_ep = src_ep;
  return c;
}

static ezb_zcl_cluster_cmd_ctrl_t make_cmd_ctrl_group(uint8_t src_ep, uint16_t group_addr) {
  ezb_zcl_cluster_cmd_ctrl_t c = {};
  c.dst_addr.addr_mode = EZB_ADDR_MODE_GROUP;
  c.dst_addr.u.group_addr.group = group_addr;
  c.dst_addr.u.group_addr.bcast = 0;
  c.src_ep = src_ep;
  return c;
}

static ezb_zcl_cluster_cmd_ctrl_t make_cmd_ctrl_short(uint8_t src_ep, uint8_t dst_ep, uint16_t short_addr) {
  ezb_zcl_cluster_cmd_ctrl_t c = {};
  ezb_address_set_short(&c.dst_addr, short_addr);
  c.dst_ep = dst_ep;
  c.src_ep = src_ep;
  return c;
}

static ezb_zcl_cluster_cmd_ctrl_t make_cmd_ctrl_ieee(uint8_t src_ep, uint8_t dst_ep, const uint8_t *ieee_addr) {
  ezb_zcl_cluster_cmd_ctrl_t c = {};
  ezb_extaddr_t ext;
  memcpy(ext.u8, ieee_addr, sizeof(ext.u8));
  ezb_address_set_extended(&c.dst_addr, ext);
  c.dst_ep = dst_ep;
  c.src_ep = src_ep;
  return c;
}

ZigbeeColorDimmerSwitch::ZigbeeColorDimmerSwitch(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_COLOR_DIMMER_SWITCH_DEVICE_ID;
  _instance = this;   // Set the static pointer to this instance
  _device = nullptr;  // Initialize light pointer to null

  _light_color_rgb = {0, 0, 0};
  _light_color_hsv = {0, 0, 255};
  _light_color_xy = {0, 0};
  _on_light_state_change = nullptr;
  _on_light_state_change_with_source = nullptr;
  _on_light_level_change = nullptr;
  _on_light_level_change_with_source = nullptr;
  _on_light_color_change = nullptr;
  _on_light_color_change_with_source = nullptr;

  _ep_config = {
    .ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_COLOR_DIMMER_SWITCH_DEVICE_ID, .app_device_version = 0
  };
    ezb_zha_color_dimmer_switch_config_t switch_cfg = EZB_ZHA_COLOR_DIMMER_SWITCH_CONFIG();
    _ep_desc = ezb_zha_create_color_dimmer_switch(_endpoint, &switch_cfg);
}



void ZigbeeColorDimmerSwitch::bindCb(const ezb_zdp_bind_req_result_t *result, void *user_ctx) {
  ZigbeeColorDimmerSwitch *instance = static_cast<ZigbeeColorDimmerSwitch *>(user_ctx);
  bool success = result && result->error == EZB_ERR_NONE && result->rsp && result->rsp->status == EZB_ZDP_STATUS_SUCCESS;
  if (success) {
    log_i("Bound successfully!");
    if (instance->_device) {
      zb_device_params_t *light = (zb_device_params_t *)instance->_device;
      log_i("The light originating from address(0x%x) on endpoint(%u)", light->short_addr, light->endpoint);
      log_d("Light bound to a switch on EP %u", instance->_endpoint);
      instance->_bound_devices.push_back(light);
    }
    instance->_is_bound = true;
  } else {
    instance->_device = nullptr;
  }
}

void ZigbeeColorDimmerSwitch::bindCbWrapper(const ezb_zdp_bind_req_result_t *result, void *user_ctx) {
  ZigbeeColorDimmerSwitch *instance = static_cast<ZigbeeColorDimmerSwitch *>(user_ctx);
  if (instance) {
    log_d("bindCbWrapper on EP %u", instance->_endpoint);
    instance->bindCb(result, user_ctx);
  }
}

void ZigbeeColorDimmerSwitch::findCbWrapper(const ezb_zdo_match_desc_req_result_t *result, void *user_ctx) {
  ZigbeeColorDimmerSwitch *instance = static_cast<ZigbeeColorDimmerSwitch *>(user_ctx);
  if (instance) {
    log_d("findCbWrapper on EP %u", instance->_endpoint);
    instance->findCb(result, user_ctx);
  }
}

void ZigbeeColorDimmerSwitch::findCb(const ezb_zdo_match_desc_req_result_t *result, void *user_ctx) {
  ZigbeeColorDimmerSwitch *instance = static_cast<ZigbeeColorDimmerSwitch *>(user_ctx);
  // A NULL response marks completion of a broadcast match; nothing matched if we never got a hit.
  if (result == nullptr || result->error != EZB_ERR_NONE || result->rsp == nullptr) {
    log_d("No color dimmable light endpoint found");
    return;
  }
  ezb_zdp_match_desc_rsp_field_t *rsp = result->rsp;
  if (rsp->status != EZB_ZDP_STATUS_SUCCESS || rsp->match_length == 0 || rsp->match_list == nullptr) {
    log_d("No color dimmable light endpoint found");
    return;
  }

  uint16_t addr = rsp->nwk_addr_of_interest;
  uint8_t endpoint = rsp->match_list[0];
  log_d("Found light endpoint");

  zb_device_params_t *light = (zb_device_params_t *)malloc(sizeof(zb_device_params_t));
  light->endpoint = endpoint;
  light->short_addr = addr;
  ezb_extaddr_t light_ext;
  ezb_address_extended_by_short(light->short_addr, &light_ext);
  memcpy(light->ieee_addr, light_ext.u8, sizeof(light_ext.u8));
  log_d("Light found: short address(0x%x), endpoint(%u)", light->short_addr, light->endpoint);

  // Build the Bind_req. The binding entry is stored on this device, so the request is addressed to ourselves.
  ezb_zdo_bind_req_t bind_req;
  memset(&bind_req, 0, sizeof(bind_req));
  bind_req.dst_nwk_addr = ezb_nwk_get_short_address();
  ezb_get_extended_address(&bind_req.field.src_addr);
  bind_req.field.src_ep = instance->_endpoint;
  bind_req.field.dst_addr_mode = 0x03;  // 64-bit extended address, DstAddress and DstEndp present
  bind_req.field.dst_addr.extended_addr = light_ext;
  bind_req.field.dst_ep = endpoint;
  bind_req.cb = ZigbeeColorDimmerSwitch::bindCbWrapper;
  bind_req.user_ctx = this;

  //save light params in the class
  instance->_device = light;

  log_d("Find callback on EP %u", instance->_endpoint);

  // Bind On/Off, Level Control and Color Control clusters of the dimmable light.
  bind_req.field.cluster_id = EZB_ZCL_CLUSTER_ID_ON_OFF;
  log_v("Try to bind on/off control of dimmable light");
  ezb_zdo_bind_req(&bind_req);
  bind_req.field.cluster_id = EZB_ZCL_CLUSTER_ID_LEVEL;
  log_v("Try to bind level control of dimmable light");
  ezb_zdo_bind_req(&bind_req);
  bind_req.field.cluster_id = EZB_ZCL_CLUSTER_ID_COLOR_CONTROL;
  log_v("Try to bind color control of dimmable light");
  ezb_zdo_bind_req(&bind_req);
}

// find color dimmable light endpoint
void ZigbeeColorDimmerSwitch::findEndpoint(ezb_zdo_match_desc_req_t *cmd_req) {
  // First num_in_clusters entries are server (input) clusters, then num_out_clusters client (output) clusters.
  static uint16_t cluster_list[] = {EZB_ZCL_CLUSTER_ID_ON_OFF,    EZB_ZCL_CLUSTER_ID_LEVEL,  EZB_ZCL_CLUSTER_ID_COLOR_CONTROL,
                                    EZB_ZCL_CLUSTER_ID_ON_OFF,    EZB_ZCL_CLUSTER_ID_LEVEL,  EZB_ZCL_CLUSTER_ID_COLOR_CONTROL};
  cmd_req->field.profile_id = EZB_AF_HA_PROFILE_ID;
  cmd_req->field.num_in_clusters = 3;
  cmd_req->field.num_out_clusters = 3;
  cmd_req->field.cluster_list = cluster_list;
  cmd_req->cb = ZigbeeColorDimmerSwitch::findCbWrapper;
  cmd_req->user_ctx = this;
  ezb_zdo_match_desc_req(cmd_req);
}

void ZigbeeColorDimmerSwitch::sendOnOffCommand(uint8_t on_off_cmd_id, const void *cmd_ctrl_ptr) {
  const ezb_zcl_cluster_cmd_ctrl_t *cmd_ctrl = static_cast<const ezb_zcl_cluster_cmd_ctrl_t *>(cmd_ctrl_ptr);

  if (!_is_bound) {
    log_e("Light not bound");
    return;
  }
  ezb_zcl_on_off_cmd_t cmd_req = {};
  cmd_req.cmd_ctrl = *cmd_ctrl;
  if (!acquireCommandLock()) {
    return;
  }
  switch (on_off_cmd_id) {
    case EZB_ZCL_CMD_ON_OFF_ON_ID:     ezb_zcl_on_off_on_cmd_req(&cmd_req); break;
    case EZB_ZCL_CMD_ON_OFF_OFF_ID:    ezb_zcl_on_off_off_cmd_req(&cmd_req); break;
    case EZB_ZCL_CMD_ON_OFF_TOGGLE_ID: ezb_zcl_on_off_toggle_cmd_req(&cmd_req); break;
    default:                           log_e("Unsupported On/Off command id 0x%02x", on_off_cmd_id); break;
  }
  releaseCommandLock();
}

// Methods to control the light
void ZigbeeColorDimmerSwitch::lightToggle() {
  log_v("Sending 'light toggle' command");
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_bound(_endpoint);
  sendOnOffCommand(EZB_ZCL_CMD_ON_OFF_TOGGLE_ID, &c);
}

void ZigbeeColorDimmerSwitch::lightToggle(uint16_t group_addr) {
  log_v("Sending 'light toggle' command to group address 0x%x", group_addr);
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_group(_endpoint, group_addr);
  sendOnOffCommand(EZB_ZCL_CMD_ON_OFF_TOGGLE_ID, &c);
}

void ZigbeeColorDimmerSwitch::lightToggle(uint8_t endpoint, uint16_t short_addr) {
  log_v("Sending 'light toggle' command to endpoint %u, address 0x%x", endpoint, short_addr);
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_short(_endpoint, endpoint, short_addr);
  sendOnOffCommand(EZB_ZCL_CMD_ON_OFF_TOGGLE_ID, &c);
}

void ZigbeeColorDimmerSwitch::lightToggle(uint8_t endpoint, const uint8_t *ieee_addr) {
  log_v(
    "Sending 'light toggle' command to endpoint %u, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6], ieee_addr[5],
    ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
  );
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_ieee(_endpoint, endpoint, ieee_addr);
  sendOnOffCommand(EZB_ZCL_CMD_ON_OFF_TOGGLE_ID, &c);
}

void ZigbeeColorDimmerSwitch::lightOn() {
  log_v("Sending 'light on' command");
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_bound(_endpoint);
  sendOnOffCommand(EZB_ZCL_CMD_ON_OFF_ON_ID, &c);
}

void ZigbeeColorDimmerSwitch::lightOn(uint16_t group_addr) {
  log_v("Sending 'light on' command to group address 0x%x", group_addr);
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_group(_endpoint, group_addr);
  sendOnOffCommand(EZB_ZCL_CMD_ON_OFF_ON_ID, &c);
}

void ZigbeeColorDimmerSwitch::lightOn(uint8_t endpoint, uint16_t short_addr) {
  log_v("Sending 'light on' command to endpoint %u, address 0x%x", endpoint, short_addr);
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_short(_endpoint, endpoint, short_addr);
  sendOnOffCommand(EZB_ZCL_CMD_ON_OFF_ON_ID, &c);
}

void ZigbeeColorDimmerSwitch::lightOn(uint8_t endpoint, const uint8_t *ieee_addr) {
  log_v(
    "Sending 'light on' command to endpoint %u, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6], ieee_addr[5],
    ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
  );
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_ieee(_endpoint, endpoint, ieee_addr);
  sendOnOffCommand(EZB_ZCL_CMD_ON_OFF_ON_ID, &c);
}

void ZigbeeColorDimmerSwitch::lightOff() {
  log_v("Sending 'light off' command");
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_bound(_endpoint);
  sendOnOffCommand(EZB_ZCL_CMD_ON_OFF_OFF_ID, &c);
}

void ZigbeeColorDimmerSwitch::lightOff(uint16_t group_addr) {
  log_v("Sending 'light off' command to group address 0x%x", group_addr);
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_group(_endpoint, group_addr);
  sendOnOffCommand(EZB_ZCL_CMD_ON_OFF_OFF_ID, &c);
}

void ZigbeeColorDimmerSwitch::lightOff(uint8_t endpoint, uint16_t short_addr) {
  log_v("Sending 'light off' command to endpoint %u, address 0x%x", endpoint, short_addr);
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_short(_endpoint, endpoint, short_addr);
  sendOnOffCommand(EZB_ZCL_CMD_ON_OFF_OFF_ID, &c);
}

void ZigbeeColorDimmerSwitch::lightOff(uint8_t endpoint, const uint8_t *ieee_addr) {
  log_v(
    "Sending 'light off' command to endpoint %u, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6], ieee_addr[5],
    ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
  );
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_ieee(_endpoint, endpoint, ieee_addr);
  sendOnOffCommand(EZB_ZCL_CMD_ON_OFF_OFF_ID, &c);
}

void ZigbeeColorDimmerSwitch::lightOffWithEffect(uint8_t effect_id, uint8_t effect_variant) {
  if (!_is_bound) {
    log_e("Light not bound");
    return;
  }
  ezb_zcl_on_off_off_with_effect_cmd_t cmd_req = {};
  cmd_req.cmd_ctrl = make_cmd_ctrl_bound(_endpoint);
  cmd_req.payload.effect_id = effect_id;
  cmd_req.payload.effect_variant = effect_variant;
  log_v("Sending 'light off with effect' command");
  if (!acquireCommandLock()) {
    return;
  }
  ezb_zcl_on_off_off_with_effect_cmd_req(&cmd_req);
  releaseCommandLock();
}

void ZigbeeColorDimmerSwitch::lightOnWithSceneRecall() {
  if (!_is_bound) {
    log_e("Light not bound");
    return;
  }
  ezb_zcl_on_off_cmd_t cmd_req = {};
  cmd_req.cmd_ctrl = make_cmd_ctrl_bound(_endpoint);
  log_v("Sending 'light on with scene recall' command");
  if (!acquireCommandLock()) {
    return;
  }
  ezb_zcl_on_off_on_with_recall_global_scene_cmd_req(&cmd_req);
  releaseCommandLock();
}

void ZigbeeColorDimmerSwitch::lightOnWithTimedOff(uint8_t on_off_control, uint16_t time_on, uint16_t time_off) {
  if (!_is_bound) {
    log_e("Light not bound");
    return;
  }
  ezb_zcl_on_off_on_with_timed_off_cmd_t cmd_req = {};
  cmd_req.cmd_ctrl = make_cmd_ctrl_bound(_endpoint);
  cmd_req.payload.on_off_control = on_off_control;  //TODO: Test how it works, then maybe change API
  cmd_req.payload.on_time = time_on;
  cmd_req.payload.off_wait_time = time_off;
  log_v("Sending 'light on with time off' command");
  if (!acquireCommandLock()) {
    return;
  }
  ezb_zcl_on_off_on_with_timed_off_cmd_req(&cmd_req);
  releaseCommandLock();
}

void ZigbeeColorDimmerSwitch::sendMoveToLevel(uint8_t level, const void *cmd_ctrl_ptr) {
  const ezb_zcl_cluster_cmd_ctrl_t *cmd_ctrl = static_cast<const ezb_zcl_cluster_cmd_ctrl_t *>(cmd_ctrl_ptr);

  if (!_is_bound) {
    log_e("Light not bound");
    return;
  }
  ezb_zcl_level_move_to_level_cmd_t cmd_req = {};
  cmd_req.cmd_ctrl = *cmd_ctrl;
  cmd_req.payload.level = level;
  cmd_req.payload.transition_time = 0xffff;
  if (!acquireCommandLock()) {
    return;
  }
  ezb_zcl_level_move_to_level_cmd_req(&cmd_req);
  releaseCommandLock();
}

void ZigbeeColorDimmerSwitch::setLightLevel(uint8_t level) {
  log_v("Sending 'set light level' command");
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_bound(_endpoint);
  sendMoveToLevel(level, &c);
}

void ZigbeeColorDimmerSwitch::setLightLevel(uint8_t level, uint16_t group_addr) {
  log_v("Sending 'set light level' command to group address 0x%x", group_addr);
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_group(_endpoint, group_addr);
  sendMoveToLevel(level, &c);
}

void ZigbeeColorDimmerSwitch::setLightLevel(uint8_t level, uint8_t endpoint, uint16_t short_addr) {
  log_v("Sending 'set light level' command to endpoint %u, address 0x%x", endpoint, short_addr);
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_short(_endpoint, endpoint, short_addr);
  sendMoveToLevel(level, &c);
}

void ZigbeeColorDimmerSwitch::setLightLevel(uint8_t level, uint8_t endpoint, const uint8_t *ieee_addr) {
  log_v(
    "Sending 'set light level' command to endpoint %u, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6],
    ieee_addr[5], ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
  );
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_ieee(_endpoint, endpoint, ieee_addr);
  sendMoveToLevel(level, &c);
}

void ZigbeeColorDimmerSwitch::sendLevelStep(ZigbeeLevelStepDirection direction, uint8_t step_size, uint16_t transition_time, const void *cmd_ctrl_ptr) {
  const ezb_zcl_cluster_cmd_ctrl_t *cmd_ctrl = static_cast<const ezb_zcl_cluster_cmd_ctrl_t *>(cmd_ctrl_ptr);

  if (!_is_bound) {
    log_e("Light not bound");
    return;
  }
  ezb_zcl_level_step_cmd_t cmd_req = {};
  cmd_req.cmd_ctrl = *cmd_ctrl;
  cmd_req.payload.step_mode = (uint8_t)direction;
  cmd_req.payload.step_size = step_size;
  cmd_req.payload.transition_time = transition_time;
  if (!acquireCommandLock()) {
    return;
  }
  ezb_zcl_level_step_cmd_req(&cmd_req);
  releaseCommandLock();
}

void ZigbeeColorDimmerSwitch::setLightLevelStep(ZigbeeLevelStepDirection direction, uint8_t step_size, uint16_t transition_time) {
  log_v("Sending 'set light level step' command");
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_bound(_endpoint);
  sendLevelStep(direction, step_size, transition_time, &c);
}

void ZigbeeColorDimmerSwitch::setLightLevelStep(ZigbeeLevelStepDirection direction, uint8_t step_size, uint16_t transition_time, uint16_t group_addr) {
  log_v("Sending 'set light level step' command to group address 0x%x", group_addr);
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_group(_endpoint, group_addr);
  sendLevelStep(direction, step_size, transition_time, &c);
}

void ZigbeeColorDimmerSwitch::setLightLevelStep(
  ZigbeeLevelStepDirection direction, uint8_t step_size, uint16_t transition_time, uint8_t endpoint, uint16_t short_addr
) {
  log_v("Sending 'set light level step' command to endpoint %u, address 0x%x", endpoint, short_addr);
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_short(_endpoint, endpoint, short_addr);
  sendLevelStep(direction, step_size, transition_time, &c);
}

void ZigbeeColorDimmerSwitch::setLightLevelStep(
  ZigbeeLevelStepDirection direction, uint8_t step_size, uint16_t transition_time, uint8_t endpoint, const uint8_t *ieee_addr
) {
  log_v(
    "Sending 'set light level step' command to endpoint %u, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6],
    ieee_addr[5], ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
  );
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_ieee(_endpoint, endpoint, ieee_addr);
  sendLevelStep(direction, step_size, transition_time, &c);
}

void ZigbeeColorDimmerSwitch::sendMoveToColor(espXyColor_t xy_color, const void *cmd_ctrl_ptr) {
  const ezb_zcl_cluster_cmd_ctrl_t *cmd_ctrl = static_cast<const ezb_zcl_cluster_cmd_ctrl_t *>(cmd_ctrl_ptr);

  if (!_is_bound) {
    log_e("Light not bound");
    return;
  }
  ezb_zcl_color_control_move_to_color_cmd_t cmd_req = {};
  cmd_req.cmd_ctrl = *cmd_ctrl;
  cmd_req.payload.color_x = xy_color.x;
  cmd_req.payload.color_y = xy_color.y;
  cmd_req.payload.transition_time = 0;
  if (!acquireCommandLock()) {
    return;
  }
  ezb_zcl_color_control_move_to_color_cmd_req(&cmd_req);
  releaseCommandLock();
}

void ZigbeeColorDimmerSwitch::setLightColor(uint8_t red, uint8_t green, uint8_t blue) {
  espXyColor_t xy_color = espRgbToXYColor(red, green, blue);
  log_v("Sending 'set light color' command");
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_bound(_endpoint);
  sendMoveToColor(xy_color, &c);
}

void ZigbeeColorDimmerSwitch::setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint16_t group_addr) {
  espXyColor_t xy_color = espRgbToXYColor(red, green, blue);
  log_v("Sending 'set light color' command to group address 0x%x", group_addr);
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_group(_endpoint, group_addr);
  sendMoveToColor(xy_color, &c);
}

void ZigbeeColorDimmerSwitch::setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t endpoint, uint16_t short_addr) {
  espXyColor_t xy_color = espRgbToXYColor(red, green, blue);
  log_v("Sending 'set light color' command to endpoint %u, address 0x%x", endpoint, short_addr);
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_short(_endpoint, endpoint, short_addr);
  sendMoveToColor(xy_color, &c);
}

void ZigbeeColorDimmerSwitch::setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t endpoint, const uint8_t *ieee_addr) {
  espXyColor_t xy_color = espRgbToXYColor(red, green, blue);
  log_v(
    "Sending 'set light color' command to endpoint %u, ieee address %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", endpoint, ieee_addr[7], ieee_addr[6],
    ieee_addr[5], ieee_addr[4], ieee_addr[3], ieee_addr[2], ieee_addr[1], ieee_addr[0]
  );
  ezb_zcl_cluster_cmd_ctrl_t c = make_cmd_ctrl_ieee(_endpoint, endpoint, ieee_addr);
  sendMoveToColor(xy_color, &c);
}

void ZigbeeColorDimmerSwitch::sendReadAttributes(
  ezb_address_t dst_addr, uint8_t dst_ep, uint16_t cluster_id, uint16_t *attr_field, uint8_t attr_number, const char *err_msg
) {
  if (!_is_bound) {
    return;
  }
  ezb_zcl_read_attr_cmd_t read_req;
  memset(&read_req, 0, sizeof(read_req));
  read_req.cmd_ctrl.dst_addr = dst_addr;
  read_req.cmd_ctrl.dst_ep = dst_ep;
  read_req.cmd_ctrl.src_ep = _endpoint;
  read_req.cmd_ctrl.cluster_id = cluster_id;
  read_req.payload.attr_number = attr_number;
  read_req.payload.attr_field = attr_field;
  if (!readClusterAttribute(&read_req)) {
    log_e("%s", err_msg);
  }
}

// v2.x read addressing helpers (library ezb_address_t; converted at SDK boundary).
static ezb_address_t make_read_addr_bound() {
  ezb_address_t a;
  ezb_address_set_none(&a);
  return a;
}

static ezb_address_t make_read_addr_group(uint16_t group_addr) {
  ezb_address_t a = {};
  a.addr_mode = EZB_ADDR_MODE_GROUP;
  a.u.group_addr.group = group_addr;
  a.u.group_addr.bcast = 0;
  return a;
}

static ezb_address_t make_read_addr_short(uint16_t short_addr) {
  ezb_address_t a;
  ezb_address_set_short(&a, short_addr);
  return a;
}

static ezb_address_t make_read_addr_ieee(const uint8_t *ieee_addr) {
  ezb_extaddr_t ext;
  memcpy(ext.u8, ieee_addr, sizeof(ext.u8));
  ezb_address_t a;
  ezb_address_set_extended(&a, ext);
  return a;
}

void ZigbeeColorDimmerSwitch::getLightState() {
  uint16_t attr_id = EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID;
  sendReadAttributes(make_read_addr_bound(), 0, EZB_ZCL_CLUSTER_ID_ON_OFF, &attr_id, 1, "Failed to send read light state command");
}

void ZigbeeColorDimmerSwitch::getLightState(uint16_t group_addr) {
  uint16_t attr_id = EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID;
  sendReadAttributes(make_read_addr_group(group_addr), 0, EZB_ZCL_CLUSTER_ID_ON_OFF, &attr_id, 1, "Failed to send read light state command");
}

void ZigbeeColorDimmerSwitch::getLightState(uint8_t endpoint, uint16_t short_addr) {
  uint16_t attr_id = EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID;
  sendReadAttributes(make_read_addr_short(short_addr), endpoint, EZB_ZCL_CLUSTER_ID_ON_OFF, &attr_id, 1, "Failed to send read light state command");
}

void ZigbeeColorDimmerSwitch::getLightState(uint8_t endpoint, const uint8_t *ieee_addr) {
  uint16_t attr_id = EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID;
  sendReadAttributes(make_read_addr_ieee(ieee_addr), endpoint, EZB_ZCL_CLUSTER_ID_ON_OFF, &attr_id, 1, "Failed to send read light state command");
}

void ZigbeeColorDimmerSwitch::getLightLevel() {
  uint16_t attr_id = EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID;
  sendReadAttributes(make_read_addr_bound(), 0, EZB_ZCL_CLUSTER_ID_LEVEL, &attr_id, 1, "Failed to send read light level command");
}

void ZigbeeColorDimmerSwitch::getLightLevel(uint16_t group_addr) {
  uint16_t attr_id = EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID;
  sendReadAttributes(make_read_addr_group(group_addr), 0, EZB_ZCL_CLUSTER_ID_LEVEL, &attr_id, 1, "Failed to send read light level command");
}

void ZigbeeColorDimmerSwitch::getLightLevel(uint8_t endpoint, uint16_t short_addr) {
  uint16_t attr_id = EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID;
  sendReadAttributes(make_read_addr_short(short_addr), endpoint, EZB_ZCL_CLUSTER_ID_LEVEL, &attr_id, 1, "Failed to send read light level command");
}

void ZigbeeColorDimmerSwitch::getLightLevel(uint8_t endpoint, const uint8_t *ieee_addr) {
  uint16_t attr_id = EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID;
  sendReadAttributes(make_read_addr_ieee(ieee_addr), endpoint, EZB_ZCL_CLUSTER_ID_LEVEL, &attr_id, 1, "Failed to send read light level command");
}

void ZigbeeColorDimmerSwitch::getLightColor() {
  uint16_t attr_id[] = {EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID, EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID};
  sendReadAttributes(make_read_addr_bound(), 0, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, attr_id, 2, "Failed to send read light color command");
}

void ZigbeeColorDimmerSwitch::getLightColor(uint16_t group_addr) {
  uint16_t attr_id[] = {EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID, EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID};
  sendReadAttributes(make_read_addr_group(group_addr), 0, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, attr_id, 2, "Failed to send read light color command");
}

void ZigbeeColorDimmerSwitch::getLightColor(uint8_t endpoint, uint16_t short_addr) {
  uint16_t attr_id[] = {EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID, EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID};
  sendReadAttributes(make_read_addr_short(short_addr), endpoint, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, attr_id, 2, "Failed to send read light color command");
}

void ZigbeeColorDimmerSwitch::getLightColor(uint8_t endpoint, const uint8_t *ieee_addr) {
  uint16_t attr_id[] = {EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID, EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID};
  sendReadAttributes(make_read_addr_ieee(ieee_addr), endpoint, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, attr_id, 2, "Failed to send read light color command");
}

void ZigbeeColorDimmerSwitch::getLightColorHS() {
  uint16_t attr_id[] = {EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID, EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID};
  sendReadAttributes(make_read_addr_bound(), 0, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, attr_id, 2, "Failed to send read light color (hue/saturation) command");
}

void ZigbeeColorDimmerSwitch::getLightColorHS(uint16_t group_addr) {
  uint16_t attr_id[] = {EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID, EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID};
  sendReadAttributes(make_read_addr_group(group_addr), 0, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, attr_id, 2, "Failed to send read light color (hue/saturation) command");
}

void ZigbeeColorDimmerSwitch::getLightColorHS(uint8_t endpoint, uint16_t short_addr) {
  uint16_t attr_id[] = {EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID, EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID};
  sendReadAttributes(make_read_addr_short(short_addr), endpoint, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, attr_id, 2, "Failed to send read light color (hue/saturation) command");
}

void ZigbeeColorDimmerSwitch::getLightColorHS(uint8_t endpoint, const uint8_t *ieee_addr) {
  uint16_t attr_id[] = {EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID, EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID};
  sendReadAttributes(make_read_addr_ieee(ieee_addr), endpoint, EZB_ZCL_CLUSTER_ID_COLOR_CONTROL, attr_id, 2, "Failed to send read light color (hue/saturation) command");
}

void ZigbeeColorDimmerSwitch::zbAttributeRead(uint16_t cluster_id, const ezb_zcl_attribute_t *attribute, uint8_t src_endpoint, ezb_address_t src_address) {
  if (cluster_id == EZB_ZCL_CLUSTER_ID_ON_OFF) {
    if (attribute->id == EZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_BOOL) {
      bool light_state = attribute->data.value ? *(bool *)attribute->data.value : false;
      if (_on_light_state_change) {
        _on_light_state_change(light_state);
      }
      if (_on_light_state_change_with_source) {
        _on_light_state_change_with_source(light_state, src_endpoint, src_address);
      }
    }
  }
  if (cluster_id == EZB_ZCL_CLUSTER_ID_LEVEL) {
    if (attribute->id == EZB_ZCL_ATTR_LEVEL_CURRENT_LEVEL_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_UINT8) {
      uint8_t light_level = attribute->data.value ? *(uint8_t *)attribute->data.value : 0;
      if (_on_light_level_change) {
        _on_light_level_change(light_level);
      }
      if (_on_light_level_change_with_source) {
        _on_light_level_change_with_source(light_level, src_endpoint, src_address);
      }
    }
  }
  if (cluster_id == EZB_ZCL_CLUSTER_ID_COLOR_CONTROL) {
    static bool x_received = false;
    static bool y_received = false;
    static bool h_received = false;
    static bool s_received = false;

    if (attribute->id == EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_UINT16) {
      _light_color_xy.x = attribute->data.value ? *(uint16_t *)attribute->data.value : 0;
      x_received = true;
    }
    if (attribute->id == EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_UINT16) {
      _light_color_xy.y = attribute->data.value ? *(uint16_t *)attribute->data.value : 0;
      y_received = true;
    }

    if (attribute->id == EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_UINT8) {
      _light_color_hsv.h = attribute->data.value ? *(uint8_t *)attribute->data.value : 0;
      h_received = true;
    }
    if (attribute->id == EZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID && attribute->data.type == EZB_ZCL_ATTR_TYPE_UINT8) {
      _light_color_hsv.s = attribute->data.value ? *(uint8_t *)attribute->data.value : 0;
      s_received = true;
    }

    // Process XY color if both X and Y have been received
    if (x_received && y_received) {
      _light_color_rgb = espXYToRgbColor(255, _light_color_xy.x, _light_color_xy.y, false);
      if (_on_light_color_change) {
        _on_light_color_change(_light_color_rgb.r, _light_color_rgb.g, _light_color_rgb.b);
      }
      if (_on_light_color_change_with_source) {
        _on_light_color_change_with_source(_light_color_rgb.r, _light_color_rgb.g, _light_color_rgb.b, src_endpoint, src_address);
      }
      x_received = false;  // Reset flags after processing
      y_received = false;
    }

    // Process HS color if both H and S have been received
    if (h_received && s_received) {
      _light_color_rgb = espHsvColorToRgbColor(_light_color_hsv);
      if (_on_light_color_change) {
        _on_light_color_change(_light_color_rgb.r, _light_color_rgb.g, _light_color_rgb.b);
      }
      if (_on_light_color_change_with_source) {
        _on_light_color_change_with_source(_light_color_rgb.r, _light_color_rgb.g, _light_color_rgb.b, src_endpoint, src_address);
      }
      h_received = false;  // Reset flags after processing
      s_received = false;
    }
  }
}
#endif  // CONFIG_ZB_ENABLED
