#include "ZigbeeCarbonDioxideSensor.h"
#if CONFIG_ZB_ENABLED

esp_zb_cluster_list_t *zigbee_carbon_dioxide_sensor_clusters_create(zigbee_carbon_dioxide_sensor_cfg_t *carbon_dioxide_sensor) {
  esp_zb_basic_cluster_cfg_t *basic_cfg = carbon_dioxide_sensor ? &(carbon_dioxide_sensor->basic_cfg) : NULL;
  esp_zb_identify_cluster_cfg_t *identify_cfg = carbon_dioxide_sensor ? &(carbon_dioxide_sensor->identify_cfg) : NULL;
  esp_zb_carbon_dioxide_measurement_cluster_cfg_t *carbon_dioxide_meas_cfg = carbon_dioxide_sensor ? &(carbon_dioxide_sensor->carbon_dioxide_meas_cfg) : NULL;
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(cluster_list, esp_zb_basic_cluster_create(basic_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(identify_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_carbon_dioxide_measurement_cluster(
    cluster_list, esp_zb_carbon_dioxide_measurement_cluster_create(carbon_dioxide_meas_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE
  );
  return cluster_list;
}

ZigbeeCarbonDioxideSensor::ZigbeeCarbonDioxideSensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID;

  //Create custom pressure sensor configuration
  zigbee_carbon_dioxide_sensor_cfg_t carbon_dioxide_sensor_cfg = ZIGBEE_DEFAULT_CARBON_DIOXIDE_SENSOR_CONFIG();
  _cluster_list = zigbee_carbon_dioxide_sensor_clusters_create(&carbon_dioxide_sensor_cfg);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};
}

bool ZigbeeCarbonDioxideSensor::setMinMaxValue(float min, float max) {
  float zb_min = min / 1000000.0f;
  float zb_max = max / 1000000.0f;
  esp_zb_attribute_list_t *carbon_dioxide_measure_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_err_t ret = esp_zb_cluster_update_attr(carbon_dioxide_measure_cluster, ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MIN_MEASURED_VALUE_ID, (void *)&zb_min);
  if (ret != ESP_OK) {
    log_e("Failed to set min value: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  ret = esp_zb_cluster_update_attr(carbon_dioxide_measure_cluster, ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MAX_MEASURED_VALUE_ID, (void *)&zb_max);
  if (ret != ESP_OK) {
    log_e("Failed to set max value: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeCarbonDioxideSensor::setTolerance(float tolerance) {
  float zb_tolerance = tolerance / 1000000.0f;
  esp_zb_attribute_list_t *carbon_dioxide_measure_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_err_t ret = esp_zb_carbon_dioxide_measurement_cluster_add_attr(
    carbon_dioxide_measure_cluster, ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_TOLERANCE_ID, (void *)&zb_tolerance
  );
  if (ret != ESP_OK) {
    log_e("Failed to set tolerance: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeCarbonDioxideSensor::setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta) {
  esp_zb_zcl_reporting_info_t reporting_info;
  memset(&reporting_info, 0, sizeof(esp_zb_zcl_reporting_info_t));
  reporting_info.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV;
  reporting_info.ep = _endpoint;
  reporting_info.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT;
  reporting_info.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
  reporting_info.attr_id = ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_ID;
  reporting_info.u.send_info.min_interval = min_interval;
  reporting_info.u.send_info.max_interval = max_interval;
  reporting_info.u.send_info.def_min_interval = min_interval;
  reporting_info.u.send_info.def_max_interval = max_interval;
  reporting_info.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  reporting_info.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;
  float delta_f = delta / 1000000.0f;
  memcpy(&reporting_info.u.send_info.delta.s32, &delta_f, sizeof(float));

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_update_reporting_info(&reporting_info);
  esp_zb_lock_release();
  if (ret != ESP_OK) {
    log_e("Failed to set reporting: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeCarbonDioxideSensor::setCarbonDioxide(float carbon_dioxide) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  float zb_carbon_dioxide = carbon_dioxide / 1000000.0f;
  log_v("Updating carbon dioxide sensor value...");
  /* Update carbon dioxide sensor measured value */
  log_d("Setting carbon dioxide to %0.1f", carbon_dioxide);
  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_ID,
    &zb_carbon_dioxide, false
  );
  esp_zb_lock_release();
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set carbon dioxide: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeCarbonDioxideSensor::report() {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_ID;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  report_attr_cmd.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  if (ret != ESP_OK) {
    log_e("Failed to send carbon dioxide report: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  log_v("Carbon dioxide report sent");
  return true;
}

#endif  // CONFIG_ZB_ENABLED
