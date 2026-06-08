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

#include "ZigbeeAnalog.h"
#if CONFIG_ZB_ENABLED
#include "ezbee/zha.h"
#include "ezbee/zcl/cluster/analog_input_desc.h"
#include "ezbee/zcl/cluster/analog_output_desc.h"
#include <cfloat>

// NOTE(zb-v2): v2.x does not expose application-type "group id" macros (v1 ESP_ZB_ZCL_AI_GROUP_ID /
// ESP_ZB_ZCL_AO_GROUP_ID). The group id is the high byte of the 32-bit ApplicationType value, so the
// ZCL-defined values are inlined here: Analog Input = 0x00, Analog Output = 0x01.
#define ZB_ANALOG_INPUT_GROUP_ID  0x00
#define ZB_ANALOG_OUTPUT_GROUP_ID 0x01

ZigbeeAnalog::ZigbeeAnalog(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID;
  _on_analog_output_change = nullptr;

  // v2.x data model: build the endpoint descriptor manually with Basic + Identify server clusters.
  // Analog Input/Output clusters are attached later by addAnalogInput()/addAnalogOutput().
  _ep_config = {.ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};
    _ep_desc = ezb_af_create_endpoint_desc(&_ep_config);
    if (_ep_desc == nullptr) {
    log_e("Failed to create analog endpoint descriptor");
    }
    ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_basic_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
    ezb_af_endpoint_add_cluster_desc(_ep_desc, ezb_zcl_identify_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER));
}



bool ZigbeeAnalog::addAnalogInput() {
  if (!requireBeforeAddEndpoint("addAnalogInput")) {
    return false;
  }
  ezb_zcl_cluster_desc_t analog_input_cluster = ezb_zcl_analog_input_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER);

  // Create default description for Analog Input
  char default_description[] = "\x0C"
                             "Analog Input";
  uint32_t application_type = 0x00000000 | (ZB_ANALOG_INPUT_GROUP_ID << 24);
  float resolution = 0.1;  // Default resolution of 0.1
  float min = -FLT_MAX;    // Default min value for float
  float max = FLT_MAX;     // Default max value for float

  ezb_err_t ret = ezb_zcl_analog_input_cluster_desc_add_attr(analog_input_cluster, EZB_ZCL_ATTR_ANALOG_INPUT_DESCRIPTION_ID, (void *)default_description);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add description attribute: 0x%x", ret);
    return false;
  }

  ret = ezb_zcl_analog_input_cluster_desc_add_attr(analog_input_cluster, EZB_ZCL_ATTR_ANALOG_INPUT_APPLICATION_TYPE_ID, (void *)&application_type);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add application type attribute: 0x%x", ret);
    return false;
  }

  ret = ezb_zcl_analog_input_cluster_desc_add_attr(analog_input_cluster, EZB_ZCL_ATTR_ANALOG_INPUT_RESOLUTION_ID, (void *)&resolution);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add resolution attribute: 0x%x", ret);
    return false;
  }

  ret = ezb_zcl_analog_input_cluster_desc_add_attr(analog_input_cluster, EZB_ZCL_ATTR_ANALOG_INPUT_MIN_PRESENT_VALUE_ID, (void *)&min);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set min value: 0x%x", ret);
    return false;
  }

  ret = ezb_zcl_analog_input_cluster_desc_add_attr(analog_input_cluster, EZB_ZCL_ATTR_ANALOG_INPUT_MAX_PRESENT_VALUE_ID, (void *)&max);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set max value: 0x%x", ret);
    return false;
  }

  ret = ezb_af_endpoint_add_cluster_desc(_ep_desc, analog_input_cluster);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add Analog Input cluster: 0x%x", ret);
    return false;
  }

  _analog_clusters |= ANALOG_INPUT;
  return true;
}

// Check the AnalogInput ApplicationType enumeration (analog_input_desc.h) for application type values
bool ZigbeeAnalog::setAnalogInputApplication(uint32_t application_type) {
  if (!(_analog_clusters & ANALOG_INPUT)) {
    log_e("Analog Input cluster not added");
    return false;
  }

  _ai_application_type = (ZB_ANALOG_INPUT_GROUP_ID << 24) | application_type;
  return configureEpClusterAttr(
    "setAnalogInputApplication", EZB_ZCL_CLUSTER_ID_ANALOG_INPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ANALOG_INPUT_APPLICATION_TYPE_ID,
    &_ai_application_type, ezb_zcl_analog_input_cluster_desc_add_attr
  );
}

