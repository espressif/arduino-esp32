/* Class of Zigbee On/Off Switch endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeColorDimmerSwitch : public ZigbeeEP {
public:
  ZigbeeColorDimmerSwitch(uint8_t endpoint);
  ~ZigbeeColorDimmerSwitch() {}

  // methods to control the color dimmable light
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

  void setLightLevel(uint8_t level);
  void setLightLevel(uint8_t level, uint16_t group_addr);
  void setLightLevel(uint8_t level, uint8_t endpoint, uint16_t short_addr);
  void setLightLevel(uint8_t level, uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

  void setLightColor(uint8_t red, uint8_t green, uint8_t blue);
  void setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint16_t group_addr);
  void setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t endpoint, uint16_t short_addr);
  void setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t endpoint, esp_zb_ieee_addr_t ieee_addr);

private:
  // save instance of the class in order to use it in static functions
  static ZigbeeColorDimmerSwitch *_instance;
  zb_device_params_t *_device;

  void findEndpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req);
  void bindCb(esp_zb_zdp_status_t zdo_status, void *user_ctx);
  void findCb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx);
  static void bindCbWrapper(esp_zb_zdp_status_t zdo_status, void *user_ctx);
  static void findCbWrapper(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx);

  void calculateXY(uint8_t red, uint8_t green, uint8_t blue, uint16_t &x, uint16_t &y);
};

#endif  // CONFIG_ZB_ENABLED
