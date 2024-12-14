/* Class of Zigbee Flow sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

// clang-format off
#define ZIGBEE_DEFAULT_FLOW_SENSOR_CONFIG()                               \
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
    .flow_meas_cfg =                                                      \
      {                                                                   \
        .measured_value = 0,                                              \
        .min_value = 0,                                                   \
        .max_value = 0x7FFF,                                              \
      },                                                                  \
  }
// clang-format on

typedef struct zigbee_flow_sensor_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;
  esp_zb_identify_cluster_cfg_t identify_cfg;
  esp_zb_flow_meas_cluster_cfg_t flow_meas_cfg;
} zigbee_flow_sensor_cfg_t;

class ZigbeeFlowSensor : public ZigbeeEP {
public:
  ZigbeeFlowSensor(uint8_t endpoint);
  ~ZigbeeFlowSensor();

  // Set the flow value in 0,1 m3/h
  void setFlow(float value);

  // Set the min and max value for the flow sensor in 0,1 m3/h
  void setMinMaxValue(float min, float max);

  // Set the tolerance value for the flow sensor in 0,01 m3/h
  void setTolerance(float tolerance);

  // Set the reporting interval for flow measurement in seconds and delta (temp change in 0,1 m3/h)
  void setReporting(uint16_t min_interval, uint16_t max_interval, float delta);

  // Report the flow value
  void report();
};

#endif  //SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED
