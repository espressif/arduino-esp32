/* Class of Zigbee On/Off Light endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#if SOC_IEEE802154_SUPPORTED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeLight : public ZigbeeEP {
public:
  ZigbeeLight(uint8_t endpoint);
  ~ZigbeeLight();

  // Use tp set a cb function to be called on light change
  void onLightChange(void (*callback)(bool)) {
    _on_light_change = callback;
  }
  void restoreLight() {
    lightChanged();
  }

private:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;
  //callback function to be called on light change
  void (*_on_light_change)(bool);
  void lightChanged();

  bool _current_state;
};

#endif  //SOC_IEEE802154_SUPPORTED
