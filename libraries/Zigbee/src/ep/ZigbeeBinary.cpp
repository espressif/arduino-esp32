// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
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

#include "ZigbeeBinary.h"
#if CONFIG_ZB_ENABLED

// NOTE(zb-v2): v2.x does not expose application-type "group id" macros. The group id is the high byte
// of the 32-bit ApplicationType value, so the ZCL-defined values are inlined here (matching v1):
// Binary Input = 0x03, Binary Output = 0x04.
#define ZB_BINARY_INPUT_GROUP_ID  0x03
#define ZB_BINARY_OUTPUT_GROUP_ID 0x04

ZigbeeBinary::ZigbeeBinary(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID;
  _on_binary_output_change = nullptr;

  // v2.x data model: build the endpoint descriptor manually with Basic + Identify server clusters.
  // Binary Input/Output clusters are attached later by addBinaryInput()/addBinaryOutput().
  _ep_config = {.ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};
  _ep_desc = ezb_af_create_endpoint_desc(&_ep_config);
  if (_ep_desc == nullptr) {
    log_e("Failed to create binary endpoint descriptor");
    return;
  }
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_basic_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
  ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_identify_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
}

bool ZigbeeBinary::addBinaryInput() {
  ezb_zcl_cluster_desc_t binary_input_cluster = ezb_zcl_binary_input_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER);

  // Create default description for Binary Input
  char default_description[] = "\x0C"
                               "Binary Input";
  uint32_t application_type = 0x00000000 | (ZB_BINARY_INPUT_GROUP_ID << 24);  // Group ID 0x03

  ezb_err_t ret = ezb_zcl_binary_input_cluster_desc_add_attr(binary_input_cluster, EZB_ZCL_ATTR_BINARY_INPUT_DESCRIPTION_ID, (void *)default_description);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add description attribute: 0x%x", ret);
    return false;
  }

  ret = ezb_zcl_binary_input_cluster_desc_add_attr(binary_input_cluster, EZB_ZCL_ATTR_BINARY_INPUT_APPLICATION_TYPE_ID, (void *)&application_type);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add application type attribute: 0x%x", ret);
    return false;
  }

  ret = ezb_af_endpoint_add_cluster_desc(_ep_desc, binary_input_cluster);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add Binary Input cluster: 0x%x", ret);
    return false;
  }
  _binary_clusters |= BINARY_INPUT;
  return true;
}

bool ZigbeeBinary::addBinaryOutput() {
  ezb_zcl_cluster_desc_t binary_output_cluster = ezb_zcl_binary_output_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER);

  // Create default description for Binary Output
  char default_description[] = "\x0D"
                               "Binary Output";
  uint32_t application_type = 0x00000000 | (ZB_BINARY_OUTPUT_GROUP_ID << 24);  // Group ID 0x04

  ezb_err_t ret = ezb_zcl_binary_output_cluster_desc_add_attr(binary_output_cluster, EZB_ZCL_ATTR_BINARY_OUTPUT_DESCRIPTION_ID, (void *)default_description);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add description attribute: 0x%x", ret);
    return false;
  }

  ret = ezb_zcl_binary_output_cluster_desc_add_attr(binary_output_cluster, EZB_ZCL_ATTR_BINARY_OUTPUT_APPLICATION_TYPE_ID, (void *)&application_type);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add application type attribute: 0x%x", ret);
    return false;
  }

  ret = ezb_af_endpoint_add_cluster_desc(_ep_desc, binary_output_cluster);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add Binary Output cluster: 0x%x", ret);
    return false;
  }
  _binary_clusters |= BINARY_OUTPUT;
  return true;
}

// Check Zigbee Cluster Specification 3.14.11.19.4 Binary Inputs (BI) Types for application type values
bool ZigbeeBinary::setBinaryInputApplication(uint32_t application_type) {
  if (!(_binary_clusters & BINARY_INPUT)) {
    log_e("Binary Input cluster not added");
    return false;
  }

  // Add the Binary Input group ID (0x03) to the application type
  uint32_t application_type_value = (ZB_BINARY_INPUT_GROUP_ID << 24) | application_type;

  ezb_zcl_cluster_desc_t binary_input_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_BINARY_INPUT, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret = ezb_zcl_binary_input_cluster_desc_add_attr(binary_input_cluster, EZB_ZCL_ATTR_BINARY_INPUT_APPLICATION_TYPE_ID, (void *)&application_type_value);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set Binary Input application type: 0x%x", ret);
    return false;
  }
  return true;
}

