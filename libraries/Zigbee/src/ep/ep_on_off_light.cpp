#include "ep_on_off_light.h"

ZigbeeLight::ZigbeeLight(uint8_t endpoint) : Zigbee_EP(endpoint) {
    _device_id = ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID;
    _version = 0;

    esp_zb_on_off_light_cfg_t light_cfg = ESP_ZB_DEFAULT_ON_OFF_LIGHT_CONFIG();
    _cluster_list = esp_zb_on_off_light_clusters_create(&light_cfg); // use esp_zb_zcl_cluster_list_create() instead of esp_zb_on_off_light_clusters_create()
    _ep_config = {       
        .endpoint = _endpoint,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID,
        .app_device_version = _version
    };
}

//set attribude method -> methon overriden in child class
void ZigbeeLight::attribute_set(const esp_zb_zcl_set_attr_value_message_t *message) {
    //check the data and call right method
    if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
        if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
            setOnOff(*(bool *)message->attribute.data.value);
        }
        else if(message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_GLOBAL_SCENE_CONTROL && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
            sceneControl(*(bool *)message->attribute.data.value);
        }
        else if(message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_TIME && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
            setOnOffTime(*(uint16_t *)message->attribute.data.value);
        }
        else if(message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_OFF_WAIT_TIME && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
            setOffWaitTime(*(uint16_t *)message->attribute.data.value);
        }
        // else if(message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_START_UP_ON_OFF && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
        //     //TODO: more info needed, not implemented for now
        // }
        else {
            log_w("Recieved message ignored. Attribute ID: %d not supported for On/Off Light", message->attribute.id);
        }
    }
    else {
        log_w("Recieved message ignored. Cluster ID: %d not supported for On/Off Light", message->info.cluster);
    }
}

//default method to set on/off
void ZigbeeLight::setOnOff(bool value) {
    //set on/off
    log_v("Function not overwritten, set on/off: %d", value);
}

void ZigbeeLight::sceneControl(bool value) {
    //set scene control
    log_v("Function not overwritten, set scene control: %d", value);
}

void ZigbeeLight::setOnOffTime(uint16_t value) {
    //set on/off time
    log_v("Function not overwritten, set on/off time: %d", value);
}

void ZigbeeLight::setOffWaitTime(uint16_t value) {
    //set off wait time
    log_v("Function not overwritten, set off wait time: %d", value);
}
