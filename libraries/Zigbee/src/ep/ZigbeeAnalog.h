/* Class of Zigbee Analog sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

//enum for bits set to check what analog cluster were added
enum zigbee_analog_clusters {
  ANALOG_INPUT = 1,
  ANALOG_OUTPUT = 2
};

typedef struct zigbee_analog_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;
  esp_zb_identify_cluster_cfg_t identify_cfg;
  esp_zb_analog_output_cluster_cfg_t analog_output_cfg;
  esp_zb_analog_input_cluster_cfg_t analog_input_cfg;
} zigbee_analog_cfg_t;

class ZigbeeAnalog : public ZigbeeEP {
public:
  ZigbeeAnalog(uint8_t endpoint);
  ~ZigbeeAnalog() {}

  // Add analog clusters
  bool addAnalogInput();
  bool addAnalogOutput();

  // Use to set a cb function to be called on analog output change
  void onAnalogOutputChange(void (*callback)(float analog)) {
    _on_analog_output_change = callback;
  }

  // Set the analog input value
  bool setAnalogInput(float analog);

  // Report Analog Input value
  bool reportAnalogInput();

  // Set reporting for Analog Input
  bool setAnalogInputReporting(uint16_t min_interval, uint16_t max_interval, float delta);

private:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;

  void (*_on_analog_output_change)(float);
  void analogOutputChanged(float analog_output);

  uint8_t _analog_clusters;
};

#endif  // CONFIG_ZB_ENABLED
