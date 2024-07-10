/* Class of Zigbee Temperature sensor endpoint inherited from common EP class */

#pragma once

#include "Zigbee_ep.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeThermostat : public Zigbee_EP {
  public:
    ZigbeeThermostat(uint8_t endpoint, void (*cb)(const esp_zb_zcl_set_attr_value_message_t *message));
    //ZigbeeThermostat(uint8_t endpoint, void (*cb)(const esp_zb_zcl_set_attr_value_message_t *message)) : ZigbeeTempSensor(endpoint, cb, NULL) {}
    ~ZigbeeThermostat();

    void find_endpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req);
    static void bind_cb(esp_zb_zdp_status_t zdo_status, void *user_ctx);
    static void find_cb(esp_zb_zdp_status_t zdo_status, uint16_t addr, uint8_t endpoint, void *user_ctx);

};