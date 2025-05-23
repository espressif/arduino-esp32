/* Class of Zigbee Binary sensor endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

//enum for bits set to check what analog cluster were added
enum zigbee_binary_clusters {
  BINARY_INPUT = 1,
  BINARY_OUTPUT = 2
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

typedef struct zigbee_binary_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;
  esp_zb_identify_cluster_cfg_t identify_cfg;
  // esp_zb_binary_output_cluster_cfg_t binary_output_cfg;
  esp_zb_binary_input_cluster_cfg_t binary_input_cfg;
} zigbee_binary_cfg_t;

class ZigbeeBinary : public ZigbeeEP {
public:
  ZigbeeBinary(uint8_t endpoint);
  ~ZigbeeBinary() {}

  // Add binary cluster
  bool addBinaryInput();
  // bool addBinaryOutput();

  // Set the application type and description for the binary input
  bool setBinaryInputApplication(uint32_t application_type);  // Check esp_zigbee_zcl_binary_input.h for application type values
  bool setBinaryInputDescription(const char *description);

  // Set the application type and description for the binary output
  // bool setBinaryOutputApplication(uint32_t application_type); // Check esp_zigbee_zcl_binary_output.h for application type values
  // bool setBinaryOutputDescription(const char *description);

  // Use to set a cb function to be called on binary output change
  // void onBinaryOutputChange(void (*callback)(bool binary_output)) {
  //   _on_binary_output_change = callback;
  // }

  // Set the binary input value
  bool setBinaryInput(bool input);

  // Report Binary Input value
  bool reportBinaryInput();

private:
  // void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;

  // void (*_on_binary_output_change)(bool);
  // void binaryOutputChanged(bool binary_output);

  uint8_t _binary_clusters;
};

#endif  // CONFIG_ZB_ENABLED
