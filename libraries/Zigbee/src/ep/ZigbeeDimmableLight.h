/* Class of Zigbee On/Off Light endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

/**
 * @brief Zigbee HA standard dimmable light device clusters.
 *        Added here as not supported by ESP Zigbee library.
 *
 *
 */
typedef struct zigbee_dimmable_light_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;       /*!<  Basic cluster configuration, @ref esp_zb_basic_cluster_cfg_s */
  esp_zb_identify_cluster_cfg_t identify_cfg; /*!<  Identify cluster configuration, @ref esp_zb_identify_cluster_cfg_s */
  esp_zb_groups_cluster_cfg_t groups_cfg;     /*!<  Groups cluster configuration, @ref esp_zb_groups_cluster_cfg_s */
  esp_zb_scenes_cluster_cfg_t scenes_cfg;     /*!<  Scenes cluster configuration, @ref esp_zb_scenes_cluster_cfg_s */
  esp_zb_on_off_cluster_cfg_t on_off_cfg;     /*!<  On off cluster configuration, @ref esp_zb_on_off_cluster_cfg_s */
  esp_zb_level_cluster_cfg_t level_cfg;       /*!<  Level cluster configuration, @ref esp_zb_level_cluster_cfg_s */
} zigbee_dimmable_light_cfg_t;

/**
 * @brief Zigbee HA standard dimmable light device default config value.
 *        Added here as not supported by ESP Zigbee library.
 *
 */
// clang-format off
#define ZIGBEE_DEFAULT_DIMMABLE_LIGHT_CONFIG()                                  \
  {                                                                             \
    .basic_cfg =                                                                \
      {                                                                         \
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,              \
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,            \
      },                                                                        \
    .identify_cfg =                                                             \
      {                                                                         \
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,       \
      },                                                                        \
    .groups_cfg =                                                               \
      {                                                                         \
        .groups_name_support_id = ESP_ZB_ZCL_GROUPS_NAME_SUPPORT_DEFAULT_VALUE, \
      },                                                                        \
    .scenes_cfg =                                                               \
      {                                                                         \
        .scenes_count = ESP_ZB_ZCL_SCENES_SCENE_COUNT_DEFAULT_VALUE,            \
        .current_scene = ESP_ZB_ZCL_SCENES_CURRENT_SCENE_DEFAULT_VALUE,         \
        .current_group = ESP_ZB_ZCL_SCENES_CURRENT_GROUP_DEFAULT_VALUE,         \
        .scene_valid = ESP_ZB_ZCL_SCENES_SCENE_VALID_DEFAULT_VALUE,             \
        .name_support = ESP_ZB_ZCL_SCENES_NAME_SUPPORT_DEFAULT_VALUE,           \
      },                                                                        \
    .on_off_cfg =                                                               \
      {                                                                         \
        .on_off = ESP_ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE,                       \
      },                                                                        \
    .level_cfg =                                                                \
      {                                                                         \
        .current_level = ESP_ZB_ZCL_LEVEL_CONTROL_CURRENT_LEVEL_DEFAULT_VALUE,  \
      },                                                                        \
  }
// clang-format on

class ZigbeeDimmableLight : public ZigbeeEP {
public:
  ZigbeeDimmableLight(uint8_t endpoint);
  ~ZigbeeDimmableLight() {}

  void onLightChange(void (*callback)(bool, uint8_t)) {
    _on_light_change = callback;
  }
  void restoreLight() {
    lightChanged();
  }

  bool setLightState(bool state);
  bool setLightLevel(uint8_t level);
  bool setLight(bool state, uint8_t level);

  bool getLightState() {
    return _current_state;
  }
  uint8_t getLightLevel() {
    return _current_level;
  }

private:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;
  void lightChanged();
  // callback function to be called on light change (State, Level)
  void (*_on_light_change)(bool, uint8_t);

  /**
   * @brief  Create a standard HA dimmable light cluster list.
   *        Added here as not supported by ESP Zigbee library.
   *
   * @note This contains basic, identify, groups, scenes, on-off, level, as server side.
   * @param[in] light_cfg  Configuration parameters for this cluster lists defined by @ref zigbee_dimmable_light_cfg_t
   *
   * @return Pointer to cluster list  @ref esp_zb_cluster_list_s
   *
   */
  esp_zb_cluster_list_t *zigbee_dimmable_light_clusters_create(zigbee_dimmable_light_cfg_t *light_cfg);

  bool _current_state;
  uint8_t _current_level;
};

#endif  // SOC_IEEE802154_SUPPORTED
