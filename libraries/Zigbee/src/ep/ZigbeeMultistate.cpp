#include "ZigbeeMultistate.h"
#if CONFIG_ZB_ENABLED
#include "ezbee/zha.h"
#include "ezbee/zcl/cluster/multistate_input_desc.h"
#include "ezbee/zcl/cluster/multistate_output_desc.h"

// NOTE(zb-v2): The v1 build needed an extern-"C" shim that aliased esp_zb_zcl_multistate_input_init_*()
// onto the misnamed esp_zb_zcl_multi_input_init_*() symbols (ESP-ZIGBEE-SDK 1.6.6 bug). v2.x ships the
// MultistateInput cluster under its correct name (ezb_zcl_multistate_input_cluster_server_init), so the
// workaround is removed. TODO(zb-v2): confirm no equivalent init alias is required once v2.x links.

// NOTE(zb-v2): v2.x does not expose application-type "group id" macros. The group id is the high byte
// of the 32-bit ApplicationType value, so the ZCL-defined values are inlined here (matching v1):
// Multistate Input = 0x0D, Multistate Output = 0x0E.
#define ZB_MULTISTATE_INPUT_GROUP_ID  0x0D
#define ZB_MULTISTATE_OUTPUT_GROUP_ID 0x0E

ZigbeeMultistate::ZigbeeMultistate(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID;

  // v2.x data model: build the endpoint descriptor manually with Basic + Identify server clusters.
  // Multistate Input/Output clusters are attached later by addMultistateInput()/addMultistateOutput().
  _ep_config = {.ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};

  // Initialize member variables
  _multistate_clusters = 0;
  _input_state = 0;
  _output_state = 0;
  _input_state_names_length = 0;
  _output_state_names_length = 0;
  // _input_state_names = nullptr;
  // _output_state_names = nullptr;
  _on_multistate_output_change = nullptr;
    _ep_desc = ezb_af_create_endpoint_desc(&_ep_config);
    if (_ep_desc == nullptr) {
    log_e("Failed to create multistate endpoint descriptor");
    }
    ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_basic_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
    ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_identify_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
}


bool ZigbeeMultistate::addMultistateInput() {
  if (!requireBeforeAddEndpoint("addMultistateInput")) {
    return false;
  }
  ezb_zcl_multistate_input_cluster_config_t multistate_input_cfg = {
    .number_of_states = 3, .out_of_service = false, .present_value = 0, .status_flags = EZB_ZCL_MULTISTATE_INPUT_STATUS_FLAGS_DEFAULT_VALUE
  };

  ezb_zcl_cluster_desc_t multistate_input_cluster = ezb_zcl_multistate_input_create_cluster_desc(&multistate_input_cfg, EZB_ZCL_CLUSTER_SERVER);

  // Create default description for Multistate Input
  char default_description[] = "\x10"                                              // Size of the description text
                             "Multistate Input";                                 // Description text
  uint32_t application_type = 0x00000000 | (ZB_MULTISTATE_INPUT_GROUP_ID << 24);  // Application type
  // const char* state_text[] = { "Off", "On", "Auto" }; // State text array

  ezb_err_t ret = ezb_zcl_multistate_input_cluster_desc_add_attr(multistate_input_cluster, EZB_ZCL_ATTR_MULTISTATE_INPUT_DESCRIPTION_ID, (void *)default_description);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add description attribute: 0x%x", ret);
    return false;
  }

  ret = ezb_zcl_multistate_input_cluster_desc_add_attr(multistate_input_cluster, EZB_ZCL_ATTR_MULTISTATE_INPUT_APPLICATION_TYPE_ID, (void *)&application_type);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add application type attribute: 0x%x", ret);
    return false;
  }

  // ret = ezb_zcl_multistate_input_cluster_desc_add_attr(multistate_input_cluster, EZB_ZCL_ATTR_MULTISTATE_INPUT_STATE_TEXT_ID, (void *)state_text);
  // if (ret != EZB_ERR_NONE) {
  //   log_e("Failed to add state text attribute: 0x%x", ret);
  //   return false;
  // }

  ret = ezb_af_endpoint_add_cluster_desc(_ep_desc, multistate_input_cluster);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add Multistate Input cluster: 0x%x", ret);
    return false;
  }

  _multistate_clusters |= MULTISTATE_INPUT;
  return true;
}

