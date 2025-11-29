// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

typedef struct zigbee_temperature_dimmable_light_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;
  esp_zb_identify_cluster_cfg_t identify_cfg;
  esp_zb_groups_cluster_cfg_t groups_cfg;
  esp_zb_scenes_cluster_cfg_t scenes_cfg;
  esp_zb_on_off_cluster_cfg_t on_off_cfg;
  esp_zb_level_cluster_cfg_t level_cfg;
  esp_zb_color_cluster_cfg_t color_cfg;
} zigbee_temperature_dimmable_light_cfg_t;

#define ZIGBEE_DEFAULT_TEMPERATURE_DIMMABLE_LIGHT_CONFIG()                                                                                                     \
  {                                                                                                                                                            \
    .basic_cfg =                                                                                                                                               \
      {                                                                                                                                                        \
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,                                                                                             \
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,                                                                                           \
      },                                                                                                                                                       \
    .identify_cfg =                                                                                                                                            \
      {                                                                                                                                                        \
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,                                                                                      \
      },                                                                                                                                                       \
    .groups_cfg =                                                                                                                                              \
      {                                                                                                                                                        \
        .groups_name_support_id = ESP_ZB_ZCL_GROUPS_NAME_SUPPORT_DEFAULT_VALUE,                                                                                \
      },                                                                                                                                                       \
    .scenes_cfg =                                                                                                                                              \
      {                                                                                                                                                        \
        .scenes_count = ESP_ZB_ZCL_SCENES_SCENE_COUNT_DEFAULT_VALUE,                                                                                           \
        .current_scene = ESP_ZB_ZCL_SCENES_CURRENT_SCENE_DEFAULT_VALUE,                                                                                        \
        .current_group = ESP_ZB_ZCL_SCENES_CURRENT_GROUP_DEFAULT_VALUE,                                                                                        \
        .scene_valid = ESP_ZB_ZCL_SCENES_SCENE_VALID_DEFAULT_VALUE,                                                                                            \
        .name_support = ESP_ZB_ZCL_SCENES_NAME_SUPPORT_DEFAULT_VALUE,                                                                                          \
      },                                                                                                                                                       \
    .on_off_cfg =                                                                                                                                              \
      {                                                                                                                                                        \
        .on_off = ESP_ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE,                                                                                                      \
      },                                                                                                                                                       \
    .level_cfg =                                                                                                                                               \
      {                                                                                                                                                        \
        .current_level = ESP_ZB_ZCL_LEVEL_CONTROL_CURRENT_LEVEL_DEFAULT_VALUE,                                                                                 \
      },                                                                                                                                                       \
    .color_cfg = {                                                                                                                                             \
      .current_x = ESP_ZB_ZCL_COLOR_CONTROL_CURRENT_X_DEF_VALUE, /*!<  The current value of the normalized chromaticity value x */                             \
      .current_y = ESP_ZB_ZCL_COLOR_CONTROL_CURRENT_Y_DEF_VALUE, /*!<  The current value of the normalized chromaticity value y */                             \
      .color_mode = 0x0002,                                      /*!<  The mode which attribute determines the color of the device */                          \
      .options = ESP_ZB_ZCL_COLOR_CONTROL_OPTIONS_DEFAULT_VALUE, /*!<  The bitmap determines behavior of some cluster commands */                              \
      .enhanced_color_mode =                                                                                                                                   \
        ESP_ZB_ZCL_COLOR_CONTROL_ENHANCED_COLOR_MODE_DEFAULT_VALUE, /*!<  The enhanced-mode which attribute determines the color of the device */              \
      .color_capabilities = 0x0010,                                 /*!<  Specifying the color capabilities of the device support the color control cluster */ \
    },                                                                                                                                                         \
  }

class ZigbeeTemperatureDimmableLight : public ZigbeeEP {
public:
  ZigbeeTemperatureDimmableLight(uint8_t endpoint);
  ~ZigbeeTemperatureDimmableLight() {}

  bool setMinMaxTemperature(uint16_t min_temp, uint16_t max_temp);

  void onLightChange(void (*callback)(bool, uint8_t, uint16_t)) {
    _on_light_change = callback;
  }
  void restoreLight() {
    lightChanged();
  }

  bool setLightState(bool state);
  bool setLightLevel(uint8_t level);
  bool setLightTemperature(uint16_t mireds);
  bool setLight(bool state, uint8_t level, uint16_t mireds);

  bool getLightState() {
    return _current_state;
  }
  uint8_t getLightLevel() {
    return _current_level;
  }
  uint16_t getLightTemperature() {
    return _current_color_temperature;
  }

private:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;
  void lightChanged();
  void (*_on_light_change)(bool, uint8_t, uint16_t);

  bool _current_state;
  uint8_t _current_level;
  uint16_t _current_color_temperature;

  esp_zb_cluster_list_t *zigbee_temperature_dimmable_light_clusters_create(zigbee_temperature_dimmable_light_cfg_t *light_cfg);
};

#endif  // CONFIG_ZB_ENABLED
