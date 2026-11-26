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

/* Class of Zigbee Binary sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"

/**
 * @brief Bit flags for binary clusters added to a ZigbeeBinary endpoint.
 */
enum zigbee_binary_clusters {
  BINARY_INPUT = 1, /**< Binary Input cluster. */
  BINARY_OUTPUT = 2 /**< Binary Output cluster. */
};

// HVAC application types for Binary Input (more can be found in Zigbee Cluster Specification 3.14.11.19.4 Binary Inputs (BI) Types)
#define BINARY_INPUT_APPLICATION_TYPE_HVAC_BOILER_STATUS  0x00000003  // Type 0x00, Index 0x0003
#define BINARY_INPUT_APPLICATION_TYPE_HVAC_CHILLER_STATUS 0x00000013  // Type 0x00, Index 0x0013
#define BINARY_INPUT_APPLICATION_TYPE_HVAC_OCCUPANCY      0x00000031  // Type 0x00, Index 0x0031
#define BINARY_INPUT_APPLICATION_TYPE_HVAC_FAN_STATUS     0x00000035  // Type 0x00, Index 0x0035
#define BINARY_INPUT_APPLICATION_TYPE_HVAC_FILTER_STATUS  0x00000036  // Type 0x00, Index 0x0036
#define BINARY_INPUT_APPLICATION_TYPE_HVAC_HEATING_ALARM  0x0000003E  // Type 0x00, Index 0x003E
#define BINARY_INPUT_APPLICATION_TYPE_HVAC_COOLING_ALARM  0x0000001D  // Type 0x00, Index 0x001D
#define BINARY_INPUT_APPLICATION_TYPE_HVAC_UNIT_ENABLE    0x00000090  // Type 0x00, Index 0x0090
#define BINARY_INPUT_APPLICATION_TYPE_HVAC_OTHER          0x0000FFFF  // Type 0x00, Index 0xFFFF

// Security application types for Binary Input
#define BINARY_INPUT_APPLICATION_TYPE_SECURITY_GLASS_BREAKAGE_DETECTION_0 0x01000000  // Type 0x01, Index 0x0000
#define BINARY_INPUT_APPLICATION_TYPE_SECURITY_INTRUSION_DETECTION        0x01000001  // Type 0x01, Index 0x0001
#define BINARY_INPUT_APPLICATION_TYPE_SECURITY_MOTION_DETECTION           0x01000002  // Type 0x01, Index 0x0002
#define BINARY_INPUT_APPLICATION_TYPE_SECURITY_GLASS_BREAKAGE_DETECTION_1 0x01000003  // Type 0x01, Index 0x0003
#define BINARY_INPUT_APPLICATION_TYPE_SECURITY_ZONE_ARMED                 0x01000004  // Type 0x01, Index 0x0004
#define BINARY_INPUT_APPLICATION_TYPE_SECURITY_GLASS_BREAKAGE_DETECTION_2 0x01000005  // Type 0x01, Index 0x0005
#define BINARY_INPUT_APPLICATION_TYPE_SECURITY_SMOKE_DETECTION            0x01000006  // Type 0x01, Index 0x0006
#define BINARY_INPUT_APPLICATION_TYPE_SECURITY_CARBON_DIOXIDE_DETECTION   0x01000007  // Type 0x01, Index 0x0007
#define BINARY_INPUT_APPLICATION_TYPE_SECURITY_HEAT_DETECTION             0x01000008  // Type 0x01, Index 0x0008
#define BINARY_INPUT_APPLICATION_TYPE_SECURITY_OTHER                      0x0100FFFF  // Type 0x01, Index 0xFFFF

// HVAC application types for Binary Output (more can be found in Zigbee Cluster Specification 3.14.11.19.5 Binary Outputs (BO) Types)
#define BINARY_OUTPUT_APPLICATION_TYPE_HVAC_BOILER        0x00000003  // Type 0x00, Index 0x0003
#define BINARY_OUTPUT_APPLICATION_TYPE_HVAC_CHILLER       0x0000000D  // Type 0x00, Index 0x000D
#define BINARY_OUTPUT_APPLICATION_TYPE_HVAC_FAN           0x00000022  // Type 0x00, Index 0x0022
#define BINARY_OUTPUT_APPLICATION_TYPE_HVAC_HEATING_VALVE 0x0000002C  // Type 0x00, Index 0x002C
#define BINARY_OUTPUT_APPLICATION_TYPE_HVAC_HUMIDIFIER    0x00000033  // Type 0x00, Index 0x0033
#define BINARY_OUTPUT_APPLICATION_TYPE_HVAC_PREHEAT       0x00000034  // Type 0x00, Index 0x0034
#define BINARY_OUTPUT_APPLICATION_TYPE_HVAC_OTHER         0x0000FFFF  // Type 0x00, Index 0xFFFF

// Security application types for Binary Output
#define BINARY_OUTPUT_APPLICATION_TYPE_SECURITY_ARM_DISARM_COMMAND 0x01000000  // Type 0x01, Index 0x0000
#define BINARY_OUTPUT_APPLICATION_TYPE_SECURITY_OCCUPANCY_CONTROL  0x01000001  // Type 0x01, Index 0x0001
#define BINARY_OUTPUT_APPLICATION_TYPE_SECURITY_ENABLE_CONTROL     0x01000002  // Type 0x01, Index 0x0002
#define BINARY_OUTPUT_APPLICATION_TYPE_SECURITY_ACCESS_CONTROL     0x01000003  // Type 0x01, Index 0x0003
#define BINARY_OUTPUT_APPLICATION_TYPE_SECURITY_OTHER              0x0100FFFF  // Type 0x01, Index 0xFFFF

/**
 * @brief Binary input/output endpoint (Zigbee HA binary sensors and actuators).
 *
 * Binary Input (BI) reports sensor states (door, motion, etc.). Binary Output (BO)
 * controls relays, switches, or other on/off actuators.
 */
class ZigbeeBinary : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee binary sensor endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeBinary(uint8_t endpoint);
  ~ZigbeeBinary() {}

  /**
   * @brief Add a binary input cluster to the endpoint.
   * @return true if successful.
   */
  bool addBinaryInput();
  /**
   * @brief Add a binary output cluster to the endpoint.
   * @return true if successful.
   */
  bool addBinaryOutput();

  /**
   * @brief Set the application type for the binary input.
   * @param application_type Application type constant (see HVAC/security defines above).
   * @return true if successful.
   */
  bool setBinaryInputApplication(uint32_t application_type);
  /**
   * @brief Set a custom description for the binary input.
   * @param description Description string.
   * @return true if successful.
   */
  bool setBinaryInputDescription(const char *description);

  /**
   * @brief Set the application type for the binary output.
   * @param application_type Application type constant (see HVAC/security defines above).
   * @return true if successful.
   */
  bool setBinaryOutputApplication(uint32_t application_type);
  /**
   * @brief Set a custom description for the binary output.
   * @param description Description string.
   * @return true if successful.
   */
  bool setBinaryOutputDescription(const char *description);

  /**
   * @brief Set a callback invoked when the binary output value changes.
   * @param callback Function that receives the new binary output value.
   */
  void onBinaryOutputChange(void (*callback)(bool binary_output)) {
    _on_binary_output_change = callback;
  }

  /**
   * @brief Set the binary input value.
   * @param input Binary value (true/false).
   * @return true if successful.
   */
  bool setBinaryInput(bool input);
  /**
   * @brief Set the binary output value.
   * @param output Binary value (true/false).
   * @return true if successful.
   */
  bool setBinaryOutput(bool output);

  /**
   * @brief Get the current binary output value.
   * @return Current binary output state.
   */
  bool getBinaryOutput() {
    return _output_state;
  }

  /**
   * @brief Manually report the current binary input value.
   * @return true if successful.
   */
  bool reportBinaryInput();
  /**
   * @brief Manually report the current binary output value.
   * @return true if successful.
   */
  bool reportBinaryOutput();

private:
  void zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) override;

  void (*_on_binary_output_change)(bool);
  void binaryOutputChanged();

  bool _output_state;
  uint8_t _binary_clusters;

  uint32_t _bi_application_type;
  uint32_t _bo_application_type;
  char _bi_description[ZB_MAX_NAME_LENGTH + 2];
  char _bo_description[ZB_MAX_NAME_LENGTH + 2];
};

#endif  // CONFIG_ZB_ENABLED