bool ZigbeeMultistate::addMultistateOutput() {
  if (!requireBeforeAddEndpoint("addMultistateOutput")) {
    return false;
  }
  ezb_zcl_multistate_output_cluster_config_t multistate_output_cfg = {
    .number_of_states = 3, .out_of_service = false, .present_value = 0, .status_flags = EZB_ZCL_MULTISTATE_OUTPUT_STATUS_FLAGS_DEFAULT_VALUE
  };

  ezb_zcl_cluster_desc_t multistate_output_cluster = ezb_zcl_multistate_output_create_cluster_desc(&multistate_output_cfg, EZB_ZCL_CLUSTER_SERVER);

  // Create default description for Multistate Output
  char default_description[] = "\x11"                                           // Size of the description text
                             "Multistate Output";                             // Description text
  uint32_t application_type = 0x00000000 | (ZB_MULTISTATE_OUTPUT_GROUP_ID << 24);
  // const char* state_text[] = { "Off", "On", "Auto" }; // State text array

  ezb_err_t ret = ezb_zcl_multistate_output_cluster_desc_add_attr(multistate_output_cluster, EZB_ZCL_ATTR_MULTISTATE_OUTPUT_DESCRIPTION_ID, (void *)default_description);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add description attribute: 0x%x", ret);
    return false;
  }

  ret = ezb_zcl_multistate_output_cluster_desc_add_attr(multistate_output_cluster, EZB_ZCL_ATTR_MULTISTATE_OUTPUT_APPLICATION_TYPE_ID, (void *)&application_type);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add application type attribute: 0x%x", ret);
    return false;
  }

  // ret = ezb_zcl_multistate_output_cluster_desc_add_attr(multistate_output_cluster, EZB_ZCL_ATTR_MULTISTATE_OUTPUT_STATE_TEXT_ID, (void *)state_text);
  // if (ret != EZB_ERR_NONE) {
  //   log_e("Failed to add state text attribute: 0x%x", ret);
  //   return false;
  // }

  ret = ezb_af_endpoint_add_cluster_desc(_ep_desc, multistate_output_cluster);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add Multistate Output cluster: 0x%x", ret);
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

  _mi_application_type = (ZB_MULTISTATE_INPUT_GROUP_ID << 24) | application_type;
  return configureEpClusterAttr(
    "setMultistateInputApplication", EZB_ZCL_CLUSTER_ID_MULTISTATE_INPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_MULTISTATE_INPUT_APPLICATION_TYPE_ID,
    &_mi_application_type, ezb_zcl_multistate_input_cluster_desc_add_attr
  );
}

bool ZigbeeMultistate::setMultistateOutputApplication(uint32_t application_type) {
  if (!(_multistate_clusters & MULTISTATE_OUTPUT)) {
    log_e("Multistate Output cluster not added");
    return false;
  }

  _mo_application_type = (ZB_MULTISTATE_OUTPUT_GROUP_ID << 24) | application_type;
  return configureEpClusterAttr(
    "setMultistateOutputApplication", EZB_ZCL_CLUSTER_ID_MULTISTATE_OUTPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_MULTISTATE_OUTPUT_APPLICATION_TYPE_ID,
    &_mo_application_type, ezb_zcl_multistate_output_cluster_desc_add_attr
  );
}

