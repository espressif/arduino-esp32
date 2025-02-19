#include "ZigbeeAnalog.h"
#if SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED

esp_zb_cluster_list_t *zigbee_analog_clusters_create(zigbee_analog_cfg_t *analog_sensor) {
  esp_zb_basic_cluster_cfg_t *basic_cfg = analog_sensor ? &(analog_sensor->basic_cfg) : NULL;
  esp_zb_identify_cluster_cfg_t *identify_cfg = analog_sensor ? &(analog_sensor->identify_cfg) : NULL;
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(cluster_list, esp_zb_basic_cluster_create(basic_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(identify_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  return cluster_list;
}

ZigbeeAnalog::ZigbeeAnalog(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID;

  //Create custom analog sensor configuration
  zigbee_analog_cfg_t analog_cfg = ZIGBEE_DEFAULT_ANALOG_CONFIG();
  _cluster_list = zigbee_analog_clusters_create(&analog_cfg);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};
}

void ZigbeeAnalog::addAnalogValue() {
  esp_zb_cluster_list_add_analog_value_cluster(_cluster_list, esp_zb_analog_value_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  _analog_clusters |= ANALOG_VALUE;
}

void ZigbeeAnalog::addAnalogInput() {
  esp_zb_cluster_list_add_analog_input_cluster(_cluster_list, esp_zb_analog_input_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  _analog_clusters |= ANALOG_INPUT;
}

void ZigbeeAnalog::addAnalogOutput() {
  esp_zb_cluster_list_add_analog_output_cluster(_cluster_list, esp_zb_analog_output_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  _analog_clusters |= ANALOG_OUTPUT;
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

void ZigbeeAnalog::setAnalogValue(float analog) {
  if (!(_analog_clusters & ANALOG_VALUE)) {
    log_e("Analog Value cluster not added");
    return;
  }
  // float zb_analog = analog;
  log_d("Setting analog value to %.1f", analog);
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_VALUE, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ANALOG_VALUE_PRESENT_VALUE_ID, &analog, false
  );
  esp_zb_lock_release();
}

void ZigbeeAnalog::setAnalogInput(float analog) {
  if (!(_analog_clusters & ANALOG_INPUT)) {
    log_e("Analog Input cluster not added");
    return;
  }
  // float zb_analog = analog;
  log_d("Setting analog input to %.1f", analog);
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID, &analog, false
  );
  esp_zb_lock_release();
}

void ZigbeeAnalog::reportAnalogInput() {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  log_v("Analog Input report sent");
}

void ZigbeeAnalog::setAnalogInputReporting(uint16_t min_interval, uint16_t max_interval, float delta) {
  esp_zb_zcl_reporting_info_t reporting_info;
  memset(&reporting_info, 0, sizeof(esp_zb_zcl_reporting_info_t));
  reporting_info.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV;
  reporting_info.ep = _endpoint;
  reporting_info.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT;
  reporting_info.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
  reporting_info.attr_id = ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID;
  reporting_info.u.send_info.min_interval = min_interval;
  reporting_info.u.send_info.max_interval = max_interval;
  reporting_info.u.send_info.def_min_interval = min_interval;
  reporting_info.u.send_info.def_max_interval = max_interval;
  reporting_info.u.send_info.delta.s32 = delta;
  reporting_info.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  reporting_info.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_update_reporting_info(&reporting_info);
  esp_zb_lock_release();
}

#endif  //SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED
