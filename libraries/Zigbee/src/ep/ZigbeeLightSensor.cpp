#include "ZigbeeLightSensor.h"
#if CONFIG_ZB_ENABLED

ZigbeeLightSensor::ZigbeeLightSensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_LIGHT_SENSOR_DEVICE_ID;

  esp_zb_light_sensor_cfg_t light_sensor_cfg = ESP_ZB_DEFAULT_LIGHT_SENSOR_CONFIG();
  _cluster_list = esp_zb_light_sensor_clusters_create(&light_sensor_cfg);

  _ep_config = {
    .endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_LIGHT_SENSOR_DEVICE_ID, .app_device_version = 0
  };
}

static int16_t zb_int_to_s16(int value) {
  return (int16_t) value;
}

void ZigbeeLightSensor::setMinMaxValue(int min, int max) {
  int16_t zb_min = zb_int_to_s16(min);
  int16_t zb_max = zb_int_to_s16(max);
  esp_zb_attribute_list_t *light_measure_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_update_attr(light_measure_cluster, ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MIN_MEASURED_VALUE_ID, (void *)&zb_min);
  esp_zb_cluster_update_attr(light_measure_cluster, ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MAX_MEASURED_VALUE_ID, (void *)&zb_max);
}

void ZigbeeLightSensor::setTolerance(int tolerance) {
  uint16_t zb_tolerance = (uint16_t) tolerance;
  esp_zb_attribute_list_t *light_measure_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_illuminance_meas_cluster_add_attr(light_measure_cluster, ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_TOLERANCE_ID, (void *)&zb_tolerance);
}

void ZigbeeLightSensor::setReporting(uint16_t min_interval, uint16_t max_interval, int delta) {
  esp_zb_zcl_reporting_info_t reporting_info;
  memset(&reporting_info, 0, sizeof(esp_zb_zcl_reporting_info_t));
  reporting_info.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV;
  reporting_info.ep = _endpoint;
  reporting_info.cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT;
  reporting_info.cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE;
  reporting_info.attr_id = ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MEASURED_VALUE_ID;
  reporting_info.u.send_info.min_interval = min_interval;
  reporting_info.u.send_info.max_interval = max_interval;
  reporting_info.u.send_info.def_min_interval = min_interval;
  reporting_info.u.send_info.def_max_interval = max_interval;
  reporting_info.u.send_info.delta.u16 = (uint16_t) delta;
  reporting_info.dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID;
  reporting_info.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_update_reporting_info(&reporting_info);
  esp_zb_lock_release();
}

void ZigbeeLightSensor::setIlluminance(int illuminanceValue) {
  int16_t zb_illuminanceValue = zb_int_to_s16(illuminanceValue);
  log_v("Updating Illuminance...");
  /* Update light sensor measured illuminance */
  log_d("Setting Illuminance to %d", zb_illuminanceValue);
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MEASURED_VALUE_ID, &zb_illuminanceValue, false
  );
  esp_zb_lock_release();
}

void ZigbeeLightSensor::reportIlluminance() {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_ILLUMINANCE_MEASUREMENT_MEASURED_VALUE_ID;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_ILLUMINANCE_MEASUREMENT;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  report_attr_cmd.manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  log_v("Illuminance report sent");
}

#endif  // CONFIG_ZB_ENABLED