bool ZigbeeMultistate::setMultistateInputDescription(const char *description) {
  if (!(_multistate_clusters & MULTISTATE_INPUT)) {
    log_e("Multistate Input cluster not added");
    return false;
  }

  size_t description_length = strlen(description);
  if (description_length > ZB_MAX_NAME_LENGTH) {
    log_e("Description is too long");
    return false;
  }

  _mi_description[0] = static_cast<char>(description_length);
  memcpy(_mi_description + 1, description, description_length);
  _mi_description[description_length + 1] = '\0';

  return configureEpClusterAttr(
    "setMultistateInputDescription", EZB_ZCL_CLUSTER_ID_MULTISTATE_INPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_MULTISTATE_INPUT_DESCRIPTION_ID,
    _mi_description, ezb_zcl_multistate_input_cluster_desc_add_attr
  );
}

bool ZigbeeMultistate::setMultistateOutputDescription(const char *description) {
  if (!(_multistate_clusters & MULTISTATE_OUTPUT)) {
    log_e("Multistate Output cluster not added");
    return false;
  }

  size_t description_length = strlen(description);
  if (description_length > ZB_MAX_NAME_LENGTH) {
    log_e("Description is too long");
    return false;
  }

  _mo_description[0] = static_cast<char>(description_length);
  memcpy(_mo_description + 1, description, description_length);
  _mo_description[description_length + 1] = '\0';

  return configureEpClusterAttr(
    "setMultistateOutputDescription", EZB_ZCL_CLUSTER_ID_MULTISTATE_OUTPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_MULTISTATE_OUTPUT_DESCRIPTION_ID,
    _mo_description, ezb_zcl_multistate_output_cluster_desc_add_attr
  );
}

bool ZigbeeMultistate::setMultistateInputStates(uint16_t number_of_states) {
  if (!(_multistate_clusters & MULTISTATE_INPUT)) {
    log_e("Multistate Input cluster not added");
    return false;
  }

  _input_state_names_length = number_of_states;
  return configureEpClusterAttr(
    "setMultistateInputStates", EZB_ZCL_CLUSTER_ID_MULTISTATE_INPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_MULTISTATE_INPUT_NUMBER_OF_STATES_ID,
    &_input_state_names_length, ezb_zcl_multistate_input_cluster_desc_add_attr
  );
}

bool ZigbeeMultistate::setMultistateOutputStates(uint16_t number_of_states) {
  if (!(_multistate_clusters & MULTISTATE_OUTPUT)) {
    log_e("Multistate Output cluster not added");
    return false;
  }

  _output_state_names_length = number_of_states;
  return configureEpClusterAttr(
    "setMultistateOutputStates", EZB_ZCL_CLUSTER_ID_MULTISTATE_OUTPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_MULTISTATE_OUTPUT_NUMBER_OF_STATES_ID,
    &_output_state_names_length, ezb_zcl_multistate_output_cluster_desc_add_attr
  );
}

/* TODO: revisit this after arrays are supported

bool ZigbeeMultistate::setMultistateInputStates(const char * const states[], uint16_t states_length) {
  if (!(_multistate_clusters & MULTISTATE_INPUT)) {
    log_e("Multistate Input cluster not added");
    return false;
  }

  ezb_zcl_cluster_desc_t multistate_input_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_MULTISTATE_INPUT, EZB_ZCL_CLUSTER_SERVER);
  if (multistate_input_cluster == nullptr) {
    log_e("Failed to get multistate input cluster");
    return false;
  }

  ezb_err_t ret = ezb_zcl_multistate_input_cluster_desc_add_attr(multistate_input_cluster, EZB_ZCL_ATTR_MULTISTATE_INPUT_STATE_TEXT_ID, (void *)states);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set states text: 0x%x", ret);
    return false;
  }
  ret = ezb_zcl_multistate_input_cluster_desc_add_attr(multistate_input_cluster, EZB_ZCL_ATTR_MULTISTATE_INPUT_NUMBER_OF_STATES_ID, (void *)&states_length);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set number of states: 0x%x", ret);
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

  ezb_zcl_cluster_desc_t multistate_output_cluster = ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_MULTISTATE_OUTPUT, EZB_ZCL_CLUSTER_SERVER);
  if (multistate_output_cluster == nullptr) {
    log_e("Failed to get multistate output cluster");
    return false;
  }

  ezb_err_t ret = ezb_zcl_multistate_output_cluster_desc_add_attr(multistate_output_cluster, EZB_ZCL_ATTR_MULTISTATE_OUTPUT_STATE_TEXT_ID, (void *)states);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set states text: 0x%x", ret);
    return false;
  }
  ret = ezb_zcl_multistate_output_cluster_desc_add_attr(multistate_output_cluster, EZB_ZCL_ATTR_MULTISTATE_OUTPUT_NUMBER_OF_STATES_ID, (void *)&states_length);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set number of states: 0x%x", ret);
    return false;
  }

  // Store state names locally for getter access
  _output_state_names = states;
  _output_state_names_length = states_length;
  return true;
}
*/

