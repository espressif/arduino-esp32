/* Class of Zigbee Temperature + Humidity sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

class ZigbeeTempSensor : public ZigbeeEP {
public:
  ZigbeeTempSensor(uint8_t endpoint);
  ~ZigbeeTempSensor() {}

  // Set the temperature value in 0,01°C
  bool setTemperature(float value);

  // Set the min and max value for the temperature sensor in 0,01°C
  bool setMinMaxValue(float min, float max);

  // Set the tolerance value for the temperature sensor in 0,01°C
  bool setTolerance(float tolerance);

  // Set the reporting interval for temperature measurement in seconds and delta (temp change in 0,01 °C)
  bool setReporting(uint16_t min_interval, uint16_t max_interval, float delta);

  // Report the temperature value
  bool reportTemperature();

  // Add humidity cluster to the temperature sensor device
  void addHumiditySensor(float min, float max, float tolerance);

  // Set the humidity value in 0,01%
  bool setHumidity(float value);

  // Set the reporting interval for humidity measurement in seconds and delta (humidity change in 0,01%)
  bool setHumidityReporting(uint16_t min_interval, uint16_t max_interval, float delta);

  // Report the humidity value
  bool reportHumidity();

  // Report the temperature and humidity values if humidity sensor is added
  bool report();

private:
  bool _humidity_sensor;
};

#endif  // CONFIG_ZB_ENABLED
