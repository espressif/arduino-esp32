#include "ZigbeePowerOutlet.h"
#if CONFIG_ZB_ENABLED

ZigbeePowerOutlet::ZigbeePowerOutlet(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_MAINS_POWER_OUTLET_DEVICE_ID;

  esp_zb_mains_power_outlet_cfg_t outlet_cfg = ESP_ZB_DEFAULT_MAINS_POWER_OUTLET_CONFIG();
  _cluster_list = esp_zb_mains_power_outlet_clusters_create(&outlet_cfg);
  _ep_config = {
    .endpoint = endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_MAINS_POWER_OUTLET_DEVICE_ID, .app_device_version = 0
  };
  log_v("Outlet endpoint created %d", _endpoint);
}

//set attribute method -> method overridden in child class
void ZigbeePowerOutlet::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
  //check the data and call right method
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF) {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL) {
      _current_state = *(bool *)message->attribute.data.value;
      stateChanged();
    } else {
      log_w("Received message ignored. Attribute ID: %d not supported for On/Off Outlet", message->attribute.id);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %d not supported for On/Off Outlet", message->info.cluster);
  }
}

void ZigbeePowerOutlet::stateChanged() {
  if (_on_state_change) {
    _on_state_change(_current_state);
  } else {
    log_w("No callback function set for outlet change");
  }
}

bool ZigbeePowerOutlet::setState(bool state) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  _current_state = state;
  stateChanged();

  log_v("Updating on/off outlet state to %d", state);
  /* Update on/off outlet state */
  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_ON_OFF, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID, &_current_state, false
  );
  esp_zb_lock_release();

  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set outlet state: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

#endif  // CONFIG_ZB_ENABLED