bool ZigbeeAnalog::addAnalogOutput() {
  if (!requireBeforeAddEndpoint("addAnalogOutput")) {
    return false;
  }
  ezb_zcl_cluster_desc_t analog_output_cluster = ezb_zcl_analog_output_create_cluster_desc(nullptr, EZB_ZCL_CLUSTER_SERVER);

  // Create default description for Analog Output
  char default_description[] = "\x0D"
                             "Analog Output";
  uint32_t application_type = 0x00000000 | (ZB_ANALOG_OUTPUT_GROUP_ID << 24);
  float resolution = 1;  // Default resolution of 1
  float min = -FLT_MAX;  // Default min value for float
  float max = FLT_MAX;   // Default max value for float

  ezb_err_t ret = ezb_zcl_analog_output_cluster_desc_add_attr(analog_output_cluster, EZB_ZCL_ATTR_ANALOG_OUTPUT_DESCRIPTION_ID, (void *)default_description);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add description attribute: 0x%x", ret);
    return false;
  }

  ret = ezb_zcl_analog_output_cluster_desc_add_attr(analog_output_cluster, EZB_ZCL_ATTR_ANALOG_OUTPUT_APPLICATION_TYPE_ID, (void *)&application_type);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add application type attribute: 0x%x", ret);
    return false;
  }

  ret = ezb_zcl_analog_output_cluster_desc_add_attr(analog_output_cluster, EZB_ZCL_ATTR_ANALOG_OUTPUT_RESOLUTION_ID, (void *)&resolution);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add resolution attribute: 0x%x", ret);
    return false;
  }

  ret = ezb_zcl_analog_output_cluster_desc_add_attr(analog_output_cluster, EZB_ZCL_ATTR_ANALOG_OUTPUT_MIN_PRESENT_VALUE_ID, (void *)&min);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set min value: 0x%x", ret);
    return false;
  }

  ret = ezb_zcl_analog_output_cluster_desc_add_attr(analog_output_cluster, EZB_ZCL_ATTR_ANALOG_OUTPUT_MAX_PRESENT_VALUE_ID, (void *)&max);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set max value: 0x%x", ret);
    return false;
  }

  ret = ezb_af_endpoint_add_cluster_desc(_ep_desc, analog_output_cluster);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to add Analog Output cluster: 0x%x", ret);
    return false;
  }
  _analog_clusters |= ANALOG_OUTPUT;
  return true;
}

// Check the AnalogOutput ApplicationType enumeration (analog_output_desc.h) for application type values
bool ZigbeeAnalog::setAnalogOutputApplication(uint32_t application_type) {
  if (!(_analog_clusters & ANALOG_OUTPUT)) {
    log_e("Analog Output cluster not added");
    return false;
  }

  _ao_application_type = (ZB_ANALOG_OUTPUT_GROUP_ID << 24) | application_type;
  return configureEpClusterAttr(
    "setAnalogOutputApplication", EZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ANALOG_OUTPUT_APPLICATION_TYPE_ID,
    &_ao_application_type, ezb_zcl_analog_output_cluster_desc_add_attr
  );
}

