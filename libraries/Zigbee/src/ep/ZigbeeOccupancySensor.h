/* Class of Zigbee Pressure sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

// clang-format off
#define ZIGBEE_DEFAULT_OCCUPANCY_SENSOR_CONFIG()                                             \
  {                                                                                          \
    .basic_cfg =                                                                             \
      {                                                                                      \
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,                           \
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,                         \
      },                                                                                     \
    .identify_cfg =                                                                          \
      {                                                                                      \
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,                    \
      },                                                                                     \
    .occupancy_meas_cfg =                                                                    \
      {                                                                                      \
        .occupancy = ESP_ZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_UNOCCUPIED,                      \
        .sensor_type = ESP_ZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_PIR,               \
        .sensor_type_bitmap = (1 << ESP_ZB_ZCL_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_PIR), \
      },                                                                                     \
  }
// clang-format on

typedef struct zigbee_occupancy_sensor_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;
  esp_zb_identify_cluster_cfg_t identify_cfg;
  esp_zb_occupancy_sensing_cluster_cfg_t occupancy_meas_cfg;
} zigbee_occupancy_sensor_cfg_t;

class ZigbeeOccupancySensor : public ZigbeeEP {
public:
  ZigbeeOccupancySensor(uint8_t endpoint);
  ~ZigbeeOccupancySensor() {}

  // Set the occupancy value. True for occupied, false for unoccupied
  bool setOccupancy(bool occupied);

  // Set the sensor type, see esp_zb_zcl_occupancy_sensing_occupancy_sensor_type_t
  bool setSensorType(uint8_t sensor_type);

  // Report the occupancy value
  bool report();
};

#endif  // CONFIG_ZB_ENABLED
