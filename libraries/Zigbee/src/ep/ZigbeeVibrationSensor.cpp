#include "ZigbeeVibrationSensor.h"
#if CONFIG_ZB_ENABLED

esp_zb_cluster_list_t *zigbee_vibration_sensor_clusters_create(zigbee_vibration_sensor_cfg_t *vibration_sensor) {
  esp_zb_basic_cluster_cfg_t *basic_cfg = vibration_sensor ? &(vibration_sensor->basic_cfg) : NULL;
  esp_zb_identify_cluster_cfg_t *identify_cfg = vibration_sensor ? &(vibration_sensor->identify_cfg) : NULL;
  esp_zb_ias_zone_cluster_cfg_t *ias_zone_cfg = vibration_sensor ? &(vibration_sensor->ias_zone_cfg) : NULL;
  esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(cluster_list, esp_zb_basic_cluster_create(basic_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(identify_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_ias_zone_cluster(cluster_list, esp_zb_ias_zone_cluster_create(ias_zone_cfg), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  return cluster_list;
}

ZigbeeVibrationSensor::ZigbeeVibrationSensor(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_IAS_ZONE_ID;
  _zone_status = 0;
  _zone_id = 0xff;
  _ias_cie_endpoint = 1;

  //Create custom vibration sensor configuration
  zigbee_vibration_sensor_cfg_t vibration_sensor_cfg = ZIGBEE_DEFAULT_VIBRATION_SENSOR_CONFIG();
  _cluster_list = zigbee_vibration_sensor_clusters_create(&vibration_sensor_cfg);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_IAS_ZONE_ID, .app_device_version = 0};
}

void ZigbeeVibrationSensor::setIASClientEndpoint(uint8_t ep_number) {
  _ias_cie_endpoint = ep_number;
}

bool ZigbeeVibrationSensor::setVibration(bool sensed) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  log_v("Setting Vibration sensor to %s", sensed ? "sensed" : "not sensed");
  uint8_t vibration = (uint8_t)sensed;
  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_IAS_ZONE_ZONESTATUS_ID, &vibration, false
  );
  esp_zb_lock_release();
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set vibration status: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  _zone_status = vibration;
  report();
  return true;
}

void ZigbeeVibrationSensor::report() {
  /* Send IAS Zone status changed notification command */

  esp_zb_zcl_ias_zone_status_change_notif_cmd_t status_change_notif_cmd;
  status_change_notif_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
  status_change_notif_cmd.zcl_basic_cmd.src_endpoint = _endpoint;
  status_change_notif_cmd.zcl_basic_cmd.dst_endpoint = _ias_cie_endpoint;  //default is 1
  memcpy(status_change_notif_cmd.zcl_basic_cmd.dst_addr_u.addr_long, _ias_cie_addr, sizeof(esp_zb_ieee_addr_t));
  status_change_notif_cmd.zone_status = _zone_status;
  status_change_notif_cmd.extend_status = 0;
  status_change_notif_cmd.zone_id = _zone_id;
  status_change_notif_cmd.delay = 0;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_ias_zone_status_change_notif_cmd_req(&status_change_notif_cmd);
  esp_zb_lock_release();
  log_v("IAS Zone status changed notification sent");
}

void ZigbeeVibrationSensor::zbIASZoneEnrollResponse(const esp_zb_zcl_ias_zone_enroll_response_message_t *message) {
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE) {
    log_v("IAS Zone Enroll Response: zone id(%d), status(%d)", message->zone_id, message->response_code);
    if (message->response_code == ESP_ZB_ZCL_IAS_ZONE_ENROLL_RESPONSE_CODE_SUCCESS) {
      log_v("IAS Zone Enroll Response: success");
      esp_zb_lock_acquire(portMAX_DELAY);
      memcpy(
        _ias_cie_addr,
        (*(esp_zb_ieee_addr_t *)
            esp_zb_zcl_get_attribute(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_IAS_ZONE, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_IAS_ZONE_IAS_CIE_ADDRESS_ID)
              ->data_p),
        sizeof(esp_zb_ieee_addr_t)
      );
      esp_zb_lock_release();
      _zone_id = message->zone_id;
    }

  } else {
    log_w("Received message ignored. Cluster ID: %d not supported for On/Off Light", message->info.cluster);
  }
}

#endif  // CONFIG_ZB_ENABLED
