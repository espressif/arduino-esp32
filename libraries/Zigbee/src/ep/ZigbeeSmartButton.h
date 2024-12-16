/* Class of Zigbee On/Off Switch endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeSmartButton : public ZigbeeEP {
public:
  ZigbeeSmartButton(uint8_t endpoint);
  ~ZigbeeSmartButton();

  void toggle();
};

#endif  //SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED
