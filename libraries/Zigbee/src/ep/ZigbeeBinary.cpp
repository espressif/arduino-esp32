#include "ZigbeeBinary.h"
#if CONFIG_ZB_ENABLED

ZigbeeBinary::ZigbeeBinary(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID;

  //Create basic binary sensor clusters without configuration
  _cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(_cluster_list, esp_zb_basic_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(_cluster_list, esp_zb_identify_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};
}

bool ZigbeeBinary::addBinaryInput() {
  esp_zb_attribute_list_t *esp_zb_binary_input_cluster = esp_zb_binary_input_cluster_create(NULL);

  // Create default description for Binary Input
  char default_description[] = "\x0C"
                               "Binary Input";
  uint32_t application_type = 0x00000000 | (0x03 << 24);  // Group ID 0x03

  esp_err_t ret = esp_zb_binary_input_cluster_add_attr(esp_zb_binary_input_cluster, ESP_ZB_ZCL_ATTR_BINARY_INPUT_DESCRIPTION_ID, (void *)default_description);
  if (ret != ESP_OK) {
    log_e("Failed to add description attribute: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  ret = esp_zb_binary_input_cluster_add_attr(esp_zb_binary_input_cluster, ESP_ZB_ZCL_ATTR_BINARY_INPUT_APPLICATION_TYPE_ID, (void *)&application_type);
  if (ret != ESP_OK) {
    log_e("Failed to add application type attribute: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  ret = esp_zb_cluster_list_add_binary_input_cluster(_cluster_list, esp_zb_binary_input_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (ret != ESP_OK) {
    log_e("Failed to add Binary Input cluster: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  _binary_clusters |= BINARY_INPUT;
  return true;
}

// Check Zigbee Cluster Specification 3.14.11.19.4 Binary Inputs (BI) Types for application type values
bool ZigbeeBinary::setBinaryInputApplication(uint32_t application_type) {
  if (!(_binary_clusters & BINARY_INPUT)) {
    log_e("Binary Input cluster not added");
    return false;
  }

  // Add the Binary Input group ID (0x03) to the application type
  uint32_t application_type_value = (0x03 << 24) | application_type;

  esp_zb_attribute_list_t *binary_input_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_err_t ret = esp_zb_cluster_update_attr(binary_input_cluster, ESP_ZB_ZCL_ATTR_BINARY_INPUT_APPLICATION_TYPE_ID, (void *)&application_type_value);
  if (ret != ESP_OK) {
    log_e("Failed to set Binary Input application type: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeBinary::setBinaryInput(bool input) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  if (!(_binary_clusters & BINARY_INPUT)) {
    log_e("Binary Input cluster not added");
    return false;
  }
  log_d("Setting binary input to %d", input);
  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID, &input, false
  );
  esp_zb_lock_release();
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set binary input: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeBinary::reportBinaryInput() {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_BINARY_INPUT_PRESENT_VALUE_ID;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  report_attr_cmd.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  if (ret != ESP_OK) {
    log_e("Failed to send Binary Input report: 0x%x: %s", ret, esp_err_to_name(ret));
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
  esp_zb_attribute_list_t *binary_input_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_BINARY_INPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
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
  esp_err_t ret = esp_zb_cluster_update_attr(binary_input_cluster, ESP_ZB_ZCL_ATTR_BINARY_INPUT_DESCRIPTION_ID, (void *)zb_description);
  if (ret != ESP_OK) {
    log_e("Failed to set description: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

#endif  // CONFIG_ZB_ENABLED