//set attribute method -> method overridden in child class
void ZigbeeMultistate::zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) {
  if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_MULTISTATE_INPUT) {
    if (message->in.attribute.id == EZB_ZCL_ATTR_MULTISTATE_INPUT_PRESENT_VALUE_ID && message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_UINT16) {
      _input_state = *(uint16_t *)message->in.attribute.data.value;
    } else {
      log_w("Received message ignored. Attribute ID: %u not supported for Multistate Input", message->in.attribute.id);
    }
  } else if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_MULTISTATE_OUTPUT) {
    if (message->in.attribute.id == EZB_ZCL_ATTR_MULTISTATE_OUTPUT_PRESENT_VALUE_ID && message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_UINT16) {
      _output_state = *(uint16_t *)message->in.attribute.data.value;
      multistateOutputChanged();
    } else {
      log_w("Received message ignored. Attribute ID: %u not supported for Multistate Output", message->in.attribute.id);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %u not supported for Multistate endpoint", message->info.cluster_id);
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
  if (!(_multistate_clusters & MULTISTATE_INPUT)) {
    log_e("Multistate Input cluster not added");
    return false;
  }
  log_d("Setting multistate input to %u", state);
  ezb_zcl_status_t ret =
    setClusterAttribute(EZB_ZCL_CLUSTER_ID_MULTISTATE_INPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_MULTISTATE_INPUT_PRESENT_VALUE_ID, &state, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set multistate input: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  _input_state = state;
  return true;
}

bool ZigbeeMultistate::setMultistateOutput(uint16_t state) {
  _output_state = state;
  multistateOutputChanged();

  log_v("Updating multistate output to %u", state);
  /* Update multistate output */
  ezb_zcl_status_t ret =
    setClusterAttribute(EZB_ZCL_CLUSTER_ID_MULTISTATE_OUTPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_MULTISTATE_OUTPUT_PRESENT_VALUE_ID, &_output_state, false);

  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set multistate output: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeMultistate::reportMultistateInput() {
  /* Send report attributes command */
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
  // No explicit destination: report to bound devices (replaces v1 ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT).
  ezb_address_set_none(&report_attr_cmd.cmd_ctrl.dst_addr);
  report_attr_cmd.cmd_ctrl.src_ep = _endpoint;
  report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_MULTISTATE_INPUT;
  report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.payload.attr_id = EZB_ZCL_ATTR_MULTISTATE_INPUT_PRESENT_VALUE_ID;

  if (!reportClusterAttribute(&report_attr_cmd)) {
    log_e("Failed to send Multistate Input report");
    return false;
  }
  log_v("Multistate Input report sent");
  return true;
}

bool ZigbeeMultistate::reportMultistateOutput() {
  /* Send report attributes command */
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
  ezb_address_set_none(&report_attr_cmd.cmd_ctrl.dst_addr);
  report_attr_cmd.cmd_ctrl.src_ep = _endpoint;
  report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_MULTISTATE_OUTPUT;
  report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.payload.attr_id = EZB_ZCL_ATTR_MULTISTATE_OUTPUT_PRESENT_VALUE_ID;

  if (!reportClusterAttribute(&report_attr_cmd)) {
    log_e("Failed to send Multistate Output report");
    return false;
  }
  log_v("Multistate Output report sent");
  return true;
}

#endif  // CONFIG_ZB_ENABLED
