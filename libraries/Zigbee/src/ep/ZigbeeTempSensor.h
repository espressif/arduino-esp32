/* Class of Zigbee Temperature sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#if SOC_IEEE802154_SUPPORTED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeTempSensor : public ZigbeeEP {
public:
  ZigbeeTempSensor(uint8_t endpoint);
  ~ZigbeeTempSensor();

  // Set the temperature value in 0,01°C
  void setTemperature(float value);

  // Set the min and max value for the temperature sensor in 0,01°C
  void setMinMaxValue(float min, float max);

  // Set the tolerance value for the temperature sensor in 0,01°C
  void setTolerance(float tolerance);

  // Set the reporting interval for temperature measurement in seconds and delta (temp change in 0,01 °C)
  void setReporting(uint16_t min_interval, uint16_t max_interval, float delta);
  void reportTemperature();
};

#endif  //SOC_IEEE802154_SUPPORTED
