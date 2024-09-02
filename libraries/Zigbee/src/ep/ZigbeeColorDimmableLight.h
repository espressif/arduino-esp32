/* Class of Zigbee On/Off Light endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#if SOC_IEEE802154_SUPPORTED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeColorDimmableLight : public ZigbeeEP {
  public:
    ZigbeeColorDimmableLight(uint8_t endpoint);
    ~ZigbeeColorDimmableLight();

    uint8_t getCurrentLevel();
    uint16_t getCurrentColorX();
    uint16_t getCurrentColorY();
    void calculateRGB(uint16_t x, uint16_t y, uint8_t &red, uint8_t &green, uint8_t &blue);

    // methods to be implemented by the user by overwritting them
    virtual void setOnOff(bool value);
    virtual void sceneControl(bool value);
    virtual void setOnOffTime(uint16_t value);
    virtual void setOffWaitTime(uint16_t value);
    virtual void setLevel(uint8_t value);
    virtual void setColor(uint8_t red, uint8_t green, uint8_t blue);
    virtual void setColorSaturation(uint8_t value);
    virtual void setColorHue(uint8_t value);

  private:
    void attribute_set(const esp_zb_zcl_set_attr_value_message_t *message) override;

};

#endif //SOC_IEEE802154_SUPPORTED
