// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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

/* Class of Zigbee On/Off Switch endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"

/**
 * @brief On/off switch endpoint that controls bound lights (Zigbee HA).
 *
 * Sends on/off commands to bound devices, groups, or a specific address.
 */
class ZigbeeSwitch : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee switch endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeSwitch(uint8_t endpoint);
  ~ZigbeeSwitch() {}

  /**
   * @brief Toggle bound lights (on to off, or off to on).
   */
  void lightToggle();
  /**
   * @brief Toggle lights in a group.
   * @param group_addr Group address to control.
   */
  void lightToggle(uint16_t group_addr);
  /**
   * @brief Toggle a light by short address.
   * @param endpoint Target device endpoint.
   * @param short_addr Target device short address.
   */
  void lightToggle(uint8_t endpoint, uint16_t short_addr);
  /**
   * @brief Toggle a light by IEEE address.
   * @param endpoint Target device endpoint.
   * @param ieee_addr Target device IEEE address.
   */
  void lightToggle(uint8_t endpoint, const uint8_t *ieee_addr);

  /**
   * @brief Turn on bound lights.
   */
  void lightOn();
  /**
   * @brief Turn on lights in a group.
   * @param group_addr Group address to control.
   */
  void lightOn(uint16_t group_addr);
  /**
   * @brief Turn on a light by short address.
   * @param endpoint Target device endpoint.
   * @param short_addr Target device short address.
   */
  void lightOn(uint8_t endpoint, uint16_t short_addr);
  /**
   * @brief Turn on a light by IEEE address.
   * @param endpoint Target device endpoint.
   * @param ieee_addr Target device IEEE address.
   */
  void lightOn(uint8_t endpoint, const uint8_t *ieee_addr);

  /**
   * @brief Turn off bound lights.
   */
  void lightOff();
  /**
   * @brief Turn off lights in a group.
   * @param group_addr Group address to control.
   */
  void lightOff(uint16_t group_addr);
  /**
   * @brief Turn off a light by short address.
   * @param endpoint Target device endpoint.
   * @param short_addr Target device short address.
   */
  void lightOff(uint8_t endpoint, uint16_t short_addr);
  /**
   * @brief Turn off a light by IEEE address.
   * @param endpoint Target device endpoint.
   * @param ieee_addr Target device IEEE address.
   */
  void lightOff(uint8_t endpoint, const uint8_t *ieee_addr);

  /**
   * @brief Turn off lights with a specific effect.
   * @param effect_id Effect identifier.
   * @param effect_variant Effect variant.
   */
  void lightOffWithEffect(uint8_t effect_id, uint8_t effect_variant);
  /**
   * @brief Turn on lights with automatic turn-off after a specified time.
   * @param on_off_control Control byte.
   * @param time_on Time to stay on (in 1/10th seconds).
   * @param time_off Time to stay off (in 1/10th seconds).
   */
  void lightOnWithTimedOff(uint8_t on_off_control, uint16_t time_on, uint16_t time_off);
  /**
   * @brief Turn on lights by recalling a scene.
   */
  void lightOnWithSceneRecall();

  /**
   * @brief Request the on/off state from bound lights.
   */
  void getLightState();
  /**
   * @brief Request the on/off state from a group.
   * @param group_addr Group address.
   */
  void getLightState(uint16_t group_addr);
  /**
   * @brief Request the on/off state from a device by short address.
   * @param endpoint Target device endpoint.
   * @param short_addr Target device short address.
   */
  void getLightState(uint8_t endpoint, uint16_t short_addr);
  /**
   * @brief Request the on/off state from a device by IEEE address.
   * @param endpoint Target device endpoint.
   * @param ieee_addr Target device IEEE address.
   */
  void getLightState(uint8_t endpoint, const uint8_t *ieee_addr);

  /**
   * @brief Set a callback invoked when a bound light's state is reported.
   * @param callback Function called with the light state (true = on).
   */
  void onLightStateChange(void (*callback)(bool)) {
    _on_light_state_change = callback;
  }
  /**
   * @brief Set a callback invoked when a bound light's state is reported, with source info.
   * @param callback Function called with state, source endpoint, and source address.
   */
  void onLightStateChangeWithSource(void (*callback)(bool, uint8_t, ezb_address_t)) {
    _on_light_state_change_with_source = callback;
  }

private:
  // save instance of the class in order to use it in static functions
  static ZigbeeSwitch *_instance;
  zb_device_params_t *_device;

  void (*_on_light_state_change)(bool);
  void (*_on_light_state_change_with_source)(bool, uint8_t, ezb_address_t);

  void sendOnOffCommand(uint8_t on_off_cmd_id, const void *cmd_ctrl);
  void sendReadLightState(ezb_address_t dst_addr, uint8_t dst_ep);

  void findEndpoint(ezb_zdo_match_desc_req_t *cmd_req) override;
  void bindCb(const ezb_zdp_bind_req_result_t *result, void *user_ctx);
  void findCb(const ezb_zdo_match_desc_req_result_t *result, void *user_ctx);
  static void bindCbWrapper(const ezb_zdp_bind_req_result_t *result, void *user_ctx);
  static void findCbWrapper(const ezb_zdo_match_desc_req_result_t *result, void *user_ctx);
  void zbAttributeRead(uint16_t cluster_id, const ezb_zcl_attribute_t *attribute, uint8_t src_endpoint, ezb_address_t src_address) override;
};

#endif  // CONFIG_ZB_ENABLED
