/* Class of Zigbee On/Off Light endpoint inherited from common EP class */

#pragma once

#include "Zigbee_ep.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeLight : public Zigbee_EP {
  public:
    ZigbeeLight(uint8_t endpoint);
    ~ZigbeeLight();

    // methods to be implemented by the user by overwritting them
    virtual void set_on_off(bool value);

  private:
    void find_endpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req);

    void attribute_set(const esp_zb_zcl_set_attr_value_message_t *message) override;

};