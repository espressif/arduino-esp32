/* Class of Zigbee Analog sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

// clang-format off
#define ZIGBEE_DEFAULT_ANALOG_SENSOR_CONFIG()                     \
  {                                                                       \
    .basic_cfg =                                                          \
      {                                                                   \
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,        \
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,      \
      },                                                                  \
    .identify_cfg =                                                       \
      {                                                                   \
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE, \
      },                                                                  \
    .analog_value_cfg =                                                   \
    {                                                                     \
        .out_of_service = 0,                                              \
        .present_value = 0,                                               \
        .status_flags = 1,                                                \
      },                                                                  \
  }
// clang-format on

typedef struct zigbee_analog_sensor_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;
  esp_zb_identify_cluster_cfg_t identify_cfg;
  esp_zb_analog_value_cluster_cfg_t analog_value_cfg;
} zigbee_analog_sensor_cfg_t;

class ZigbeeAnalogSensor : public ZigbeeEP {
public:
  ZigbeeAnalogSensor(uint8_t endpoint);
  ~ZigbeeAnalogSensor() {}

  // Set the value
  void setAnalog(float analog);
/*
  // Set the min and max value for the sensor
  void setMinMaxValue(float min, float max);

  // Set the tolerance value for the sensor
  void setTolerance(float tolerance);
*/
  // Set the reporting interval for  in seconds and delta
  void setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta);

  // Report the analog value
  void report();
};

#endif  //SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED
