/* Class of Zigbee On/Off Light endpoint inherited from common EP class */

#pragma once

#include "Zigbee_ep.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeSwitch : public Zigbee_EP {
  public:
    ZigbeeSwitch(uint8_t endpoint, void (*cb)(const esp_zb_zcl_set_attr_value_message_t *message));//, void *arg);
    // ZigbeeSwitch(uint8_t endpoint, void (*cb)(const esp_zb_zcl_set_attr_value_message_t *message)) : ZigbeeSwitch(endpoint, cb, NULL) {}
    ~ZigbeeSwitch();

    void find_endpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req);
    static void bind_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx);
    static void find_cb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx);

    static uint8_t get_endpoint() { return _endpoint; }

};