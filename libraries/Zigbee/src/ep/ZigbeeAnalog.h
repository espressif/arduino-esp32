/* Class of Zigbee Analog sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

// clang-format off
#define ZIGBEE_DEFAULT_ANALOG_CONFIG()                                    \
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
  }
// clang-format on

//enum for bits set to check what analog cluster were added
enum zigbee_analog_clusters {
  ANALOG_VALUE = 1,
  ANALOG_INPUT = 2,
  ANALOG_OUTPUT = 4
};

typedef struct zigbee_analog_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;
  esp_zb_identify_cluster_cfg_t identify_cfg;
  esp_zb_analog_value_cluster_cfg_t analog_value_cfg;
  esp_zb_analog_output_cluster_cfg_t analog_output_cfg;
  esp_zb_analog_input_cluster_cfg_t analog_input_cfg;
} zigbee_analog_cfg_t;

class ZigbeeAnalog : public ZigbeeEP {
public:
  ZigbeeAnalog(uint8_t endpoint);
  ~ZigbeeAnalog() {}

  // Add analog clusters
  void addAnalogValue();
  void addAnalogInput();
  void addAnalogOutput();

  // Use to set a cb function to be called on analog output change
  void onAnalogOutputChange(void (*callback)(float analog)) {
    _on_analog_output_change = callback;
  }

  // Set the analog value / input
  void setAnalogValue(float analog);
  void setAnalogInput(float analog);

  // Report Analog Input
  void reportAnalogInput();

  // Set reporting for Analog Input
  void setAnalogInputReporting(uint16_t min_interval, uint16_t max_interval, float delta);

private:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;

  void (*_on_analog_output_change)(float);
  void analogOutputChanged(float analog_output);

  uint8_t _analog_clusters;
};

#endif  //SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED
