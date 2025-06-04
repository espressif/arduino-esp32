#include "ZigbeeAnalog.h"
#if CONFIG_ZB_ENABLED

ZigbeeAnalog::ZigbeeAnalog(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID;

  //Create basic analog sensor clusters without configuration
  _cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(_cluster_list, esp_zb_basic_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(_cluster_list, esp_zb_identify_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};
}

bool ZigbeeAnalog::addAnalogInput() {
  esp_zb_attribute_list_t *esp_zb_analog_input_cluster = esp_zb_analog_input_cluster_create(NULL);

  // Create default description for Analog Input
  char default_description[] = "\x0C"
                               "Analog Input";
  uint32_t application_type = 0x00000000 | (ESP_ZB_ZCL_AI_GROUP_ID << 24);
  float resolution = 0.1;  // Default resolution of 0.1

  esp_err_t ret = esp_zb_analog_input_cluster_add_attr(esp_zb_analog_input_cluster, ESP_ZB_ZCL_ATTR_ANALOG_INPUT_DESCRIPTION_ID, (void *)default_description);
  if (ret != ESP_OK) {
    log_e("Failed to add description attribute: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  ret = esp_zb_analog_input_cluster_add_attr(esp_zb_analog_input_cluster, ESP_ZB_ZCL_ATTR_ANALOG_INPUT_APPLICATION_TYPE_ID, (void *)&application_type);
  if (ret != ESP_OK) {
    log_e("Failed to add application type attribute: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  ret = esp_zb_analog_input_cluster_add_attr(esp_zb_analog_input_cluster, ESP_ZB_ZCL_ATTR_ANALOG_INPUT_RESOLUTION_ID, (void *)&resolution);
  if (ret != ESP_OK) {
    log_e("Failed to add resolution attribute: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  ret = esp_zb_cluster_list_add_analog_input_cluster(_cluster_list, esp_zb_analog_input_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (ret != ESP_OK) {
    log_e("Failed to add Analog Input cluster: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  _analog_clusters |= ANALOG_INPUT;
  return true;
}

// Check esp_zigbee_zcl_analog_input.h for application type values
bool ZigbeeAnalog::setAnalogInputApplication(uint32_t application_type) {
  if (!(_analog_clusters & ANALOG_INPUT)) {
    log_e("Analog Input cluster not added");
    return false;
  }

  // Add the Analog Input group ID (0x00) to the application type
  uint32_t application_type_value = (ESP_ZB_ZCL_AI_GROUP_ID << 24) | application_type;

  esp_zb_attribute_list_t *analog_input_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_err_t ret = esp_zb_cluster_update_attr(analog_input_cluster, ESP_ZB_ZCL_ATTR_ANALOG_INPUT_APPLICATION_TYPE_ID, (void *)&application_type_value);
  if (ret != ESP_OK) {
    log_e("Failed to set AI application type: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeAnalog::addAnalogOutput() {
  esp_zb_attribute_list_t *esp_zb_analog_output_cluster = esp_zb_analog_output_cluster_create(NULL);

  // Create default description for Analog Output
  char default_description[] = "\x0D"
                               "Analog Output";
  uint32_t application_type = 0x00000000 | (ESP_ZB_ZCL_AO_GROUP_ID << 24);
  float resolution = 1;  // Default resolution of 1

  esp_err_t ret =
    esp_zb_analog_output_cluster_add_attr(esp_zb_analog_output_cluster, ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_DESCRIPTION_ID, (void *)default_description);
  if (ret != ESP_OK) {
    log_e("Failed to add description attribute: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  ret = esp_zb_analog_output_cluster_add_attr(esp_zb_analog_output_cluster, ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_APPLICATION_TYPE_ID, (void *)&application_type);
  if (ret != ESP_OK) {
    log_e("Failed to add application type attribute: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  ret = esp_zb_analog_output_cluster_add_attr(esp_zb_analog_output_cluster, ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_RESOLUTION_ID, (void *)&resolution);
  if (ret != ESP_OK) {
    log_e("Failed to add resolution attribute: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  ret = esp_zb_cluster_list_add_analog_output_cluster(_cluster_list, esp_zb_analog_output_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (ret != ESP_OK) {
    log_e("Failed to add Analog Output cluster: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  _analog_clusters |= ANALOG_OUTPUT;
  return true;
}

// Check esp_zigbee_zcl_analog_output.h for application type values
bool ZigbeeAnalog::setAnalogOutputApplication(uint32_t application_type) {
  if (!(_analog_clusters & ANALOG_OUTPUT)) {
    log_e("Analog Output cluster not added");
    return false;
  }

  // Add the Analog Output group ID (0x00) to the application type
  uint32_t application_type_value = (ESP_ZB_ZCL_AO_GROUP_ID << 24) | application_type;

  esp_zb_attribute_list_t *analog_output_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_err_t ret = esp_zb_cluster_update_attr(analog_output_cluster, ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_APPLICATION_TYPE_ID, (void *)&application_type_value);
  if (ret != ESP_OK) {
    log_e("Failed to set AO application type: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

//set attribute method -> method overridden in child class
void ZigbeeAnalog::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_SINGLE) {
      _output_state = *(float *)message->attribute.data.value;
      analogOutputChanged();
    } else {
      log_w("Received message ignored. Attribute ID: %d not supported for Analog Output", message->attribute.id);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %d not supported for Analog endpoint", message->info.cluster);
  }
}

void ZigbeeAnalog::analogOutputChanged() {
  if (_on_analog_output_change) {
    _on_analog_output_change(_output_state);
  } else {
    log_w("No callback function set for analog output change");
  }
}

bool ZigbeeAnalog::setAnalogInput(float analog) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  if (!(_analog_clusters & ANALOG_INPUT)) {
    log_e("Analog Input cluster not added");
    return false;
  }
  log_d("Setting analog input to %.1f", analog);
  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID, &analog, false
  );
  esp_zb_lock_release();
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set analog input: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeAnalog::setAnalogOutput(float analog) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  _output_state = analog;
  analogOutputChanged();

  log_v("Updating analog output to %.2f", analog);
  /* Update analog output */
  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID, &_output_state, false
  );
  esp_zb_lock_release();

  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set analog output: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeAnalog::reportAnalogInput() {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  report_attr_cmd.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  if (ret != ESP_OK) {
    log_e("Failed to send Analog Input report: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  log_v("Analog Input report sent");
  return true;
}

bool ZigbeeAnalog::reportAnalogOutput() {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  report_attr_cmd.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  if (ret != ESP_OK) {
    log_e("Failed to send Analog Output report: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  log_v("Analog Output report sent");
  return true;
}

bool ZigbeeAnalog::setAnalogInputReporting(uint16_t min_interval, uint16_t max_interval, float delta) {
  esp_zb_zcl_reporting_info_t reporting_info;
  memset(&reporting_info, 0, sizeof(esp_zb_zcl_reporting_info_t));
  reporting_info.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV;
  reporting_info.ep = _endpoint;
  reporting_info.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT;
  reporting_info.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
  reporting_info.attr_id = ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID;
  reporting_info.u.send_info.min_interval = min_interval;
  reporting_info.u.send_info.max_interval = max_interval;
  reporting_info.u.send_info.def_min_interval = min_interval;
  reporting_info.u.send_info.def_max_interval = max_interval;
  reporting_info.u.send_info.delta.s32 = delta;
  reporting_info.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  reporting_info.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_update_reporting_info(&reporting_info);
  esp_zb_lock_release();
  if (ret != ESP_OK) {
    log_e("Failed to set Analog Input reporting: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeAnalog::setAnalogInputDescription(const char *description) {
  if (!(_analog_clusters & ANALOG_INPUT)) {
    log_e("Analog Input cluster not added");
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

  // Get and check the analog input cluster
  esp_zb_attribute_list_t *analog_input_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (analog_input_cluster == nullptr) {
    log_e("Failed to get analog input cluster");
    return false;
  }

  // Store the length as the first element
  zb_description[0] = static_cast<char>(description_length);  // Cast size_t to char
  // Use memcpy to copy the characters to the result array
  memcpy(zb_description + 1, description, description_length);
  // Null-terminate the array
  zb_description[description_length + 1] = '\0';

  // Update the description attribute
  esp_err_t ret = esp_zb_cluster_update_attr(analog_input_cluster, ESP_ZB_ZCL_ATTR_ANALOG_INPUT_DESCRIPTION_ID, (void *)zb_description);
  if (ret != ESP_OK) {
    log_e("Failed to set description: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeAnalog::setAnalogOutputDescription(const char *description) {
  if (!(_analog_clusters & ANALOG_OUTPUT)) {
    log_e("Analog Output cluster not added");
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

  // Get and check the analog output cluster
  esp_zb_attribute_list_t *analog_output_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (analog_output_cluster == nullptr) {
    log_e("Failed to get analog output cluster");
    return false;
  }

  // Store the length as the first element
  zb_description[0] = static_cast<char>(description_length);  // Cast size_t to char
  // Use memcpy to copy the characters to the result array
  memcpy(zb_description + 1, description, description_length);
  // Null-terminate the array
  zb_description[description_length + 1] = '\0';

  // Update the description attribute
  esp_err_t ret = esp_zb_cluster_update_attr(analog_output_cluster, ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_DESCRIPTION_ID, (void *)zb_description);
  if (ret != ESP_OK) {
    log_e("Failed to set description: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeAnalog::setAnalogInputResolution(float resolution) {
  if (!(_analog_clusters & ANALOG_INPUT)) {
    log_e("Analog Input cluster not added");
    return false;
  }

  esp_zb_attribute_list_t *analog_input_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (analog_input_cluster == nullptr) {
    log_e("Failed to get analog input cluster");
    return false;
  }

  esp_err_t ret = esp_zb_cluster_update_attr(analog_input_cluster, ESP_ZB_ZCL_ATTR_ANALOG_INPUT_RESOLUTION_ID, (void *)&resolution);
  if (ret != ESP_OK) {
    log_e("Failed to set resolution: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeAnalog::setAnalogOutputResolution(float resolution) {
  if (!(_analog_clusters & ANALOG_OUTPUT)) {
    log_e("Analog Output cluster not added");
    return false;
  }

  esp_zb_attribute_list_t *analog_output_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (analog_output_cluster == nullptr) {
    log_e("Failed to get analog output cluster");
    return false;
  }

  esp_err_t ret = esp_zb_cluster_update_attr(analog_output_cluster, ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_RESOLUTION_ID, (void *)&resolution);
  if (ret != ESP_OK) {
    log_e("Failed to set resolution: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

#endif  // CONFIG_ZB_ENABLED
