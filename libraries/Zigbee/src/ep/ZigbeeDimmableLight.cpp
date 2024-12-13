
#include "ZigbeeDimmableLight.h"
#if SOC_IEEE802154_SUPPORTED

#include "esp_zigbee_cluster.h"

ZigbeeDimmableLight::ZigbeeDimmableLight(uint8_t endpoint) : ZigbeeEP(endpoint)
{
  _device_id = ESP_ZB_HA_DIMMABLE_LIGHT_DEVICE_ID;

  esp_zb_dimmable_light_cfg_t light_cfg = ESP_ZB_DEFAULT_DIMMABLE_LIGHT_CONFIG();
  _cluster_list = esp_zb_dimmable_light_clusters_create(&light_cfg);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_DIMMABLE_LIGHT_DEVICE_ID, .app_device_version = 0};

  // set default values
  _current_state = false;
  _current_level = 255;
}

// set attribute method -> method overridden in child class
void ZigbeeDimmableLight::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message)
{
  // check the data and call right method
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ON_OFF)
  {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_ON_OFF_ON_OFF_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_BOOL)
    {
      if (_current_state != *(bool *)message->attribute.data.value)
      {
        _current_state = *(bool *)message->attribute.data.value;
        lightChanged();
      }
      return;
    }
    else
    {
      log_w("Received message ignored. Attribute ID: %d not supported for On/Off Light", message->attribute.id);
    }
  }
  else if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL)
  {
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8)
    {
      if (_current_level != *(uint8_t *)message->attribute.data.value)
      {
        _current_level = *(uint8_t *)message->attribute.data.value;
        lightChanged();
      }
      return;
    }
    else
    {
      log_w("Received message ignored. Attribute ID: %d not supported for Level Control", message->attribute.id);
      // TODO: implement more attributes -> includes/zcl/esp_zigbee_zcl_level.h
    }
  }
  else
  {
    log_w("Received message ignored. Cluster ID: %d not supported for Color dimmable Light", message->info.cluster);
  }
}

void ZigbeeDimmableLight::lightChanged()
{
  if (_on_light_change)
  {
    _on_light_change(_current_state, _current_level);
  }
}

esp_zb_cluster_list_t *ZigbeeDimmableLight::esp_zb_dimmable_light_clusters_create(esp_zb_dimmable_light_cfg_t *light_cfg)
{
  esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_basic_cluster_create(&light_cfg->basic_cfg);
  esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_identify_cluster_create(&light_cfg->identify_cfg);
  esp_zb_attribute_list_t *esp_zb_groups_cluster = esp_zb_groups_cluster_create(&light_cfg->groups_cfg);
  esp_zb_attribute_list_t *esp_zb_scenes_cluster = esp_zb_scenes_cluster_create(&light_cfg->scenes_cfg);
  esp_zb_attribute_list_t *esp_zb_on_off_cluster = esp_zb_on_off_cluster_create(&light_cfg->on_off_cfg);
  esp_zb_attribute_list_t *esp_zb_level_cluster = esp_zb_level_cluster_create(&light_cfg->level_cfg);

  // ------------------------------ Create cluster list ------------------------------
  esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_groups_cluster(esp_zb_cluster_list, esp_zb_groups_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_scenes_cluster(esp_zb_cluster_list, esp_zb_scenes_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_on_off_cluster(esp_zb_cluster_list, esp_zb_on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_level_cluster(esp_zb_cluster_list, esp_zb_level_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  return esp_zb_cluster_list;
}

#endif // SOC_IEEE802154_SUPPORTED
