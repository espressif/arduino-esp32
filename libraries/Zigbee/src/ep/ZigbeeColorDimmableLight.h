/* Class of Zigbee On/Off Light endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeColorDimmableLight : public ZigbeeEP {
public:
  ZigbeeColorDimmableLight(uint8_t endpoint);
  ~ZigbeeColorDimmableLight() {}

  void onLightChange(void (*callback)(bool, uint8_t, uint8_t, uint8_t, uint8_t)) {
    _on_light_change = callback;
  }
  void restoreLight() {
    lightChanged();
  }

  void setLightState(bool state);
  void setLightLevel(uint8_t level);
  void setLightColor(uint8_t red, uint8_t green, uint8_t blue);
  void setLightColor(espRgbColor_t rgb_color);
  void setLightColor(espHsvColor_t hsv_color);
  void setLight(bool state, uint8_t level, uint8_t red, uint8_t green, uint8_t blue);

  bool getLightState() {
    return _current_state;
  }
  uint8_t getLightLevel() {
    return _current_level;
  }
  espRgbColor_t getLightColor() {
    return _current_color;
  }
  uint8_t getLightRed() {
    return _current_color.r;
  }
  uint8_t getLightGreen() {
    return _current_color.g;
  }
  uint8_t getLightBlue() {
    return _current_color.b;
  }

private:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;

  uint16_t getCurrentColorX();
  uint16_t getCurrentColorY();
  uint16_t getCurrentColorHue();
  uint8_t getCurrentColorSaturation();

  void lightChanged();
  //callback function to be called on light change (State, R, G, B, Level)
  void (*_on_light_change)(bool, uint8_t, uint8_t, uint8_t, uint8_t);

  bool _current_state;
  uint8_t _current_level;
  espRgbColor_t _current_color;
};

#endif  //SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED
