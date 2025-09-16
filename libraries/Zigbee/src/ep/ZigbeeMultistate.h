/* Class of Zigbee Multistate sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

// Types for Multistate Input/Output
// uint16_t for present value -> index to array of states
// uint16_t for number of states
// uint8_t chars for state names, with max of 16 chars ASCII

// Multistate Input/Output Application Types Defines
#define ZB_MULTISTATE_APPLICATION_TYPE_0_INDEX      0x0000
#define ZB_MULTISTATE_APPLICATION_TYPE_0_NUM_STATES 3
#define ZB_MULTISTATE_APPLICATION_TYPE_0_STATE_NAMES \
  (const char *const[]) {                            \
    "Off", "On", "Auto"                              \
  }

#define ZB_MULTISTATE_APPLICATION_TYPE_1_INDEX      0x0001
#define ZB_MULTISTATE_APPLICATION_TYPE_1_NUM_STATES 4
#define ZB_MULTISTATE_APPLICATION_TYPE_1_STATE_NAMES \
  (const char *const[]) {                            \
    "Off", "Low", "Medium", "High"                   \
  }

#define ZB_MULTISTATE_APPLICATION_TYPE_2_INDEX      0x0002
#define ZB_MULTISTATE_APPLICATION_TYPE_2_NUM_STATES 7
#define ZB_MULTISTATE_APPLICATION_TYPE_2_STATE_NAMES                        \
  (const char *const[]) {                                                   \
    "Auto", "Heat", "Cool", "Off", "Emergency Heat", "Fan Only", "Max Heat" \
  }

#define ZB_MULTISTATE_APPLICATION_TYPE_3_INDEX      0x0003
#define ZB_MULTISTATE_APPLICATION_TYPE_3_NUM_STATES 4
#define ZB_MULTISTATE_APPLICATION_TYPE_3_STATE_NAMES \
  (const char *const[]) {                            \
    "Occupied", "Unoccupied", "Standby", "Bypass"    \
  }

#define ZB_MULTISTATE_APPLICATION_TYPE_4_INDEX      0x0004
#define ZB_MULTISTATE_APPLICATION_TYPE_4_NUM_STATES 3
#define ZB_MULTISTATE_APPLICATION_TYPE_4_STATE_NAMES \
  (const char *const[]) {                            \
    "Inactive", "Active", "Hold"                     \
  }

#define ZB_MULTISTATE_APPLICATION_TYPE_5_INDEX      0x0005
#define ZB_MULTISTATE_APPLICATION_TYPE_5_NUM_STATES 8
#define ZB_MULTISTATE_APPLICATION_TYPE_5_STATE_NAMES                                                                         \
  (const char *const[]) {                                                                                                    \
    "Auto", "Warm-up", "Water Flush", "Autocalibration", "Shutdown Open", "Shutdown Closed", "Low Limit", "Test and Balance" \
  }

#define ZB_MULTISTATE_APPLICATION_TYPE_6_INDEX      0x0006
#define ZB_MULTISTATE_APPLICATION_TYPE_6_NUM_STATES 6
#define ZB_MULTISTATE_APPLICATION_TYPE_6_STATE_NAMES                 \
  (const char *const[]) {                                            \
    "Off", "Auto", "Heat Cool", "Heat Only", "Cool Only", "Fan Only" \
  }

#define ZB_MULTISTATE_APPLICATION_TYPE_7_INDEX      0x0007
#define ZB_MULTISTATE_APPLICATION_TYPE_7_NUM_STATES 3
#define ZB_MULTISTATE_APPLICATION_TYPE_7_STATE_NAMES \
  (const char *const[]) {                            \
    "High", "Normal", "Low"                          \
  }

#define ZB_MULTISTATE_APPLICATION_TYPE_8_INDEX      0x0008
#define ZB_MULTISTATE_APPLICATION_TYPE_8_NUM_STATES 4
#define ZB_MULTISTATE_APPLICATION_TYPE_8_STATE_NAMES \
  (const char *const[]) {                            \
    "Occupied", "Unoccupied", "Startup", "Shutdown"  \
  }

#define ZB_MULTISTATE_APPLICATION_TYPE_9_INDEX      0x0009
#define ZB_MULTISTATE_APPLICATION_TYPE_9_NUM_STATES 3
#define ZB_MULTISTATE_APPLICATION_TYPE_9_STATE_NAMES \
  (const char *const[]) {                            \
    "Night", "Day", "Hold"                           \
  }

#define ZB_MULTISTATE_APPLICATION_TYPE_10_INDEX      0x000A
#define ZB_MULTISTATE_APPLICATION_TYPE_10_NUM_STATES 5
#define ZB_MULTISTATE_APPLICATION_TYPE_10_STATE_NAMES \
  (const char *const[]) {                             \
    "Off", "Cool", "Heat", "Auto", "Emergency Heat"   \
  }

#define ZB_MULTISTATE_APPLICATION_TYPE_11_INDEX      0x000B
#define ZB_MULTISTATE_APPLICATION_TYPE_11_NUM_STATES 7
#define ZB_MULTISTATE_APPLICATION_TYPE_11_STATE_NAMES                                             \
  (const char *const[]) {                                                                         \
    "Shutdown Closed", "Shutdown Open", "Satisfied", "Mixing", "Cooling", "Heating", "Suppl Heat" \
  }

#define ZB_MULTISTATE_APPLICATION_TYPE_OTHER_INDEX 0xFFFF

//enum for bits set to check what multistate cluster were added
enum zigbee_multistate_clusters {
  MULTISTATE_INPUT = 1,
  MULTISTATE_OUTPUT = 2
};

typedef struct zigbee_multistate_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;
  esp_zb_identify_cluster_cfg_t identify_cfg;
} zigbee_multistate_cfg_t;

class ZigbeeMultistate : public ZigbeeEP {
public:
  ZigbeeMultistate(uint8_t endpoint);
  ~ZigbeeMultistate() {}

  // Add multistate clusters
  bool addMultistateInput();
  bool addMultistateOutput();

  // Set the application type and description for the multistate input
  bool setMultistateInputApplication(uint32_t application_type);  // Check esp_zigbee_zcl_multistate_input.h for application type values
  bool setMultistateInputDescription(const char *description);
  bool setMultistateInputStates(uint16_t number_of_states);
  // bool setMultistateInputStates(const char * const states[], uint16_t states_length);

  // Set the application type and description for the multistate output
  bool setMultistateOutputApplication(uint32_t application_type);  // Check esp_zigbee_zcl_multistate_output.h for application type values
  bool setMultistateOutputDescription(const char *description);
  bool setMultistateOutputStates(uint16_t number_of_states);
  // bool setMultistateOutputStates(const char * const states[], uint16_t states_length);

  // Use to set a cb function to be called on multistate output change
  void onMultistateOutputChange(void (*callback)(uint16_t state)) {
    _on_multistate_output_change = callback;
  }

  // Set the Multistate Input/Output value
  bool setMultistateInput(uint16_t state);
  bool setMultistateOutput(uint16_t state);

  // Get the Multistate Input/Output values
  uint16_t getMultistateInput() {
    return _input_state;
  }
  uint16_t getMultistateOutput() {
    return _output_state;
  }

  // Get state names and length
  uint16_t getMultistateInputStateNamesLength() {
    return _input_state_names_length;
  }
  uint16_t getMultistateOutputStateNamesLength() {
    return _output_state_names_length;
  }
  // const char* const* getMultistateInputStateNames() {
  //   return _input_state_names;
  // }
  // const char* const* getMultistateOutputStateNames() {
  //   return _output_state_names;
  // }

  // Report Multistate Input/Output
  bool reportMultistateInput();
  bool reportMultistateOutput();

private:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;

  void (*_on_multistate_output_change)(uint16_t state);
  void multistateOutputChanged();

  uint8_t _multistate_clusters;
  uint16_t _output_state;
  uint16_t _input_state;

  // Local storage for state names
  uint16_t _input_state_names_length;
  uint16_t _output_state_names_length;
  // const char* const* _input_state_names;
  // const char* const* _output_state_names;
};

#endif  // CONFIG_ZB_ENABLED
