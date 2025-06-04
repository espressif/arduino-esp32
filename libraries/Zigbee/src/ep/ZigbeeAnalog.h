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

  // Set the application type and description for the analog input
  bool setAnalogInputApplication(uint32_t application_type);  // Check esp_zigbee_zcl_analog_input.h for application type values
  bool setAnalogInputDescription(const char *description);
  bool setAnalogInputResolution(float resolution);

  // Set the application type and description for the analog output
  bool setAnalogOutputApplication(uint32_t application_type);  // Check esp_zigbee_zcl_analog_output.h for application type values
  bool setAnalogOutputDescription(const char *description);
  bool setAnalogOutputResolution(float resolution);

  // Use to set a cb function to be called on analog output change
  void onAnalogOutputChange(void (*callback)(float analog)) {
    _on_analog_output_change = callback;
  }

  // Set the Analog Input/Output value
  bool setAnalogInput(float analog);
  bool setAnalogOutput(float analog);

  // Get the Analog Output value
  float getAnalogOutput() {
    return _output_state;
  }

  // Report Analog Input/Output
  bool reportAnalogInput();
  bool reportAnalogOutput();

  // Set reporting for Analog Input
  bool setAnalogInputReporting(uint16_t min_interval, uint16_t max_interval, float delta);

private:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;

  void (*_on_analog_output_change)(float);
  void analogOutputChanged();

  uint8_t _analog_clusters;
  float _output_state;
};

#endif  // CONFIG_ZB_ENABLED
