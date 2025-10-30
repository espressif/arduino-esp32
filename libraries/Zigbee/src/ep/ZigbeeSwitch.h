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

/* Class of Zigbee On/Off Switch endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeSwitch : public ZigbeeEP {
public:
  ZigbeeSwitch(uint8_t endpoint);
  ~ZigbeeSwitch() {}

  // methods to control the on/off light
  void lightToggle();
  void lightToggle(uint16_t group_addr);
  void lightToggle(uint8_t endpoint, uint16_t short_addr);
  void lightToggle(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

  void lightOn();
  void lightOn(uint16_t group_addr);
  void lightOn(uint8_t endpoint, uint16_t short_addr);
  void lightOn(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

  void lightOff();
  void lightOff(uint16_t group_addr);
  void lightOff(uint8_t endpoint, uint16_t short_addr);
  void lightOff(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

  void lightOffWithEffect(uint8_t effect_id, uint8_t effect_variant);
  void lightOnWithTimedOff(uint8_t on_off_control, uint16_t time_on, uint16_t time_off);
  void lightOnWithSceneRecall();

  void getLightState();
  void getLightState(uint16_t group_addr);
  void getLightState(uint8_t endpoint, uint16_t short_addr);
  void getLightState(uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

  void onLightStateChange(void (*callback)(bool)) {
    _on_light_state_change = callback;
  }
  void onLightStateChangeWithSource(void (*callback)(bool, uint8_t, esp_zb_zcl_addr_t)) {
    _on_light_state_change_with_source = callback;
  }

private:
  // save instance of the class in order to use it in static functions
  static ZigbeeSwitch *_instance;
  zb_device_params_t *_device;

  void (*_on_light_state_change)(bool);
  void (*_on_light_state_change_with_source)(bool, uint8_t, esp_zb_zcl_addr_t);

  void findEndpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req);
  void bindCb(esp_zb_zdp_status_t zdo_status, void *user_ctx);
  void findCb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx);
  static void findCbWrapper(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx);
  static void bindCbWrapper(esp_zb_zdp_status_t zdo_status, void *user_ctx);
  void zbAttributeRead(uint16_t cluster_id, const esp_zb_zcl_attribute_t *attribute, uint8_t src_endpoint, esp_zb_zcl_addr_t src_address) override;
};

#endif  // CONFIG_ZB_ENABLED
