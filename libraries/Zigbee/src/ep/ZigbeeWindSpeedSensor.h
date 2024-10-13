/* Class of Zigbee WindSpeed sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#if SOC_IEEE802154_SUPPORTED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeWindSpeedSensor : public ZigbeeEP {
public:
  ZigbeeWindSpeedSensor(uint8_t endpoint);
  ~ZigbeeWindSpeedSensor();

  // Set the WindSpeed value in 0,01°C
  void setWindSpeed(float value);

  // Set the min and max value for the WindSpeed sensor
  void setMinMaxValue(float min, float max);

  // Set the tolerance value for the WindSpeed sensor
  void setTolerance(float tolerance);

  // Set the reporting interval for WindSpeed measurement in seconds and delta
  void setReporting(uint16_t min_interval, uint16_t max_interval, float delta);
  void reportWindSpeed();
};

#endif  //SOC_IEEE802154_SUPPORTED
