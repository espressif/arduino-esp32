#include "ZigbeeOccupancySensor.h"
#if SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED

esp_zb_cluster_list_t *zigbee_occupancy_sensor_clusters_create(zigbee_occupancy_sensor_cfg_t *occupancy_sensor) {
  esp_zb_basic_cluster_cfg_t *basic_cfg = occupancy_sensor ? &(occupancy_sensor->basic_cfg) : NULL;
  esp_zb_identify_cluster_cfg_t *identify_cfg = occupancy_sensor ? &(occupancy_sensor->identify_cfg) : NULL;
  esp_zb_occupancy_sensing_cluster_cfg_t *occupancy_meas_cfg = occupancy_sensor ? &(occupancy_sensor->occupancy_meas_cfg) : NULL;
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(cluster_list, esp_zb_basic_cluster_create(basic_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(identify_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_occupancy_sensing_cluster(cluster_list, esp_zb_occupancy_sensing_cluster_create(occupancy_meas_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  return cluster_list;
}

ZigbeeOccupancySensor::ZigbeeOccupancySensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID;

  //Create custom occupancy sensor configuration
  zigbee_occupancy_sensor_cfg_t occupancy_sensor_cfg = ZIGBEE_DEFAULT_OCCUPANCY_SENSOR_CONFIG();
  _cluster_list = zigbee_occupancy_sensor_clusters_create(&occupancy_sensor_cfg);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID, .app_device_version = 0};
}

void ZigbeeOccupancySensor::setSensorType(uint8_t sensor_type) {
  uint8_t sensor_type_bitmap = 1 << sensor_type;
  esp_zb_attribute_list_t *occupancy_sens_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_update_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_ID, (void *)&sensor_type);
  esp_zb_cluster_update_attr(occupancy_sens_cluster, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_SENSOR_TYPE_BITMAP_ID, (void *)&sensor_type_bitmap);
}

void ZigbeeOccupancySensor::setOccupancy(bool occupied) {
  log_v("Updating occupancy sensor value...");
  /* Update occupancy sensor value */
  log_d("Setting occupancy to %d", occupied);
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID, &occupied, false
  );
  esp_zb_lock_release();
}

void ZigbeeOccupancySensor::report() {
  /* Send report attributes command */
  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_OCCUPANCY_SENSING_OCCUPANCY_ID;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_OCCUPANCY_SENSING;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  esp_zb_lock_release();
  log_v("Occupancy report sent");
}

#endif  //SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED
