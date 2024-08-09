/* Class of Zigbee Temperature sensor endpoint inherited from common EP class */

#pragma once

#include "Zigbee_ep.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeTempSensor : public Zigbee_EP {
  public:
    ZigbeeTempSensor(uint8_t endpoint);
    ~ZigbeeTempSensor();

    void find_endpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req);
};