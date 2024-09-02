/* Class of Zigbee On/Off Light endpoint inherited from common EP class */

#pragma once

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeLight : public ZigbeeEP {
  public:
    ZigbeeLight(uint8_t endpoint);
    ~ZigbeeLight();

    // methods to be implemented by the user by overwritting them
    virtual void setOnOff(bool value);
    virtual void sceneControl(bool value);
    virtual void setOnOffTime(uint16_t value);
    virtual void setOffWaitTime(uint16_t value);

  private:
    void attribute_set(const esp_zb_zcl_set_attr_value_message_t *message) override;

};