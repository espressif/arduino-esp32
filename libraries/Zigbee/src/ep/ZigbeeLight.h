/* Class of Zigbee On/Off Light endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeLight : public ZigbeeEP {
public:
  ZigbeeLight(uint8_t endpoint);
  ~ZigbeeLight() {}

  // Use to set a cb function to be called on light change
  void onLightChange(void (*callback)(bool)) {
    _on_light_change = callback;
  }
  // Use to restore light state
  void restoreLight() {
    lightChanged();
  }
  // Use to control light state
  bool setLight(bool state);
  // Use to get light state
  bool getLightState() {
    return _current_state;
  }

private:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;
  //callback function to be called on light change
  void (*_on_light_change)(bool);
  void lightChanged();

  bool _current_state;
};

#endif  // CONFIG_ZB_ENABLED
