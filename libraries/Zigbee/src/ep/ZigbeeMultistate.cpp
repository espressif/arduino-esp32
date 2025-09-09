#include "ZigbeeMultistate.h"
#if CONFIG_ZB_ENABLED

// Workaround for ESP-ZIGBEE-SDK 1.6.6 known issue
#ifdef __cplusplus
extern "C" {
#endif
extern void esp_zb_zcl_multi_input_init_server(void);
extern void esp_zb_zcl_multi_input_init_client(void);

void esp_zb_zcl_multistate_input_init_server(void) {
  esp_zb_zcl_multi_input_init_server();
}
void esp_zb_zcl_multistate_input_init_client(void) {
  esp_zb_zcl_multi_input_init_client();
}
#ifdef __cplusplus
}
#endif

ZigbeeMultistate::ZigbeeMultistate(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID;

  //Create basic multistate clusters without configuration
  _cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(_cluster_list, esp_zb_basic_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(_cluster_list, esp_zb_identify_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};

  // Initialize member variables
  _multistate_clusters = 0;
  _input_state = 0;
  _output_state = 0;
  _input_state_names_length = 0;
  _output_state_names_length = 0;
  // _input_state_names = nullptr;
  // _output_state_names = nullptr;
  _on_multistate_output_change = nullptr;
}

bool ZigbeeMultistate::addMultistateInput() {
  esp_zb_multistate_input_cluster_cfg_t multistate_input_cfg = {
    .number_of_states = 3, .out_of_service = false, .present_value = 0, .status_flags = ESP_ZB_ZCL_MULTI_VALUE_STATUS_FLAGS_NORMAL
  };

  esp_zb_attribute_list_t *multistate_input_cluster = esp_zb_multistate_input_cluster_create(&multistate_input_cfg);

  // Create default description for Multistate Input
  char default_description[] = "\x10"                     // Size of the description text
                               "Multistate Input";        // Description text
  uint32_t application_type = 0x00000000 | (0x0D << 24);  // Application type
  // const char* state_text[] = { "Off", "On", "Auto" }; // State text array

  esp_err_t ret = esp_zb_multistate_input_cluster_add_attr(multistate_input_cluster, ESP_ZB_ZCL_ATTR_MULTI_INPUT_DESCRIPTION_ID, (void *)default_description);
  if (ret != ESP_OK) {
    log_e("Failed to add description attribute: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  ret = esp_zb_multistate_input_cluster_add_attr(multistate_input_cluster, ESP_ZB_ZCL_ATTR_MULTI_INPUT_APPLICATION_TYPE_ID, (void *)&application_type);
  if (ret != ESP_OK) {
    log_e("Failed to add application type attribute: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  // ret = esp_zb_multistate_input_cluster_add_attr(multistate_input_cluster, ESP_ZB_ZCL_ATTR_MULTI_INPUT_STATE_TEXT_ID, (void *)state_text);
  // if (ret != ESP_OK) {
  //   log_e("Failed to add state text attribute: 0x%x: %s", ret, esp_err_to_name(ret));
  //   return false;
  // }

  ret = esp_zb_cluster_list_add_multistate_input_cluster(_cluster_list, multistate_input_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (ret != ESP_OK) {
    log_e("Failed to add Multistate Input cluster: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  _multistate_clusters |= MULTISTATE_INPUT;
  return true;
}

bool ZigbeeMultistate::addMultistateOutput() {
  esp_zb_multistate_output_cluster_cfg_t multistate_output_cfg = {
    .number_of_states = 3, .out_of_service = false, .present_value = 0, .status_flags = ESP_ZB_ZCL_MULTI_VALUE_STATUS_FLAGS_NORMAL
  };

  esp_zb_attribute_list_t *multistate_output_cluster = esp_zb_multistate_output_cluster_create(&multistate_output_cfg);

  // Create default description for Multistate Output
  char default_description[] = "\x11"                // Size of the description text
                               "Multistate Output";  // Description text
  uint32_t application_type = 0x00000000 | (0x0E << 24);
  // const char* state_text[] = { "Off", "On", "Auto" }; // State text array

  esp_err_t ret =
    esp_zb_multistate_output_cluster_add_attr(multistate_output_cluster, ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_DESCRIPTION_ID, (void *)default_description);
  if (ret != ESP_OK) {
    log_e("Failed to add description attribute: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  ret = esp_zb_multistate_output_cluster_add_attr(multistate_output_cluster, ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_APPLICATION_TYPE_ID, (void *)&application_type);
  if (ret != ESP_OK) {
    log_e("Failed to add application type attribute: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  // ret = esp_zb_multistate_output_cluster_add_attr(multistate_output_cluster, ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_STATE_TEXT_ID, (void *)state_text);
  // if (ret != ESP_OK) {
  //   log_e("Failed to add state text attribute: 0x%x: %s", ret, esp_err_to_name(ret));
  //   return false;
  // }

  ret = esp_zb_cluster_list_add_multistate_output_cluster(_cluster_list, multistate_output_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (ret != ESP_OK) {
    log_e("Failed to add Multistate Output cluster: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  _multistate_clusters |= MULTISTATE_OUTPUT;
  return true;
}

bool ZigbeeMultistate::setMultistateInputApplication(uint32_t application_type) {
  if (!(_multistate_clusters & MULTISTATE_INPUT)) {
    log_e("Multistate Input cluster not added");
    return false;
  }

  // Add the Multistate Input group ID (0x0D) to the application type
  uint32_t application_type_value = (0x0D << 24) | application_type;

  esp_zb_attribute_list_t *multistate_input_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_MULTI_INPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_err_t ret = esp_zb_cluster_update_attr(multistate_input_cluster, ESP_ZB_ZCL_ATTR_MULTI_INPUT_APPLICATION_TYPE_ID, (void *)&application_type_value);
  if (ret != ESP_OK) {
    log_e("Failed to set Multistate Input application type: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeMultistate::setMultistateOutputApplication(uint32_t application_type) {
  if (!(_multistate_clusters & MULTISTATE_OUTPUT)) {
    log_e("Multistate Output cluster not added");
    return false;
  }

  // Add the Multistate Output group ID (0x0E) to the application type
  uint32_t application_type_value = (0x0E << 24) | application_type;

  esp_zb_attribute_list_t *multistate_output_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_MULTI_OUTPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_err_t ret = esp_zb_cluster_update_attr(multistate_output_cluster, ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_APPLICATION_TYPE_ID, (void *)&application_type_value);
  if (ret != ESP_OK) {
    log_e("Failed to set Multistate Output application type: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeMultistate::setMultistateInputDescription(const char *description) {
  if (!(_multistate_clusters & MULTISTATE_INPUT)) {
    log_e("Multistate Input cluster not added");
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

  // Get and check the multistate input cluster
  esp_zb_attribute_list_t *multistate_input_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_MULTI_INPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (multistate_input_cluster == nullptr) {
    log_e("Failed to get multistate input cluster");
    return false;
  }

  // Store the length as the first element
  zb_description[0] = static_cast<char>(description_length);  // Cast size_t to char
  // Use memcpy to copy the characters to the result array
  memcpy(zb_description + 1, description, description_length);
  // Null-terminate the array
  zb_description[description_length + 1] = '\0';

  // Update the description attribute
  esp_err_t ret = esp_zb_cluster_update_attr(multistate_input_cluster, ESP_ZB_ZCL_ATTR_MULTI_INPUT_DESCRIPTION_ID, (void *)zb_description);
  if (ret != ESP_OK) {
    log_e("Failed to set description: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeMultistate::setMultistateOutputDescription(const char *description) {
  if (!(_multistate_clusters & MULTISTATE_OUTPUT)) {
    log_e("Multistate Output cluster not added");
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

  // Get and check the multistate output cluster
  esp_zb_attribute_list_t *multistate_output_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_MULTI_OUTPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (multistate_output_cluster == nullptr) {
    log_e("Failed to get multistate output cluster");
    return false;
  }

  // Store the length as the first element
  zb_description[0] = static_cast<char>(description_length);  // Cast size_t to char
  // Use memcpy to copy the characters to the result array
  memcpy(zb_description + 1, description, description_length);
  // Null-terminate the array
  zb_description[description_length + 1] = '\0';

  // Update the description attribute
  esp_err_t ret = esp_zb_cluster_update_attr(multistate_output_cluster, ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_DESCRIPTION_ID, (void *)zb_description);
  if (ret != ESP_OK) {
    log_e("Failed to set description: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeMultistate::setMultistateInputStates(uint16_t number_of_states) {
  if (!(_multistate_clusters & MULTISTATE_INPUT)) {
    log_e("Multistate Input cluster not added");
    return false;
  }

  esp_zb_attribute_list_t *multistate_input_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_MULTI_INPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (multistate_input_cluster == nullptr) {
    log_e("Failed to get multistate input cluster");
    return false;
  }

  esp_err_t ret = esp_zb_cluster_update_attr(multistate_input_cluster, ESP_ZB_ZCL_ATTR_MULTI_INPUT_NUMBER_OF_STATES_ID, (void *)&number_of_states);
  if (ret != ESP_OK) {
    log_e("Failed to set number of states: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  _input_state_names_length = number_of_states;
  return true;
}

bool ZigbeeMultistate::setMultistateOutputStates(uint16_t number_of_states) {
  if (!(_multistate_clusters & MULTISTATE_OUTPUT)) {
    log_e("Multistate Output cluster not added");
    return false;
  }

  esp_zb_attribute_list_t *multistate_output_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_MULTI_OUTPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (multistate_output_cluster == nullptr) {
    log_e("Failed to get multistate output cluster");
    return false;
  }

  esp_err_t ret = esp_zb_cluster_update_attr(multistate_output_cluster, ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_NUMBER_OF_STATES_ID, (void *)&number_of_states);
  if (ret != ESP_OK) {
    log_e("Failed to set number of states: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  _output_state_names_length = number_of_states;
  return true;
}

/* TODO: revisit this after arrays are supported

bool ZigbeeMultistate::setMultistateInputStates(const char * const states[], uint16_t states_length) {
  if (!(_multistate_clusters & MULTISTATE_INPUT)) {
    log_e("Multistate Input cluster not added");
    return false;
  }

  esp_zb_attribute_list_t *multistate_input_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_MULTI_INPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (multistate_input_cluster == nullptr) {
    log_e("Failed to get multistate input cluster");
    return false;
  }

  esp_err_t ret = esp_zb_cluster_update_attr(multistate_input_cluster, ESP_ZB_ZCL_ATTR_MULTI_INPUT_STATE_TEXT_ID, (void *)states);
  if (ret != ESP_OK) {
    log_e("Failed to set states text: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  ret = esp_zb_cluster_update_attr(multistate_input_cluster, ESP_ZB_ZCL_ATTR_MULTI_INPUT_NUMBER_OF_STATES_ID, (void *)&states_length);
  if (ret != ESP_OK) {
    log_e("Failed to set number of states: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  // Store state names locally for getter access
  _input_state_names = states;
  _input_state_names_length = states_length;
  return true;
}

bool ZigbeeMultistate::setMultistateOutputStates(const char * const states[], uint16_t states_length) {
  if (!(_multistate_clusters & MULTISTATE_OUTPUT)) {
    log_e("Multistate Output cluster not added");
    return false;
  }

  esp_zb_attribute_list_t *multistate_output_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_MULTI_OUTPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (multistate_output_cluster == nullptr) {
    log_e("Failed to get multistate output cluster");
    return false;
  }

  esp_err_t ret = esp_zb_cluster_update_attr(multistate_output_cluster, ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_STATE_TEXT_ID, (void *)states);
  if (ret != ESP_OK) {
    log_e("Failed to set states text: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  ret = esp_zb_cluster_update_attr(multistate_output_cluster, ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_NUMBER_OF_STATES_ID, (void *)&states_length);
  if (ret != ESP_OK) {
    log_e("Failed to set number of states: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }

  // Store state names locally for getter access
  _output_state_names = states;
  _output_state_names_length = states_length;
  return true;
}
*/

//set attribute method -> method overridden in child class
void ZigbeeMultistate::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_MULTI_OUTPUT) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_PRESENT_VALUE_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
      _output_state = *(uint16_t *)message->attribute.data.value;
      multistateOutputChanged();
    } else {
      log_w("Received message ignored. Attribute ID: %d not supported for Multistate Output", message->attribute.id);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %d not supported for Multistate endpoint", message->info.cluster);
  }
}

void ZigbeeMultistate::multistateOutputChanged() {
  if (_on_multistate_output_change) {
    _on_multistate_output_change(_output_state);
  } else {
    log_w("No callback function set for multistate output change");
  }
}

bool ZigbeeMultistate::setMultistateInput(uint16_t state) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  if (!(_multistate_clusters & MULTISTATE_INPUT)) {
    log_e("Multistate Input cluster not added");
    return false;
  }
  log_d("Setting multistate input to %d", state);
  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_MULTI_INPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_MULTI_INPUT_PRESENT_VALUE_ID, &state, false
  );
  esp_zb_lock_release();
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set multistate input: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeMultistate::setMultistateOutput(uint16_t state) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  _output_state = state;
  multistateOutputChanged();

  log_v("Updating multistate output to %d", state);
  /* Update multistate output */
  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_MULTI_OUTPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_PRESENT_VALUE_ID, &_output_state, false
  );
  esp_zb_lock_release();

  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set multistate output: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeMultistate::reportMultistateInput() {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_MULTI_INPUT_PRESENT_VALUE_ID;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_MULTI_INPUT;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  report_attr_cmd.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  if (ret != ESP_OK) {
    log_e("Failed to send Multistate Input report: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  log_v("Multistate Input report sent");
  return true;
}

bool ZigbeeMultistate::reportMultistateOutput() {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_MULTI_OUTPUT_PRESENT_VALUE_ID;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_MULTI_OUTPUT;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  report_attr_cmd.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  if (ret != ESP_OK) {
    log_e("Failed to send Multistate Output report: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  log_v("Multistate Output report sent");
  return true;
}

#endif  // CONFIG_ZB_ENABLED
