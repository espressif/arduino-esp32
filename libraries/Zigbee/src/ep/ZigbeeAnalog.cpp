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
  esp_err_t ret = esp_zb_cluster_list_add_analog_input_cluster(_cluster_list, esp_zb_analog_input_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (ret != ESP_OK) {
    log_e("Failed to add Analog Input cluster: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  _analog_clusters |= ANALOG_INPUT;
  return true;
}

bool ZigbeeAnalog::addAnalogOutput() {
  esp_err_t ret = esp_zb_cluster_list_add_analog_output_cluster(_cluster_list, esp_zb_analog_output_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  if (ret != ESP_OK) {
    log_e("Failed to add Analog Output cluster: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  _analog_clusters |= ANALOG_OUTPUT;
  return true;
}

//set attribute method -> method overridden in child class
void ZigbeeAnalog::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ANALOG_OUTPUT) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_ANALOG_OUTPUT_PRESENT_VALUE_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_SINGLE) {
      float analog_output = *(float *)message->attribute.data.value;
      analogOutputChanged(analog_output);
    } else {
      log_w("Received message ignored. Attribute ID: %d not supported for Analog Output", message->attribute.id);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %d not supported for Analog endpoint", message->info.cluster);
  }
}

void ZigbeeAnalog::analogOutputChanged(float analog_output) {
  if (_on_analog_output_change) {
    _on_analog_output_change(analog_output);
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

#endif  // CONFIG_ZB_ENABLED
