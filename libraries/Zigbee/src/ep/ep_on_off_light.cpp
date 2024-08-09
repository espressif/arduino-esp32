#include "ep_on_off_light.h"

ZigbeeLight::ZigbeeLight(uint8_t endpoint) : Zigbee_EP(endpoint) {
    _device_id = ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID;
    _version = 0;

    esp_zb_on_off_light_cfg_t light_cfg = ESP_ZB_DEFAULT_ON_OFF_LIGHT_CONFIG();
    _cluster_list = esp_zb_on_off_light_clusters_create(&light_cfg);
    _ep_config = {       
        .endpoint = _endpoint,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID,
        .app_device_version = _version
    };
}

void ZigbeeLight::find_endpoint(esp_zb_zdo_match_desc_req_param_t *cmd_req) {
    //NOTE: Light is looking for switch?
    return;
}

//set attribude method -> methon overriden in child class
void ZigbeeLight::attribute_set(const esp_zb_zcl_set_attr_value_message_t *message) {
    //check the data and call right method
    if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
        bool light_state = 0;
        if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
            light_state = message->attribute.data.value ? *(bool *)message->attribute.data.value : light_state;
            set_on_off(light_state);
        }
    }
    // if (message->attr_id == ESP_ZB_ON_OFF_LIGHT_ATTR_ON_OFF) {
    //     //call method to set on/off
    //     set_on_off(message->attribute.data[0]);
    // }
    // else if (message->attr_id == ESP_ZB_ON_OFF_LIGHT_ATTR_LEVEL) {
    //     //call method to set level
    // }
    else {
        //unknown attribute
    }
}

//default method to set on/off
void ZigbeeLight::set_on_off(bool value) {
    //set on/off
    log_v("Function not overwritten, set on/off: %d", value);
}