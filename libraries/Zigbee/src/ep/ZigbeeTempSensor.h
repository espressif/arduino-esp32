/* Class of Zigbee Temperature + Humidity sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeTempSensor : public ZigbeeEP {
public:
  ZigbeeTempSensor(uint8_t endpoint);
  ~ZigbeeTempSensor() {}

  // Set the temperature value in 0,01°C
  void setTemperature(float value);

  // Set the min and max value for the temperature sensor in 0,01°C
  void setMinMaxValue(float min, float max);

  // Set the tolerance value for the temperature sensor in 0,01°C
  void setTolerance(float tolerance);

  // Set the reporting interval for temperature measurement in seconds and delta (temp change in 0,01 °C)
  void setReporting(uint16_t min_interval, uint16_t max_interval, float delta);

  // Report the temperature value
  void reportTemperature();

  // Add humidity cluster to the temperature sensor device
  void addHumiditySensor(float min, float max, float tolerance);

  // Set the humidity value in 0,01%
  void setHumidity(float value);

  // Set the reporting interval for humidity measurement in seconds and delta (humidity change in 0,01%)
  void setHumidityReporting(uint16_t min_interval, uint16_t max_interval, float delta);

  // Report the humidity value
  void reportHumidity();

  // Report the temperature and humidity values if humidity sensor is added
  void report();

private:
  bool _humidity_sensor;
};

#endif  //SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED
