/* Class of Zigbee On/Off Light endpoint inherited from common EP class */

#pragma once

#include "Zigbee_ep.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeLight : public Zigbee_EP {
  public:
    ZigbeeLight(uint8_t endpoint, void (*cb)(const esp_zb_zcl_set_attr_value_message_t *message));
    //ZigbeeLight(uint8_t endpoint, void (*cb)(const esp_zb_zcl_set_attr_value_message_t *message)) : ZigbeeLight(endpoint, cb, NULL) {}
    ~ZigbeeLight();

    void find_endpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req);
};