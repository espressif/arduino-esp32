#include "ZigbeeAnalogSensor.h"
#if SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED

esp_zb_cluster_list_t *zigbee_analog_sensor_clusters_create(zigbee_analog_sensor_cfg_t *analog_sensor) {
  esp_zb_basic_cluster_cfg_t *basic_cfg = analog_sensor ? &(analog_sensor->basic_cfg) : NULL;
  esp_zb_identify_cluster_cfg_t *identify_cfg = analog_sensor ? &(analog_sensor->identify_cfg) : NULL;
  esp_zb_analog_value_cluster_cfg_t *analog_value_cfg = analog_sensor ? &(analog_sensor->analog_value_cfg) : NULL;
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(cluster_list, esp_zb_basic_cluster_create(basic_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(identify_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_analog_value_cluster(
    cluster_list, esp_zb_analog_value_cluster_create(analog_value_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE
  );
  return cluster_list;
}

ZigbeeAnalogSensor::ZigbeeAnalogSensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID;

  //Create custom analog sensor configuration
  zigbee_analog_sensor_cfg_t analog_sensor_cfg = ZIGBEE_DEFAULT_ANALOG_SENSOR_CONFIG();
  _cluster_list = zigbee_analog_sensor_clusters_create(&analog_sensor_cfg);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};
}

void ZigbeeAnalogSensor::setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta) {
  if (delta > 0) {
    log_e("Delta reporting is currently not supported by the sensor");
  }
  esp_zb_zcl_reporting_info_t reporting_info;
  memset(&reporting_info, 0, sizeof(esp_zb_zcl_reporting_info_t));
  reporting_info.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV;
  reporting_info.ep = _endpoint;
  reporting_info.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ANALOG_VALUE;
  reporting_info.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
  reporting_info.attr_id = ESP_ZB_ZCL_ATTR_ANALOG_VALUE_PRESENT_VALUE_ID;
  reporting_info.u.send_info.min_interval = min_interval;
  reporting_info.u.send_info.max_interval = max_interval;
  reporting_info.u.send_info.def_min_interval = min_interval;
  reporting_info.u.send_info.def_max_interval = max_interval;
  reporting_info.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  reporting_info.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC, esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_update_reporting_info(&reporting_info);
  esp_zb_lock_release();

}

void ZigbeeAnalogSensor::setAnalog(float analog) {
  float zb_analog = analog;
  log_v("Updating sensor value...");
  /* Update sensor measured value */
  log_d("Setting value to %0.1f", zb_analog);
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_VALUE, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ANALOG_VALUE_PRESENT_VALUE_ID,
    &zb_analog, false
  );
  esp_zb_lock_release();
}

void ZigbeeAnalogSensor::report() {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_ANALOG_VALUE_PRESENT_VALUE_ID;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_ANALOG_VALUE;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  log_v("Analog report sent");
}

#endif  //SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED
