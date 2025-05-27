/* Class of Zigbee Pressure sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

// clang-format off
#define ZIGBEE_DEFAULT_ELECTRICAL_MEASUREMENT_CONFIG()                                              \
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
        .electrical_measurement_cfg =                                                               \
            {                                                                                       \
                .measured_type = 0x00,                                                              \
            },                                                                                      \
    }
// clang-format on

enum ZIGBEE_DC_MEASUREMENT_TYPE {
  ZIGBEE_DC_MEASUREMENT_TYPE_VOLTAGE = 0x0001,
  ZIGBEE_DC_MEASUREMENT_TYPE_CURRENT = 0x0002,
  ZIGBEE_DC_MEASUREMENT_TYPE_POWER = 0x0003,
};

enum ZIGBEE_AC_MEASUREMENT_TYPE {
  ZIGBEE_AC_MEASUREMENT_TYPE_VOLTAGE = 0x0001,
  ZIGBEE_AC_MEASUREMENT_TYPE_CURRENT = 0x0002,
  ZIGBEE_AC_MEASUREMENT_TYPE_POWER = 0x0003,
  ZIGBEE_AC_MEASUREMENT_TYPE_POWER_FACTOR = 0x0004,
  ZIGBEE_AC_MEASUREMENT_TYPE_FREQUENCY = 0x0005,
};

enum ZIGBEE_AC_PHASE_TYPE {
  ZIGBEE_AC_PHASE_TYPE_NON_SPECIFIC = 0x0000,
  ZIGBEE_AC_PHASE_TYPE_A = 0x0001,
  ZIGBEE_AC_PHASE_TYPE_B = 0x0002,
  ZIGBEE_AC_PHASE_TYPE_C = 0x0003,
};

typedef struct zigbee_electrical_measurement_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;
  esp_zb_identify_cluster_cfg_t identify_cfg;
  esp_zb_electrical_meas_cluster_cfg_t electrical_measurement_cfg;
} zigbee_electrical_measurement_cfg_t;

class ZigbeeElectricalMeasurement : public ZigbeeEP {
public:
  ZigbeeElectricalMeasurement(uint8_t endpoint);
  ~ZigbeeElectricalMeasurement() {}

  /**
   * @brief DC measurement methods
   */
  // Add a DC measurement type
  bool addDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type);
  // Set the DC measurement value for the given measurement type
  bool setDCMeasurement(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, int16_t value);
  // Set the DC min and max value for the given measurement type
  bool setDCMinMaxValue(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, int16_t min, int16_t max);
  // Set the DC multiplier and divisor for the given measurement type
  bool setDCMultiplierDivisor(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, uint16_t multiplier, uint16_t divisor);
  // Set the DC reporting interval for the given measurement type in seconds and delta (measurement change)
  bool setDCReporting(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type, uint16_t min_interval, uint16_t max_interval, int16_t delta);
  // Report the DC measurement value for the given measurement type
  bool reportDC(ZIGBEE_DC_MEASUREMENT_TYPE measurement_type);

  /**
   * @brief AC measurement methods
   */
  // Add an AC measurement type for selected phase type
  bool addACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type);
  // Set the AC measurement value for the given measurement type and phase type (uint16_t for voltage, current and frequency, int32_t for power)
  bool setACMeasurement(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type, int32_t value);
  // Set the AC min and max value for the given measurement type and phase type (uint16_t for voltage, current and frequency, int32_t for power)
  bool setACMinMaxValue(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type, int32_t min, int32_t max);
  // Set the AC multiplier and divisor for the given measurement type (common for all phases)
  bool setACMultiplierDivisor(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, uint16_t multiplier, uint16_t divisor);
  // Set the AC power factor for the given phase type (-100 to 100 %)
  bool setACPowerFactor(ZIGBEE_AC_PHASE_TYPE phase_type, int8_t power_factor);
  // Set the AC reporting interval for the given measurement type and phase type in seconds and delta (measurement change - uint16_t for voltage, current and frequency, int32_t for power)
  bool
    setACReporting(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type, uint16_t min_interval, uint16_t max_interval, int32_t delta);
  // Report the AC measurement value for the given measurement type and phase type
  bool reportAC(ZIGBEE_AC_MEASUREMENT_TYPE measurement_type, ZIGBEE_AC_PHASE_TYPE phase_type);

private:
  uint32_t measure_type = 0x0000;
  bool ac_volt_mult_div_set = false;
  bool ac_current_mult_div_set = false;
  bool ac_power_mult_div_set = false;
};

#endif  // CONFIG_ZB_ENABLED
