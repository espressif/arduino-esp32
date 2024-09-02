#include "ZigbeeColorDimmableLight.h"

ZigbeeColorDimmableLight::ZigbeeColorDimmableLight(uint8_t endpoint) : ZigbeeEP(endpoint) {
    _device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID;

    esp_zb_color_dimmable_light_cfg_t light_cfg = ESP_ZB_DEFAULT_COLOR_DIMMABLE_LIGHT_CONFIG();
    _cluster_list = esp_zb_color_dimmable_light_clusters_create(&light_cfg);
    _ep_config = {       
        .endpoint = _endpoint,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID,
        .app_device_version = 0
    };
}

uint8_t ZigbeeColorDimmableLight::getCurrentLevel(){
    return (*(uint8_t *)esp_zb_zcl_get_attribute(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID)->data_p);
}

uint16_t ZigbeeColorDimmableLight::getCurrentColorX(){
    return (*(uint16_t *)esp_zb_zcl_get_attribute(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID)->data_p);
}

uint16_t ZigbeeColorDimmableLight::getCurrentColorY(){
    return (*(uint16_t *)esp_zb_zcl_get_attribute(_endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID)->data_p);
}

void ZigbeeColorDimmableLight::calculateRGB( uint16_t x, uint16_t y, uint8_t &red, uint8_t &green, uint8_t &blue){
    float r, g, b, color_x, color_y;
    color_x = (float)x / 65535;
    color_y = (float)y / 65535;
    
    float color_X = color_x / color_y;
    float color_Z = (1 - color_x - color_y) / color_y;

    XYZ_to_RGB(color_X, 1, color_Z, r, g, b);

    red = (uint8_t)(r * (float)255);
    green = (uint8_t)(g * (float)255);
    blue = (uint8_t)(b * (float)255);
}

//set attribude method -> methon overriden in child class
void ZigbeeColorDimmableLight::attribute_set(const esp_zb_zcl_set_attr_value_message_t *message) {
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
    else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL) {
        if (message->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
            setLevel(*(uint8_t *)message->attribute.data.value);
        }
        else {
            log_w("Recieved message ignored. Attribute ID: %d not supported for Level Control", message->attribute.id);
            //TODO: implement more attributes -> includes/zcl/esp_zigbee_zcl_level.h
        }
    }
    else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL) {
        if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
            log_v("Light color x changes to 0x%x", *(uint16_t *)message->attribute.data.value);
            uint16_t light_color_x = (*(uint16_t *)message->attribute.data.value);
            uint16_t light_color_y = getCurrentColorY();
            //calculate RGB from XY and call setColor()
            uint8_t red, green, blue;
            calculateRGB(light_color_x, light_color_y, red, green, blue);
            setColor(red, green, blue);

        }
        else if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
            log_v("Light color x changes to 0x%x", *(uint16_t *)message->attribute.data.value);
            uint16_t light_color_x = getCurrentColorX();
            uint16_t light_color_y = (*(uint16_t *)message->attribute.data.value);
            //calculate RGB from XY and call setColor()
            uint8_t red, green, blue;
            calculateRGB(light_color_x, light_color_y, red, green, blue);
            setColor(red, green, blue);
        }
        else if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_SATURATION_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
            setColorSaturation(*(uint8_t *)message->attribute.data.value);
        }
        else if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_HUE_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
            setColorHue(*(uint8_t *)message->attribute.data.value);
        }
        else {
            log_w("Recieved message ignored. Attribute ID: %d not supported for Color Control", message->attribute.id);
            //TODO: implement more attributes -> includes/zcl/esp_zigbee_zcl_color_control.h
        }
    }
    else {
        log_w("Recieved message ignored. Cluster ID: %d not supported for Color dimmable Light", message->info.cluster);
    }
}

//default method to set on/off
void ZigbeeColorDimmableLight::setOnOff(bool value) {
    //set on/off
    log_v("Function not overwritten, set on/off: %d", value);
}

void ZigbeeColorDimmableLight::sceneControl(bool value) {
    //set scene control
    log_v("Function not overwritten, set scene control: %d", value);
}

void ZigbeeColorDimmableLight::setOnOffTime(uint16_t value) {
    //set on/off time
    log_v("Function not overwritten, set on/off time: %d", value);
}

void ZigbeeColorDimmableLight::setOffWaitTime(uint16_t value) {
    //set off wait time
    log_v("Function not overwritten, set off wait time: %d", value);
}

void ZigbeeColorDimmableLight::setLevel(uint8_t value) {
    //set level
    log_v("Function not overwritten, set level: %d", value);
}

void ZigbeeColorDimmableLight::setColor(uint8_t r, uint8_t g, uint8_t b) {
    //set color
    log_v("Function not overwritten, set color: r: %d, g: %d, b: %d", r, g, b);
}

void ZigbeeColorDimmableLight::setColorSaturation(uint8_t value) {
    //set color saturation
    log_v("Function not overwritten, set color saturation: %d", value);
}

void ZigbeeColorDimmableLight::setColorHue(uint8_t value) {
    //set color hue
    log_v("Function not overwritten, set color hue: %d", value);
}