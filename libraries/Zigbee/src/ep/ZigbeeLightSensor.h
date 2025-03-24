/* Class of Zigbee Light sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

/*
the new macro works here, but should better be added to
esp-zigbee-sdk/components/esp-zigbee-lib/include/ha/esp_zigbee_ha_standard.h
*/
#define ESP_ZB_DEFAULT_LIGHT_SENSOR_CONFIG()                                                            \
    {                                                                                                   \
        .basic_cfg =                                                                                    \
            {                                                                                           \
                .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,                              \
                .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,                            \
            },                                                                                          \
        .identify_cfg =                                                                                 \
            {                                                                                           \
                .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,                       \
            },                                                                                          \
        .illuminance_cfg =                                                                              \
            {                                                                                           \
                .measured_value = ESP_ZB_ZCL_ILLUMINANCE_MEASUREMENT_LIGHT_SENSOR_TYPE_DEFAULT_VALUE,   \
                /*not sure with next two values, but there are no*/                                     \
                /*ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MIN_MEASURED_VALUE_DEFAULT and*/              \
                /*ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MAX_MEASURED_VALUE_DEFAULT*/                  \
                /*thats why I chose MIN_VALUE and MAX_VALUE*/                                           \
                .min_value = ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MIN_MEASURED_VALUE_MIN_VALUE,      \
                .max_value = ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MAX_MEASURED_VALUE_MAX_VALUE,      \
            },                                                                                          \
    }

class ZigbeeLightSensor : public ZigbeeEP {
public:
  ZigbeeLightSensor(uint8_t endpoint);
  ~ZigbeeLightSensor() {}

  // Set the light value
  void setIlluminance(int value);

  // Set the min and max value for the light sensor
  void setMinMaxValue(int min, int max);

  // Set the tolerance value for the light sensor
  void setTolerance(int tolerance);

  // Set the reporting interval for light measurement in seconds and delta
  void setReporting(uint16_t min_interval, uint16_t max_interval, int delta);

  // Report the light value
  void reportIlluminance();
};

#endif  // CONFIG_ZB_ENABLED
