#include "ep_template.h"

ZigbeeDevice::ZigbeeDevice(uint8_t endpoint, void (*cb)(const esp_zb_zcl_set_attr_value_message_t *message)) : Zigbee_EP(endpoint, cb) {
    _device_id = ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID; //TODO: Change to correct device ID
    _version = 0;
    
    //TODO: Change to correct config
    esp_zb_on_off_light_cfg_t light_cfg = ESP_ZB_DEFAULT_ON_OFF_LIGHT_CONFIG();
    _cluster_list = esp_zb_on_off_light_clusters_create(&light_cfg);

    _ep_config = {       
        .endpoint = _endpoint,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID, //TODO: Change to correct device ID
        .app_device_version = _version
    };
}

void ZigbeeDevice::find_endpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req) {
    // If device is looking for a endpoint, implement this function + find_cb and bind_cb callbacks
    // for reference check ZigbeeSwitch::find_endpoint, ZigbeeSwitch::find_cb and ZigbeeSwitch::bind_cb
    return;
}