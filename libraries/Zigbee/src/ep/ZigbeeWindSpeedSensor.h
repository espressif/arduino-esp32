/* Class of Zigbee WindSpeed sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

#define ZIGBEE_DEFAULT_WIND_SPEED_SENSOR_CONFIG()                                         \
  {                                                                                       \
    .basic_cfg =                                                                          \
      {                                                                                   \
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,                        \
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,                      \
      },                                                                                  \
    .identify_cfg =                                                                       \
      {                                                                                   \
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,                 \
      },                                                                                  \
    .wind_speed_meas_cfg = {                                                              \
      .measured_value = ESP_ZB_ZCL_WIND_SPEED_MEASUREMENT_MEASURED_VALUE_DEFAULT,         \
      .min_measured_value = ESP_ZB_ZCL_WIND_SPEED_MEASUREMENT_MIN_MEASURED_VALUE_DEFAULT, \
      .max_measured_value = ESP_ZB_ZCL_WIND_SPEED_MEASUREMENT_MAX_MEASURED_VALUE_DEFAULT, \
    },                                                                                    \
  }

typedef struct zigbee_wind_speed_sensor_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;       /*!<  Basic cluster configuration, @ref esp_zb_basic_cluster_cfg_s */
  esp_zb_identify_cluster_cfg_t identify_cfg; /*!<  Identify cluster configuration, @ref esp_zb_identify_cluster_cfg_s */
  esp_zb_wind_speed_measurement_cluster_cfg_t
    wind_speed_meas_cfg; /*!<  Wind speed measurement cluster configuration, @ref esp_zb_wind_speed_measurement_cluster_cfg_s */
} zigbee_wind_speed_sensor_cfg_t;

class ZigbeeWindSpeedSensor : public ZigbeeEP {
public:
  ZigbeeWindSpeedSensor(uint8_t endpoint);
  ~ZigbeeWindSpeedSensor() {}

  // Set the WindSpeed value in 0,01 m/s
  bool setWindSpeed(float value);

  // Set the min and max value for the WindSpeed sensor
  bool setMinMaxValue(float min, float max);

  // Set the tolerance value for the WindSpeed sensor
  bool setTolerance(float tolerance);

  // Set the reporting interval for WindSpeed measurement in seconds and delta
  bool setReporting(uint16_t min_interval, uint16_t max_interval, float delta);
  bool reportWindSpeed();
};

#endif  //CONFIG_ZB_ENABLED
