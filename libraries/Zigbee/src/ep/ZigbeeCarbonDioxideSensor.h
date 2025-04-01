/* Class of Zigbee Pressure sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

// clang-format off
#define ZIGBEE_DEFAULT_CARBON_DIOXIDE_SENSOR_CONFIG()                     \
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
    .carbon_dioxide_meas_cfg =                                            \
      {                                                                   \
        .measured_value = 0.0,                                            \
        .min_measured_value = 0.0,                                        \
        .max_measured_value = 1.0,                                        \
      },                                                                  \
  }
// clang-format on

typedef struct zigbee_carbon_dioxide_sensor_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;
  esp_zb_identify_cluster_cfg_t identify_cfg;
  esp_zb_carbon_dioxide_measurement_cluster_cfg_t carbon_dioxide_meas_cfg;
} zigbee_carbon_dioxide_sensor_cfg_t;

class ZigbeeCarbonDioxideSensor : public ZigbeeEP {
public:
  ZigbeeCarbonDioxideSensor(uint8_t endpoint);
  ~ZigbeeCarbonDioxideSensor() {}

  // Set the carbon dioxide value in ppm
  bool setCarbonDioxide(float carbon_dioxide);

  // Set the min and max value for the carbon dioxide sensor in ppm
  bool setMinMaxValue(float min, float max);

  // Set the tolerance value for the carbon dioxide sensor in ppm
  bool setTolerance(float tolerance);

  // Set the reporting interval for carbon dioxide measurement in seconds and delta (carbon dioxide change in ppm)
  // NOTE: Delta reporting is currently not supported by the carbon dioxide sensor
  bool setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta);

  // Report the carbon dioxide value
  bool report();
};

#endif  // CONFIG_ZB_ENABLED
