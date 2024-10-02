#include "ZigbeeLight.h"
#if SOC_IEEE802154_SUPPORTED

ZigbeeLight::ZigbeeLight(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID;

  esp_zb_on_off_light_cfg_t light_cfg = ESP_ZB_DEFAULT_ON_OFF_LIGHT_CONFIG();
  _cluster_list = esp_zb_on_off_light_clusters_create(&light_cfg);  // use esp_zb_zcl_cluster_list_create() instead of esp_zb_on_off_light_clusters_create()
  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID, .app_device_version = 0};
}

//set attribute method -> method overridden in child class
void ZigbeeLight::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
  //check the data and call right method
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
      _current_state = *(bool *)message->attribute.data.value;
      lightChanged();
    } else {
      log_w("Received message ignored. Attribute ID: %d not supported for On/Off Light", message->attribute.id);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %d not supported for On/Off Light", message->info.cluster);
  }
}

void ZigbeeLight::lightChanged() {
  if (_on_light_change) {
    _on_light_change(_current_state);
  } else {
    log_w("No callback function set for light change");
  }
}

#endif  //SOC_IEEE802154_SUPPORTED
