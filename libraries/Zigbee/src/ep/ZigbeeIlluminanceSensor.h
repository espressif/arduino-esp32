/* Class of Zigbee Illuminance sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

#define ZIGBEE_DEFAULT_ILLUMINANCE_SENSOR_CONFIG()                                          \
  {                                                                                         \
    .basic_cfg =                                                                            \
      {                                                                                     \
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,                          \
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,                        \
      },                                                                                    \
    .identify_cfg =                                                                         \
      {                                                                                     \
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,                   \
      },                                                                                    \
    .illuminance_cfg = {                                                                    \
      .measured_value = ESP_ZB_ZCL_ILLUMINANCE_MEASUREMENT_LIGHT_SENSOR_TYPE_DEFAULT_VALUE, \
      .min_value = ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MIN_MEASURED_VALUE_MIN_VALUE,    \
      .max_value = ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MAX_MEASURED_VALUE_MAX_VALUE,    \
    },                                                                                      \
  }

class ZigbeeIlluminanceSensor : public ZigbeeEP {
public:
  ZigbeeIlluminanceSensor(uint8_t endpoint);
  ~ZigbeeIlluminanceSensor() {}

  // Set the illuminance value
  bool setIlluminance(uint16_t value);

  // Set the min and max value for the illuminance sensor
  bool setMinMaxValue(uint16_t min, uint16_t max);

  // Set the tolerance value for the illuminance sensor
  bool setTolerance(uint16_t tolerance);

  // Set the reporting interval for illuminance measurement in seconds and delta
  bool setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta);

  // Report the illuminance value
  bool report();
};

#endif  // CONFIG_ZB_ENABLED
