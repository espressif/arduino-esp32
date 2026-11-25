// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/* Class of Zigbee Multistate sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"

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

/**
 * @brief Bit flags for multistate clusters added to a ZigbeeMultistate endpoint.
 */
enum zigbee_multistate_clusters {
  MULTISTATE_INPUT = 1, /**< Multistate Input cluster. */
  MULTISTATE_OUTPUT = 2 /**< Multistate Output cluster. */
};

/**
 * @brief Multistate input/output endpoint for discrete states (Zigbee HA).
 *
 * Multistate Input (MI) reports sensor states (fan speed, HVAC mode, occupancy).
 * Multistate Output (MO) is controlled by the coordinator.
 *
 * @note Home Assistant ZHA does not support multistate input and output clusters yet.
 */
class ZigbeeMultistate : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee multistate endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeMultistate(uint8_t endpoint);
  ~ZigbeeMultistate() {}

  /**
   * @brief Add the multistate input cluster to the endpoint.
   * @return true if successful.
   */
  bool addMultistateInput();
  /**
   * @brief Add the multistate output cluster to the endpoint.
   * @return true if successful.
   */
  bool addMultistateOutput();

  /**
   * @brief Set the application type for the multistate input.
   * @param application_type Application type constant (`ZB_MULTISTATE_APPLICATION_TYPE_*_INDEX`).
   * @return true if successful.
   */
  bool setMultistateInputApplication(uint32_t application_type);
  /**
   * @brief Set a custom description for the multistate input.
   * @param description Description string.
   * @return true if successful.
   */
  bool setMultistateInputDescription(const char *description);
  /**
   * @brief Set the number of states for the multistate input.
   * @param number_of_states Number of discrete states (1-65535).
   * @return true if successful.
   */
  bool setMultistateInputStates(uint16_t number_of_states);
  // bool setMultistateInputStates(const char * const states[], uint16_t states_length);

  /**
   * @brief Set the application type for the multistate output.
   * @param application_type Application type constant (`ZB_MULTISTATE_APPLICATION_TYPE_*_INDEX`).
   * @return true if successful.
   */
  bool setMultistateOutputApplication(uint32_t application_type);
  /**
   * @brief Set a custom description for the multistate output.
   * @param description Description string.
   * @return true if successful.
   */
  bool setMultistateOutputDescription(const char *description);
  /**
   * @brief Set the number of states for the multistate output.
   * @param number_of_states Number of discrete states (1-65535).
   * @return true if successful.
   */
  bool setMultistateOutputStates(uint16_t number_of_states);
  // bool setMultistateOutputStates(const char * const states[], uint16_t states_length);

  /**
   * @brief Set a callback invoked when the multistate output value changes.
   * @param callback Function called with the new state index.
   */
  void onMultistateOutputChange(void (*callback)(uint16_t state)) {
    _on_multistate_output_change = callback;
  }

  /**
   * @brief Set the multistate input value.
   * @param state State index (0 to number_of_states-1).
   * @return true if successful.
   */
  bool setMultistateInput(uint16_t state);
  /**
   * @brief Set the multistate output value.
   * @param state State index (0 to number_of_states-1).
   * @return true if successful.
   */
  bool setMultistateOutput(uint16_t state);

  /**
   * @brief Get the current multistate input value.
   * @return Current input state index.
   */
  uint16_t getMultistateInput() {
    return _input_state;
  }
  /**
   * @brief Get the current multistate output value.
   * @return Current output state index.
   */
  uint16_t getMultistateOutput() {
    return _output_state;
  }

  /**
   * @brief Get the number of state names configured for multistate input.
   * @return Number of input state names.
   */
  uint16_t getMultistateInputStateNamesLength() {
    return _input_state_names_length;
  }
  /**
   * @brief Get the number of state names configured for multistate output.
   * @return Number of output state names.
   */
  uint16_t getMultistateOutputStateNamesLength() {
    return _output_state_names_length;
  }
  // const char* const* getMultistateInputStateNames() {
  //   return _input_state_names;
  // }
  // const char* const* getMultistateOutputStateNames() {
  //   return _output_state_names;
  // }

  /**
   * @brief Manually report the current multistate input value.
   * @return true if successful.
   */
  bool reportMultistateInput();
  /**
   * @brief Manually report the current multistate output value.
   * @return true if successful.
   */
  bool reportMultistateOutput();

private:
  void zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) override;

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

  uint32_t _mi_application_type;
  uint32_t _mo_application_type;
  char _mi_description[ZB_MAX_NAME_LENGTH + 2];
  char _mo_description[ZB_MAX_NAME_LENGTH + 2];
};

#endif  // CONFIG_ZB_ENABLED
