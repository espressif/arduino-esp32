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

/* Class of Zigbee Color Dimmer Switch endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"

/**
 * @brief Level step direction for `setLightLevelStep` (ZCL Step command: 0 = Up, 1 = Down).
 */
enum ZigbeeLevelStepDirection {
  ZIGBEE_LEVEL_STEP_UP = 0,  /**< Increase CurrentLevel. */
  ZIGBEE_LEVEL_STEP_DOWN = 1 /**< Decrease CurrentLevel. */
};

/**
 * @brief Color dimmer switch endpoint for on/off, dimming, and color control of bound lights (Zigbee HA).
 */
class ZigbeeColorDimmerSwitch : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee color dimmer switch endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeColorDimmerSwitch(uint8_t endpoint);
  ~ZigbeeColorDimmerSwitch() {}

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
   * @brief Turn on lights with scene recall.
   */
  void lightOnWithSceneRecall();

  /**
   * @brief Set the brightness of bound lights.
   * @param level ZCL CurrentLevel (0-255, sent unchanged; 255 is full brightness).
   */
  void setLightLevel(uint8_t level);
  /**
   * @brief Set the brightness of lights in a group.
   * @param level ZCL CurrentLevel (0-255, sent unchanged; 255 is full brightness).
   * @param group_addr Group address to control.
   */
  void setLightLevel(uint8_t level, uint16_t group_addr);
  /**
   * @brief Set the brightness of a light by short address.
   * @param level ZCL CurrentLevel (0-255, sent unchanged; 255 is full brightness).
   * @param endpoint Target device endpoint.
   * @param short_addr Target device short address.
   */
  void setLightLevel(uint8_t level, uint8_t endpoint, uint16_t short_addr);
  /**
   * @brief Set the brightness of a light by IEEE address.
   * @param level ZCL CurrentLevel (0-255, sent unchanged; 255 is full brightness).
   * @param endpoint Target device endpoint.
   * @param ieee_addr Target device IEEE address.
   */
  void setLightLevel(uint8_t level, uint8_t endpoint, const uint8_t *ieee_addr);

  /**
   * @brief Step bound lights' level by a fixed size in the given direction.
   *
   * Transition time is in tenths of a second (1 unit = 100 ms). Use `0xFFFF` to move
   * as fast as the device is able. If the device cannot move at a variable rate, it
   * may disregard the transition time.
   *
   * @param direction `ZIGBEE_LEVEL_STEP_UP` (0) or `ZIGBEE_LEVEL_STEP_DOWN` (1).
   * @param step_size Number of level units to step (e.g. 1-254).
   * @param transition_time Time to perform the step, in tenths of a second.
   */
  void setLightLevelStep(ZigbeeLevelStepDirection direction, uint8_t step_size, uint16_t transition_time);
  /**
   * @brief Step lights in a group by a fixed size in the given direction.
   * @param direction `ZIGBEE_LEVEL_STEP_UP` or `ZIGBEE_LEVEL_STEP_DOWN`.
   * @param step_size Number of level units to step.
   * @param transition_time Time to perform the step, in tenths of a second (`0xFFFF` = as fast as able).
   * @param group_addr Group address to control.
   */
  void setLightLevelStep(ZigbeeLevelStepDirection direction, uint8_t step_size, uint16_t transition_time, uint16_t group_addr);
  /**
   * @brief Step a light's level by short address.
   * @param direction `ZIGBEE_LEVEL_STEP_UP` or `ZIGBEE_LEVEL_STEP_DOWN`.
   * @param step_size Number of level units to step.
   * @param transition_time Time to perform the step, in tenths of a second (`0xFFFF` = as fast as able).
   * @param endpoint Target device endpoint.
   * @param short_addr Target device short address.
   */
  void setLightLevelStep(ZigbeeLevelStepDirection direction, uint8_t step_size, uint16_t transition_time, uint8_t endpoint, uint16_t short_addr);
  /**
   * @brief Step a light's level by IEEE address.
   * @param direction `ZIGBEE_LEVEL_STEP_UP` or `ZIGBEE_LEVEL_STEP_DOWN`.
   * @param step_size Number of level units to step.
   * @param transition_time Time to perform the step, in tenths of a second (`0xFFFF` = as fast as able).
   * @param endpoint Target device endpoint.
   * @param ieee_addr Target device IEEE address.
   */
  void setLightLevelStep(ZigbeeLevelStepDirection direction, uint8_t step_size, uint16_t transition_time, uint8_t endpoint, const uint8_t *ieee_addr);

  /**
   * @brief Set the color of bound lights using RGB values.
   * @param red Red component (0-255).
   * @param green Green component (0-255).
   * @param blue Blue component (0-255).
   */
  void setLightColor(uint8_t red, uint8_t green, uint8_t blue);
  /**
   * @brief Set the color of lights in a group using RGB values.
   * @param red Red component (0-255).
   * @param green Green component (0-255).
   * @param blue Blue component (0-255).
   * @param group_addr Group address to control.
   */
  void setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint16_t group_addr);
  /**
   * @brief Set the color of a light by short address using RGB values.
   * @param red Red component (0-255).
   * @param green Green component (0-255).
   * @param blue Blue component (0-255).
   * @param endpoint Target device endpoint.
   * @param short_addr Target device short address.
   */
  void setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t endpoint, uint16_t short_addr);
  /**
   * @brief Set the color of a light by IEEE address using RGB values.
   * @param red Red component (0-255).
   * @param green Green component (0-255).
   * @param blue Blue component (0-255).
   * @param endpoint Target device endpoint.
   * @param ieee_addr Target device IEEE address.
   */
  void setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t endpoint, const uint8_t *ieee_addr);

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
   * @brief Request the brightness level from bound lights.
   */
  void getLightLevel();
  /**
   * @brief Request the brightness level from a group.
   * @param group_addr Group address.
   */
  void getLightLevel(uint16_t group_addr);
  /**
   * @brief Request the brightness level from a device by short address.
   * @param endpoint Target device endpoint.
   * @param short_addr Target device short address.
   */
  void getLightLevel(uint8_t endpoint, uint16_t short_addr);
  /**
   * @brief Request the brightness level from a device by IEEE address.
   * @param endpoint Target device endpoint.
   * @param ieee_addr Target device IEEE address.
   */
  void getLightLevel(uint8_t endpoint, const uint8_t *ieee_addr);

  /**
   * @brief Request the XY/RGB color from bound lights.
   */
  void getLightColor();
  /**
   * @brief Request the XY/RGB color from a group.
   * @param group_addr Group address.
   */
  void getLightColor(uint16_t group_addr);
  /**
   * @brief Request the XY/RGB color from a device by short address.
   * @param endpoint Target device endpoint.
   * @param short_addr Target device short address.
   */
  void getLightColor(uint8_t endpoint, uint16_t short_addr);
  /**
   * @brief Request the XY/RGB color from a device by IEEE address.
   * @param endpoint Target device endpoint.
   * @param ieee_addr Target device IEEE address.
   */
  void getLightColor(uint8_t endpoint, const uint8_t *ieee_addr);

  /**
   * @brief Request the hue/saturation color from bound lights.
   */
  void getLightColorHS();
  /**
   * @brief Request the hue/saturation color from a group.
   * @param group_addr Group address.
   */
  void getLightColorHS(uint16_t group_addr);
  /**
   * @brief Request the hue/saturation color from a device by short address.
   * @param endpoint Target device endpoint.
   * @param short_addr Target device short address.
   */
  void getLightColorHS(uint8_t endpoint, uint16_t short_addr);
  /**
   * @brief Request the hue/saturation color from a device by IEEE address.
   * @param endpoint Target device endpoint.
   * @param ieee_addr Target device IEEE address.
   */
  void getLightColorHS(uint8_t endpoint, const uint8_t *ieee_addr);

  /**
   * @brief Set a callback invoked when a bound light's on/off state is reported.
   * @param callback Function called with the light state (true = on).
   */
  void onLightStateChange(void (*callback)(bool)) {
    _on_light_state_change = callback;
  }
  /**
   * @brief Set a callback invoked when a bound light's on/off state is reported, with source info.
   * @param callback Function called with state, source endpoint, and source address.
   */
  void onLightStateChangeWithSource(void (*callback)(bool, uint8_t, ezb_address_t)) {
    _on_light_state_change_with_source = callback;
  }
  /**
   * @brief Set a callback invoked when a bound light's brightness is reported.
   * @param callback Function called with ZCL CurrentLevel (0-255).
   */
  void onLightLevelChange(void (*callback)(uint8_t)) {
    _on_light_level_change = callback;
  }
  /**
   * @brief Set a callback invoked when a bound light's brightness is reported, with source info.
   * @param callback Function called with ZCL CurrentLevel (0-255), source endpoint, and source address.
   */
  void onLightLevelChangeWithSource(void (*callback)(uint8_t, uint8_t, ezb_address_t)) {
    _on_light_level_change_with_source = callback;
  }
  /**
   * @brief Set a callback invoked when a bound light's RGB color is reported.
   * @param callback Function called with red, green, and blue (0-255).
   */
  void onLightColorChange(void (*callback)(uint8_t, uint8_t, uint8_t)) {
    _on_light_color_change = callback;
  }
  /**
   * @brief Set a callback invoked when a bound light's RGB color is reported, with source info.
   * @param callback Function called with red, green, blue, source endpoint, and source address.
   */
  void onLightColorChangeWithSource(void (*callback)(uint8_t, uint8_t, uint8_t, uint8_t, ezb_address_t)) {
    _on_light_color_change_with_source = callback;
  }

private:
  // save instance of the class in order to use it in static functions
  static ZigbeeColorDimmerSwitch *_instance;
  zb_device_params_t *_device;

  espHsvColor_t _light_color_hsv;
  espXyColor_t _light_color_xy;
  espRgbColor_t _light_color_rgb;

  void (*_on_light_state_change)(bool);
  void (*_on_light_state_change_with_source)(bool, uint8_t, ezb_address_t);
  void (*_on_light_level_change)(uint8_t);
  void (*_on_light_level_change_with_source)(uint8_t, uint8_t, ezb_address_t);
  void (*_on_light_color_change)(uint8_t, uint8_t, uint8_t);
  void (*_on_light_color_change_with_source)(uint8_t, uint8_t, uint8_t, uint8_t, ezb_address_t);

  // On/Off, Level, Color and read senders shared by all addressing overloads.
  void sendOnOffCommand(uint8_t on_off_cmd_id, const void *cmd_ctrl);
  void sendMoveToLevel(uint8_t level, const void *cmd_ctrl);
  void sendLevelStep(ZigbeeLevelStepDirection direction, uint8_t step_size, uint16_t transition_time, const void *cmd_ctrl);
  void sendMoveToColor(espXyColor_t xy_color, const void *cmd_ctrl);
  void sendReadAttributes(ezb_address_t dst_addr, uint8_t dst_ep, uint16_t cluster_id, uint16_t *attr_field, uint8_t attr_number, const char *err_msg);

  void findEndpoint(ezb_zdo_match_desc_req_t *cmd_req) override;
  void bindCb(const ezb_zdp_bind_req_result_t *result, void *user_ctx);
  void findCb(const ezb_zdo_match_desc_req_result_t *result, void *user_ctx);
  static void bindCbWrapper(const ezb_zdp_bind_req_result_t *result, void *user_ctx);
  static void findCbWrapper(const ezb_zdo_match_desc_req_result_t *result, void *user_ctx);

  void zbAttributeRead(uint16_t cluster_id, const ezb_zcl_attribute_t *attribute, uint8_t src_endpoint, ezb_address_t src_address) override;
};

#endif  // CONFIG_ZB_ENABLED
