#include "ZigbeePressureSensor.h"
#if CONFIG_ZB_ENABLED

esp_zb_cluster_list_t *zigbee_pressure_sensor_clusters_create(zigbee_pressure_sensor_cfg_t *pressure_sensor) {
  esp_zb_basic_cluster_cfg_t *basic_cfg = pressure_sensor ? &(pressure_sensor->basic_cfg) : NULL;
  esp_zb_identify_cluster_cfg_t *identify_cfg = pressure_sensor ? &(pressure_sensor->identify_cfg) : NULL;
  esp_zb_pressure_meas_cluster_cfg_t *pressure_meas_cfg = pressure_sensor ? &(pressure_sensor->pressure_meas_cfg) : NULL;
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(cluster_list, esp_zb_basic_cluster_create(basic_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(identify_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_pressure_meas_cluster(cluster_list, esp_zb_pressure_meas_cluster_create(pressure_meas_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  return cluster_list;
}

ZigbeePressureSensor::ZigbeePressureSensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID;

  //Create custom pressure sensor configuration
  zigbee_pressure_sensor_cfg_t presssure_sensor_cfg = ZIGBEE_DEFAULT_PRESSURE_SENSOR_CONFIG();
  _cluster_list = zigbee_pressure_sensor_clusters_create(&presssure_sensor_cfg);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};
}

bool ZigbeePressureSensor::setMinMaxValue(int16_t min, int16_t max) {
  esp_zb_attribute_list_t *pressure_measure_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_err_t ret = esp_zb_cluster_update_attr(pressure_measure_cluster, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_MIN_VALUE_ID, (void *)&min);
  if (ret != ESP_OK) {
    log_e("Failed to set min value: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  ret = esp_zb_cluster_update_attr(pressure_measure_cluster, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_MAX_VALUE_ID, (void *)&max);
  if (ret != ESP_OK) {
    log_e("Failed to set max value: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeePressureSensor::setTolerance(uint16_t tolerance) {
  esp_zb_attribute_list_t *pressure_measure_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_err_t ret = esp_zb_pressure_meas_cluster_add_attr(pressure_measure_cluster, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_TOLERANCE_ID, (void *)&tolerance);
  if (ret != ESP_OK) {
    log_e("Failed to set tolerance: 0x%x: %s", ret, esp_err_to_name(ret));
  }
  return ret == ESP_OK;
}

bool ZigbeePressureSensor::setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta) {
  esp_zb_zcl_reporting_info_t reporting_info;
  memset(&reporting_info, 0, sizeof(esp_zb_zcl_reporting_info_t));
  reporting_info.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV;
  reporting_info.ep = _endpoint;
  reporting_info.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT;
  reporting_info.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
  reporting_info.attr_id = ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_VALUE_ID;
  reporting_info.u.send_info.min_interval = min_interval;
  reporting_info.u.send_info.max_interval = max_interval;
  reporting_info.u.send_info.def_min_interval = min_interval;
  reporting_info.u.send_info.def_max_interval = max_interval;
  reporting_info.u.send_info.delta.u16 = delta;  // x hPa
  reporting_info.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  reporting_info.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_update_reporting_info(&reporting_info);
  esp_zb_lock_release();
  if (ret != ESP_OK) {
    log_e("Failed to set reporting: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeePressureSensor::setPressure(int16_t pressure) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  log_v("Updating pressure sensor value...");
  /* Update pressure sensor measured value */
  log_d("Setting pressure to %d hPa", pressure);
  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_VALUE_ID, &pressure, false
  );
  esp_zb_lock_release();
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set pressure: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeePressureSensor::report() {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_VALUE_ID;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  report_attr_cmd.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t ret = esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  if (ret != ESP_OK) {
    log_e("Failed to send pressure report: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  log_v("Pressure report sent");
  return true;
}

#endif  // CONFIG_ZB_ENABLED
