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

enum ZigbeeOccupancySensorType{
  ZIGBEE_OCCUPANCY_SENSOR_TYPE_PIR = 0,
  ZIGBEE_OCCUPANCY_SENSOR_TYPE_ULTRASONIC = 1,
  ZIGBEE_OCCUPANCY_SENSOR_TYPE_PIR_AND_ULTRASONIC = 2,
  ZIGBEE_OCCUPANCY_SENSOR_TYPE_PHYSICAL_CONTACT = 3
};

enum ZigbeeOccupancySensorTypeBitmap{
  ZIGBEE_OCCUPANCY_SENSOR_BITMAP_PIR = 0x01,
  ZIGBEE_OCCUPANCY_SENSOR_BITMAP_ULTRASONIC = 0x02,
  ZIGBEE_OCCUPANCY_SENSOR_BITMAP_PIR_AND_ULTRASONIC = 0x03,
  // ZIGBEE_OCCUPANCY_SENSOR_BITMAP_PHYSICAL_CONTACT = 0x04, // No info in cluster specification R8
  ZIGBEE_OCCUPANCY_SENSOR_BITMAP_PHYSICAL_CONTACT_AND_PIR = 0x05,
  ZIGBEE_OCCUPANCY_SENSOR_BITMAP_PHYSICAL_CONTACT_AND_ULTRASONIC = 0x06,
  ZIGBEE_OCCUPANCY_SENSOR_BITMAP_PHYSICAL_CONTACT_AND_PIR_AND_ULTRASONIC = 0x07,
  ZIGBEE_OCCUPANCY_SENSOR_BITMAP_DEFAULT = 0xff
};

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

  // Set the sensor type, see ZigbeeOccupancySensorType
  bool setSensorType(ZigbeeOccupancySensorType sensor_type, ZigbeeOccupancySensorTypeBitmap sensor_type_bitmap = ZIGBEE_OCCUPANCY_SENSOR_BITMAP_DEFAULT);

  // Set the occupied to unoccupied delay
  // Specifies the time delay, in seconds, before the sensor changes to its unoccupied state after the last detection of movement in the sensed area.
  bool setOccupiedToUnoccupiedDelay(ZigbeeOccupancySensorType sensor_type, uint16_t delay);

  // Set the unoccupied to occupied delay
  // Specifies the time delay, in seconds, before the sensor changes to its occupied state after the detection of movement in the sensed area.
  bool setUnoccupiedToOccupiedDelay(ZigbeeOccupancySensorType sensor_type, uint16_t delay);

  // Set the unoccupied to occupied threshold
  // Specifies the number of movement detection events that must occur in the period unoccupied to occupied delay, before the sensor changes to its occupied state.
  bool setUnoccupiedToOccupiedThreshold(ZigbeeOccupancySensorType sensor_type, uint8_t threshold);

  // Report the occupancy value
  bool report();

private:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;

  void (*_on_occupancy_config_change)(ZigbeeOccupancySensorType sensor_type, uint16_t occ_to_unocc_delay, uint16_t unocc_to_occ_delay, uint8_t unocc_to_occ_threshold);
  void occupancyConfigChanged(ZigbeeOccupancySensorType sensor_type, uint16_t occ_to_unocc_delay, uint16_t unocc_to_occ_delay, uint8_t unocc_to_occ_threshold); 

  // PIR sensor configuration
  uint16_t _pir_occ_to_unocc_delay;
  uint16_t _pir_unocc_to_occ_delay;
  uint8_t _pir_unocc_to_occ_threshold;

  // Ultrasonic sensor configuration
  uint16_t _ultrasonic_occ_to_unocc_delay;
  uint16_t _ultrasonic_unocc_to_occ_delay;
  uint8_t _ultrasonic_unocc_to_occ_threshold;

  // Physical contact sensor configuration
  uint16_t _physical_contact_occ_to_unocc_delay;
  uint16_t _physical_contact_unocc_to_occ_delay;
  uint8_t _physical_contact_unocc_to_occ_threshold;
};

#endif  // CONFIG_ZB_ENABLED
