#include "ep_temperature_sensor.h"

ZigbeeTempSensor::ZigbeeTempSensor(uint8_t endpoint) : Zigbee_EP(endpoint) {
    _device_id = ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID;
    _version = 0;

    esp_zb_temperature_sensor_cfg_t temp_sensor_cfg = ESP_ZB_DEFAULT_TEMPERATURE_SENSOR_CONFIG();
    _cluster_list = esp_zb_temperature_sensor_clusters_create(&temp_sensor_cfg);
    _ep_config = {       
        .endpoint = _endpoint,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID,
        .app_device_version = _version
    };
}

void ZigbeeTempSensor::find_endpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req) {
    //empty function, not used for this type of device
    return;
}