// Check Zigbee Cluster Specification 3.14.11.19.5 Binary Outputs (BO) Types for application type values
bool ZigbeeBinary::setBinaryOutputApplication(uint32_t application_type) {
  if (!(_binary_clusters & BINARY_OUTPUT)) {
    log_e("Binary Output cluster not added");
    return false;
  }

  // Add the Binary Output group ID (0x04) to the application type
  uint32_t application_type_value = (ZB_BINARY_OUTPUT_GROUP_ID << 24) | application_type;

  ezb_zcl_cluster_desc_t binary_output_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_BINARY_OUTPUT, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret = ezb_zcl_binary_output_cluster_desc_add_attr(binary_output_cluster, EZB_ZCL_ATTR_BINARY_OUTPUT_APPLICATION_TYPE_ID, (void *)&application_type_value);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set Binary Output application type: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeBinary::setBinaryInput(bool input) {
  if (!(_binary_clusters & BINARY_INPUT)) {
    log_e("Binary Input cluster not added");
    return false;
  }
  log_d("Setting binary input to %d", input);
  ezb_zcl_status_t ret =
    setClusterAttribute(EZB_ZCL_CLUSTER_ID_BINARY_INPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID, &input, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set binary input: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeBinary::reportBinaryInput() {
  /* Send report attributes command */
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
  // No explicit destination: report to bound devices (replaces v1 ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT).
  ezb_address_set_none(&report_attr_cmd.cmd_ctrl.dst_addr);
  report_attr_cmd.cmd_ctrl.src_ep = _endpoint;
  report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_BINARY_INPUT;
  report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.payload.attr_id = EZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID;

  if (!reportClusterAttribute(&report_attr_cmd)) {
    log_e("Failed to send Binary Input report");
    return false;
  }
  log_v("Binary Input report sent");
  return true;
}

bool ZigbeeBinary::setBinaryInputDescription(const char *description) {
  if (!(_binary_clusters & BINARY_INPUT)) {
    log_e("Binary Input cluster not added");
    return false;
  }

  // Allocate a new array of size length + 2 (1 for the length, 1 for null terminator)
  char zb_description[ZB_MAX_NAME_LENGTH + 2];

  // Convert description to ZCL string
  size_t description_length = strlen(description);
  if (description_length > ZB_MAX_NAME_LENGTH) {
    log_e("Description is too long");
    return false;
  }

  // Get and check the binary input cluster
  ezb_zcl_cluster_desc_t binary_input_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_BINARY_INPUT, EZB_ZCL_CLUSTER_SERVER);
  if (binary_input_cluster == nullptr) {
    log_e("Failed to get binary input cluster");
    return false;
  }

  // Store the length as the first element
  zb_description[0] = static_cast<char>(description_length);  // Cast size_t to char
  // Use memcpy to copy the characters to the result array
  memcpy(zb_description + 1, description, description_length);
  // Null-terminate the array
  zb_description[description_length + 1] = '\0';

  // Update the description attribute
  ezb_err_t ret = ezb_zcl_binary_input_cluster_desc_add_attr(binary_input_cluster, EZB_ZCL_ATTR_BINARY_INPUT_DESCRIPTION_ID, (void *)zb_description);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set description: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeBinary::setBinaryOutputDescription(const char *description) {
  if (!(_binary_clusters & BINARY_OUTPUT)) {
    log_e("Binary Output cluster not added");
    return false;
  }

  // Allocate a new array of size length + 2 (1 for the length, 1 for null terminator)
  char zb_description[ZB_MAX_NAME_LENGTH + 2];

  // Convert description to ZCL string
  size_t description_length = strlen(description);
  if (description_length > ZB_MAX_NAME_LENGTH) {
    log_e("Description is too long");
    return false;
  }

  // Get and check the binary output cluster
  ezb_zcl_cluster_desc_t binary_output_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_BINARY_OUTPUT, EZB_ZCL_CLUSTER_SERVER);
  if (binary_output_cluster == nullptr) {
    log_e("Failed to get binary output cluster");
    return false;
  }

  // Store the length as the first element
  zb_description[0] = static_cast<char>(description_length);  // Cast size_t to char
  // Use memcpy to copy the characters to the result array
  memcpy(zb_description + 1, description, description_length);
  // Null-terminate the array
  zb_description[description_length + 1] = '\0';

  // Update the description attribute
  ezb_err_t ret = ezb_zcl_binary_output_cluster_desc_add_attr(binary_output_cluster, EZB_ZCL_ATTR_BINARY_OUTPUT_DESCRIPTION_ID, (void *)zb_description);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set description: 0x%x", ret);
    return false;
  }
  return true;
}

//set attribute method -> method overridden in child class
void ZigbeeBinary::zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) {
  if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_BINARY_OUTPUT) {
    if (message->in.attribute.id == EZB_ZCL_ATTR_BINARY_OUTPUT_PRESENT_VALUE_ID && message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_BOOL) {
      _output_state = *(bool *)message->in.attribute.data.value;
      binaryOutputChanged();
    } else {
      log_w("Received message ignored. Attribute ID: %u not supported for Binary Output", message->in.attribute.id);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %u not supported for Binary endpoint", message->info.cluster_id);
  }
}

void ZigbeeBinary::binaryOutputChanged() {
  if (_on_binary_output_change) {
    _on_binary_output_change(_output_state);
  } else {
    log_w("No callback function set for binary output change");
  }
}

bool ZigbeeBinary::setBinaryOutput(bool output) {
  _output_state = output;
  binaryOutputChanged();

  log_v("Updating binary output to %d", output);
  /* Update binary output */
  ezb_zcl_status_t ret =
    setClusterAttribute(EZB_ZCL_CLUSTER_ID_BINARY_OUTPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_BINARY_OUTPUT_PRESENT_VALUE_ID, &_output_state, false);

  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set binary output: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeBinary::reportBinaryOutput() {
  /* Send report attributes command */
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
  ezb_address_set_none(&report_attr_cmd.cmd_ctrl.dst_addr);
  report_attr_cmd.cmd_ctrl.src_ep = _endpoint;
  report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_BINARY_OUTPUT;
  report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.payload.attr_id = EZB_ZCL_ATTR_BINARY_OUTPUT_PRESENT_VALUE_ID;

  if (!reportClusterAttribute(&report_attr_cmd)) {
    log_e("Failed to send Binary Output report");
    return false;
  }
  log_v("Binary Output report sent");
  return true;
}

#endif  // CONFIG_ZB_ENABLED
