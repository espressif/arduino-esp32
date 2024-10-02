#include "ZigbeeColorDimmableLight.h"
#if SOC_IEEE802154_SUPPORTED

ZigbeeColorDimmableLight::ZigbeeColorDimmableLight(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID;

  esp_zb_color_dimmable_light_cfg_t light_cfg = ESP_ZB_DEFAULT_COLOR_DIMMABLE_LIGHT_CONFIG();
  _cluster_list = esp_zb_color_dimmable_light_clusters_create(&light_cfg);
  _ep_config = {
    .endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID, .app_device_version = 0
  };

  //set default values
  _current_state = false;
  _current_level = 255;
  _current_red = 255;
  _current_green = 255;
  _current_blue = 255;
}

uint16_t ZigbeeColorDimmableLight::getCurrentColorX() {
  return (*(uint16_t *)esp_zb_zcl_get_attribute(
             _endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID
  )
             ->data_p);
}

uint16_t ZigbeeColorDimmableLight::getCurrentColorY() {
  return (*(uint16_t *)esp_zb_zcl_get_attribute(
             _endpoint, ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID
  )
             ->data_p);
}

void ZigbeeColorDimmableLight::calculateRGB(uint16_t x, uint16_t y, uint8_t &red, uint8_t &green, uint8_t &blue) {
  float r, g, b, color_x, color_y;
  color_x = (float)x / 65535;
  color_y = (float)y / 65535;

  float color_X = color_x / color_y;
  float color_Z = (1 - color_x - color_y) / color_y;

  XYZ_TO_RGB(color_X, 1, color_Z, r, g, b);

  red = (uint8_t)(r * (float)255);
  green = (uint8_t)(g * (float)255);
  blue = (uint8_t)(b * (float)255);
}

//set attribute method -> method overridden in child class
void ZigbeeColorDimmableLight::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
  //check the data and call right method
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
      if (_current_state != *(bool *)message->attribute.data.value) {
        _current_state = *(bool *)message->attribute.data.value;
        lightChanged();
      }
      return;
    } else {
      log_w("Received message ignored. Attribute ID: %d not supported for On/Off Light", message->attribute.id);
    }
  } else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
      if (_current_level != *(uint8_t *)message->attribute.data.value) {
        _current_level = *(uint8_t *)message->attribute.data.value;
        lightChanged();
      }
      return;
    } else {
      log_w("Received message ignored. Attribute ID: %d not supported for Level Control", message->attribute.id);
      //TODO: implement more attributes -> includes/zcl/esp_zigbee_zcl_level.h
    }
  } else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_COLOR_CONTROL) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_X_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
      uint16_t light_color_x = (*(uint16_t *)message->attribute.data.value);
      uint16_t light_color_y = getCurrentColorY();
      //calculate RGB from XY and call setColor()
      uint8_t red, green, blue;
      calculateRGB(light_color_x, light_color_y, red, green, blue);
      _current_blue = blue;
      _current_green = green;
      _current_red = red;
      lightChanged();
      return;

    } else if (message->attribute.id == ESP_ZB_ZCL_ATTR_COLOR_CONTROL_CURRENT_Y_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U16) {
      uint16_t light_color_x = getCurrentColorX();
      uint16_t light_color_y = (*(uint16_t *)message->attribute.data.value);
      //calculate RGB from XY and call setColor()
      uint8_t red, green, blue;
      calculateRGB(light_color_x, light_color_y, red, green, blue);
      _current_blue = blue;
      _current_green = green;
      _current_red = red;
      lightChanged();
      return;
    } else {
      log_w("Received message ignored. Attribute ID: %d not supported for Color Control", message->attribute.id);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %d not supported for Color dimmable Light", message->info.cluster);
  }
}

void ZigbeeColorDimmableLight::lightChanged() {
  if (_on_light_change) {
    _on_light_change(_current_state, _current_red, _current_green, _current_blue, _current_level);
  }
}

#endif  //SOC_IEEE802154_SUPPORTED