//set attribute method -> method overridden in child class
void ZigbeeAnalog::zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) {
  if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT) {
    if (message->in.attribute.id == EZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID && message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_SINGLE) {
      _output_state = *(float *)message->in.attribute.data.value;
      analogOutputChanged();
    } else {
      log_w("Received message ignored. Attribute ID: %u not supported for Analog Output", message->in.attribute.id);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %u not supported for Analog endpoint", message->info.cluster_id);
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
  if (!(_analog_clusters & ANALOG_INPUT)) {
    log_e("Analog Input cluster not added");
    return false;
  }
  log_d("Setting analog input to %.1f", analog);
  ezb_zcl_status_t ret =
    setClusterAttribute(EZB_ZCL_CLUSTER_ID_ANALOG_INPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID, &analog, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set analog input: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeAnalog::setAnalogOutput(float analog) {
  _output_state = analog;
  analogOutputChanged();

  log_v("Updating analog output to %.2f", analog);
  ezb_zcl_status_t ret =
    setClusterAttribute(EZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID, &_output_state, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set analog output: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeAnalog::reportAnalogInput() {
  /* Send report attributes command */
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
  // No explicit destination: report to bound devices (replaces v1 ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT).
  ezb_address_set_none(&report_attr_cmd.cmd_ctrl.dst_addr);
  report_attr_cmd.cmd_ctrl.src_ep = _endpoint;
  report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_ANALOG_INPUT;
  report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.payload.attr_id = EZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID;

  if (!reportClusterAttribute(&report_attr_cmd)) {
    log_e("Failed to send Analog Input report");
    return false;
  }
  log_v("Analog Input report sent");
  return true;
}

bool ZigbeeAnalog::reportAnalogOutput() {
  /* Send report attributes command */
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
  ezb_address_set_none(&report_attr_cmd.cmd_ctrl.dst_addr);
  report_attr_cmd.cmd_ctrl.src_ep = _endpoint;
  report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT;
  report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.payload.attr_id = EZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID;

  if (!reportClusterAttribute(&report_attr_cmd)) {
    log_e("Failed to send Analog Output report");
    return false;
  }
  log_v("Analog Output report sent");
  return true;
}

bool ZigbeeAnalog::setAnalogInputReporting(uint16_t min_interval, uint16_t max_interval, float delta) {
  // NOTE(zb-v2): Reporting is now handle-based; the reporting record is created by the stack when the
  // endpoint is registered, so this must be called after Zigbee.begin(). Look up the handle, tune the
  // intervals/reportable change, then start the report.
  ezb_zcl_reporting_info_t reporting_info = ezb_zcl_reporting_info_find(
    _endpoint, EZB_ZCL_CLUSTER_ID_ANALOG_INPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID, EZB_ZCL_STD_MANUF_CODE
  );
  if (reporting_info == EZB_ZCL_INVALID_REPORTING_INFO) {
    log_e("Failed to find reporting info for analog input");
    return false;
  }
  ezb_zcl_attr_variable_t delta_var = {};
  delta_var.f32 = delta;  // PresentValue is a single-precision float
  ezb_zcl_reporting_info_update(reporting_info, min_interval, max_interval, &delta_var);
  ezb_zcl_reporting_info_update_default_interval(reporting_info, min_interval, max_interval);
  return setClusterReporting(reporting_info);
}

bool ZigbeeAnalog::setAnalogInputDescription(const char *description) {
  if (!(_analog_clusters & ANALOG_INPUT)) {
    log_e("Analog Input cluster not added");
    return false;
  }

  size_t description_length = strlen(description);
  if (description_length > ZB_MAX_NAME_LENGTH) {
    log_e("Description is too long");
    return false;
  }

  _ai_description[0] = static_cast<char>(description_length);
  memcpy(_ai_description + 1, description, description_length);
  _ai_description[description_length + 1] = '\0';

  return configureEpClusterAttr(
    "setAnalogInputDescription", EZB_ZCL_CLUSTER_ID_ANALOG_INPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ANALOG_INPUT_DESCRIPTION_ID, _ai_description,
    ezb_zcl_analog_input_cluster_desc_add_attr
  );
}

bool ZigbeeAnalog::setAnalogOutputDescription(const char *description) {
  if (!(_analog_clusters & ANALOG_OUTPUT)) {
    log_e("Analog Output cluster not added");
    return false;
  }

  size_t description_length = strlen(description);
  if (description_length > ZB_MAX_NAME_LENGTH) {
    log_e("Description is too long");
    return false;
  }

  _ao_description[0] = static_cast<char>(description_length);
  memcpy(_ao_description + 1, description, description_length);
  _ao_description[description_length + 1] = '\0';

  return configureEpClusterAttr(
    "setAnalogOutputDescription", EZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ANALOG_OUTPUT_DESCRIPTION_ID, _ao_description,
    ezb_zcl_analog_output_cluster_desc_add_attr
  );
}

bool ZigbeeAnalog::setAnalogInputResolution(float resolution) {
  if (!(_analog_clusters & ANALOG_INPUT)) {
    log_e("Analog Input cluster not added");
    return false;
  }

  _ai_resolution = resolution;
  return configureEpClusterAttr(
    "setAnalogInputResolution", EZB_ZCL_CLUSTER_ID_ANALOG_INPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ANALOG_INPUT_RESOLUTION_ID, &_ai_resolution,
    ezb_zcl_analog_input_cluster_desc_add_attr
  );
}

bool ZigbeeAnalog::setAnalogOutputResolution(float resolution) {
  if (!(_analog_clusters & ANALOG_OUTPUT)) {
    log_e("Analog Output cluster not added");
    return false;
  }

  _ao_resolution = resolution;
  return configureEpClusterAttr(
    "setAnalogOutputResolution", EZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ANALOG_OUTPUT_RESOLUTION_ID, &_ao_resolution,
    ezb_zcl_analog_output_cluster_desc_add_attr
  );
}

bool ZigbeeAnalog::setAnalogOutputMinMax(float min, float max) {
  if (!(_analog_clusters & ANALOG_OUTPUT)) {
    log_e("Analog Output cluster not added");
    return false;
  }

  _ao_min = min;
  _ao_max = max;
  if (!configureEpClusterAttr(
        "setAnalogOutputMinMax", EZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ANALOG_OUTPUT_MIN_PRESENT_VALUE_ID, &_ao_min,
        ezb_zcl_analog_output_cluster_desc_add_attr
      )) {
    return false;
  }
  return configureEpClusterAttr(
    "setAnalogOutputMinMax", EZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ANALOG_OUTPUT_MAX_PRESENT_VALUE_ID, &_ao_max,
    ezb_zcl_analog_output_cluster_desc_add_attr
  );
}

bool ZigbeeAnalog::setAnalogInputMinMax(float min, float max) {
  if (!(_analog_clusters & ANALOG_INPUT)) {
    log_e("Analog Input cluster not added");
    return false;
  }

  _ai_min = min;
  _ai_max = max;
  if (!configureEpClusterAttr(
        "setAnalogInputMinMax", EZB_ZCL_CLUSTER_ID_ANALOG_INPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ANALOG_INPUT_MIN_PRESENT_VALUE_ID, &_ai_min,
        ezb_zcl_analog_input_cluster_desc_add_attr
      )) {
    return false;
  }
  return configureEpClusterAttr(
    "setAnalogInputMinMax", EZB_ZCL_CLUSTER_ID_ANALOG_INPUT, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_ANALOG_INPUT_MAX_PRESENT_VALUE_ID, &_ai_max,
    ezb_zcl_analog_input_cluster_desc_add_attr
  );
}

#endif  // CONFIG_ZB_ENABLED
