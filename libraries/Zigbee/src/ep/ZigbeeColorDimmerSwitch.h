/* Class of Zigbee On/Off Switch endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#if SOC_IEEE802154_SUPPORTED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeColorDimmerSwitch : public ZigbeeEP {
public:
  ZigbeeColorDimmerSwitch(uint8_t endpoint);
  ~ZigbeeColorDimmerSwitch();

  // methods to control the color dimmable light
  void lightToggle();
  void lightToggle(uint16_t group_addr);
  void lightToggle(uint8_t endpoint, uint16_t short_addr);

  void lightOn();
  void lightOn(uint16_t group_addr);
  void lightOn(uint8_t endpoint, uint16_t short_addr);

  void lightOff();
  void lightOff(uint16_t group_addr);
  void lightOff(uint8_t endpoint, uint16_t short_addr);

  void lightOffWithEffect(uint8_t effect_id, uint8_t effect_variant);
  void lightOnWithTimedOff(uint8_t on_off_control, uint16_t time_on, uint16_t time_off);
  void lightOnWithSceneRecall();

  void setLightLevel(uint8_t level);
  void setLightLevel(uint8_t level, uint16_t group_addr);
  void setLightLevel(uint8_t level, uint8_t endpoint, uint16_t short_addr);

  void setLightColor(uint8_t red, uint8_t green, uint8_t blue);
  void setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint16_t group_addr);
  void setLightColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t endpoint, uint16_t short_addr);

  void setLightColorSaturation(uint8_t value);
  void setLightColorSaturation(uint8_t value, uint16_t group_addr);
  void setLightColorSaturation(uint8_t value, uint8_t endpoint, uint16_t short_addr);

  void setLightColorHue(uint8_t value);
  void setLightColorHue(uint8_t value, uint16_t group_addr);
  void setLightColorHue(uint8_t value, uint8_t endpoint, uint16_t short_addr);

private:
  // save instance of the class in order to use it in static functions
  static ZigbeeColorDimmerSwitch *_instance;

  void findEndpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req);
  static void bindCb(esp_zb_zdp_status_t zdo_status, void *user_ctx);
  static void findCb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx);

  void calculateXY(uint8_t red, uint8_t green, uint8_t blue, uint16_t &x, uint16_t &y);
};

#endif  //SOC_IEEE802154_SUPPORTED
