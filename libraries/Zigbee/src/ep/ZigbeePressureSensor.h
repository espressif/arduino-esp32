/* Class of Zigbee Pressure sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

// clang-format off
#define ZIGBEE_DEFAULT_PRESSURE_SENSOR_CONFIG()                                                     \
    {                                                                                               \
        .basic_cfg =                                                                                \
            {                                                                                       \
                .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,                          \
                .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,                        \
            },                                                                                      \
        .identify_cfg =                                                                             \
            {                                                                                       \
                .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,                   \
            },                                                                                      \
        .pressure_meas_cfg =                                                                        \
            {                                                                                       \
                .measured_value = ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_VALUE_DEFAULT_VALUE,         \
                .min_value = ESP_ZB_ZCL_PATTR_RESSURE_MEASUREMENT_MIN_VALUE_DEFAULT_VALUE,          \
                .max_value = ESP_ZB_ZCL_PATTR_RESSURE_MEASUREMENT_MAX_VALUE_DEFAULT_VALUE,          \
            },                                                                                      \
    }
// clang-format on

typedef struct zigbee_pressure_sensor_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;
  esp_zb_identify_cluster_cfg_t identify_cfg;
  esp_zb_pressure_meas_cluster_cfg_t pressure_meas_cfg;
} zigbee_pressure_sensor_cfg_t;

class ZigbeePressureSensor : public ZigbeeEP {
public:
  ZigbeePressureSensor(uint8_t endpoint);
  ~ZigbeePressureSensor() {}

  // Set the pressure value in 1 hPa
  bool setPressure(int16_t value);

  // Set the min and max value for the pressure sensor in 1 hPa
  bool setMinMaxValue(int16_t min, int16_t max);

  // Set the tolerance value for the pressure sensor in 1 hPa
  bool setTolerance(uint16_t tolerance);

  // Set the reporting interval for pressure measurement in seconds and delta (pressure change in 1 hPa)
  bool setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta);

  // Report the pressure value
  bool report();
};

#endif  // CONFIG_ZB_ENABLED
