#include "ZigbeeCarbonDioxideSensor.h"
#if SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED

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
  esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  return cluster_list;
}

ZigbeeCarbonDioxideSensor::ZigbeeCarbonDioxideSensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID;

  //Create custom pressure sensor configuration
  zigbee_carbon_dioxide_sensor_cfg_t carbon_dioxide_sensor_cfg = ZIGBEE_DEFAULT_CARBON_DIOXIDE_SENSOR_CONFIG();
  _cluster_list = zigbee_carbon_dioxide_sensor_clusters_create(&carbon_dioxide_sensor_cfg);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};
}

void ZigbeeCarbonDioxideSensor::setMinMaxValue(float min, float max) {
  float zb_min = min / 1000000.0f;
  float zb_max = max / 1000000.0f;
  esp_zb_attribute_list_t *carbon_dioxide_measure_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_update_attr(carbon_dioxide_measure_cluster, ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MIN_MEASURED_VALUE_ID, (void *)&zb_min);
  esp_zb_cluster_update_attr(carbon_dioxide_measure_cluster, ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MAX_MEASURED_VALUE_ID, (void *)&zb_max);
}

void ZigbeeCarbonDioxideSensor::setTolerance(float tolerance) {
  float zb_tolerance = tolerance / 1000000.0f;
  esp_zb_attribute_list_t *carbon_dioxide_measure_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_temperature_meas_cluster_add_attr(carbon_dioxide_measure_cluster, ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_TOLERANCE_ID, (void *)&zb_tolerance);
}

void ZigbeeCarbonDioxideSensor::setReporting(uint16_t min_interval, uint16_t max_interval, uint16_t delta) {
  if (delta > 0) {
    log_e("Delta reporting is currently not supported by the carbon dioxide sensor");
  }
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
  reporting_info.u.send_info.delta.u16 = delta;
  reporting_info.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  reporting_info.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_update_reporting_info(&reporting_info);
  esp_zb_lock_release();
}

void ZigbeeCarbonDioxideSensor::setCarbonDioxide(float carbon_dioxide) {
  float zb_carbon_dioxide = carbon_dioxide / 1000000.0f;
  log_v("Updating carbon dioxide sensor value...");
  /* Update carbon dioxide sensor measured value */
  log_d("Setting carbon dioxide to %0.1f", carbon_dioxide);
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_ID,
    &zb_carbon_dioxide, false
  );
  esp_zb_lock_release();
}

void ZigbeeCarbonDioxideSensor::report() {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_CARBON_DIOXIDE_MEASUREMENT_MEASURED_VALUE_ID;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_CARBON_DIOXIDE_MEASUREMENT;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  log_v("Carbon dioxide report sent");
}

#endif  //SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED
