/* Class of Zigbee Window Covering endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

/**
 * @brief Zigbee HA standard window covering device clusters.
 *        Added here as not supported by ESP Zigbee library.
 *
 *
 */
typedef struct zigbee_window_covering_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;                         /*!<  Basic cluster configuration, @ref esp_zb_basic_cluster_cfg_s */
  esp_zb_identify_cluster_cfg_t identify_cfg;                   /*!<  Identify cluster configuration, @ref esp_zb_identify_cluster_cfg_s */
  esp_zb_groups_cluster_cfg_t groups_cfg;                       /*!<  Groups cluster configuration, @ref esp_zb_groups_cluster_cfg_s */
  esp_zb_scenes_cluster_cfg_t scenes_cfg;                       /*!<  Scenes cluster configuration, @ref esp_zb_scenes_cluster_cfg_s */
  esp_zb_window_covering_cluster_cfg_t window_covering_cfg;     /*!<  Window covering cluster configuration, @ref esp_zb_window_covering_cluster_cfg_s */
} zigbee_window_covering_cfg_t;

enum WindowCoveringType {
    ROLLERSHADE = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_ROLLERSHADE,
    ROLLERSHADE_2_MOTOR = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_ROLLERSHADE_2_MOTOR,
    ROLLERSHADE_EXTERIOR = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_ROLLERSHADE_EXTERIOR,
    ROLLERSHADE_EXTERIOR_2_MOTOR = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_ROLLERSHADE_EXTERIOR_2_MOTOR,
    DRAPERY = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_DRAPERY,
    AWNING = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_AWNING,
    SHUTTER = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_SHUTTER,
    BLIND_TILT_ONLY = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_TILT_BLIND_TILT_ONLY,
    BLIND_LIFT_AND_TILT = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_TILT_BLIND_LIFT_AND_TILT,
    PROJECTOR_SCREEN = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_PROJECTOR_SCREEN,
};

/**
 * @brief Zigbee HA standard window covering device default config value.
 *        Added here as not supported by ESP Zigbee library.
 *
 */
// clang-format off
#define ZIGBEE_DEFAULT_WINDOW_COVERING_CONFIG()                                         \
  {                                                                                     \
    .basic_cfg =                                                                        \
      {                                                                                 \
        .zcl_version = ESP_ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE,                      \
        .power_source = ESP_ZB_ZCL_BASIC_POWER_SOURCE_DEFAULT_VALUE,                    \
      },                                                                                \
    .identify_cfg =                                                                     \
      {                                                                                 \
        .identify_time = ESP_ZB_ZCL_IDENTIFY_IDENTIFY_TIME_DEFAULT_VALUE,               \
      },                                                                                \
    .groups_cfg =                                                                       \
      {                                                                                 \
        .groups_name_support_id = ESP_ZB_ZCL_GROUPS_NAME_SUPPORT_DEFAULT_VALUE,         \
      },                                                                                \
    .scenes_cfg =                                                                       \
      {                                                                                 \
        .scenes_count = ESP_ZB_ZCL_SCENES_SCENE_COUNT_DEFAULT_VALUE,                    \
        .current_scene = ESP_ZB_ZCL_SCENES_CURRENT_SCENE_DEFAULT_VALUE,                 \
        .current_group = ESP_ZB_ZCL_SCENES_CURRENT_GROUP_DEFAULT_VALUE,                 \
        .scene_valid = ESP_ZB_ZCL_SCENES_SCENE_VALID_DEFAULT_VALUE,                     \
        .name_support = ESP_ZB_ZCL_SCENES_NAME_SUPPORT_DEFAULT_VALUE,                   \
      },                                                                                \
    .window_covering_cfg =                                                              \
      {                                                                                 \
        .covering_type = ESP_ZB_ZCL_WINDOW_COVERING_WINDOW_COVERING_TYPE_DEFAULT_VALUE, \
        .covering_status = ESP_ZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_DEFAULT_VALUE,      \
        .covering_mode = ESP_ZB_ZCL_WINDOW_COVERING_MODE_DEFAULT_VALUE,                 \
      },                                                                                \
  }
// clang-format on

class ZigbeeWindowCovering : public ZigbeeEP {
public:
  ZigbeeWindowCovering(uint8_t endpoint);
  ~ZigbeeWindowCovering();

  void onGoToLiftPercentage(void (*callback)(uint8_t)) {
    _on_go_to_lift_percentage = callback;
  }

  void onStop(void (*callback)()) {
    _on_stop = callback;
  }

  void setLiftPosition(uint16_t lift_position);

  // Set the window covering type, see esp_zb_zcl_window_covering_window_covering_type_t
  void setCoveringType(WindowCoveringType covering_type);

  // Set window covering config/status, see esp_zb_zcl_window_covering_config_status_t
  void setConfigStatus(bool operational, bool online, bool commands_reversed,
                       bool lift_closed_loop, bool tilt_closed_loop,
                       bool lift_encoder_controlled, bool tilt_encoder_controlled);

  // Set configuration mode of window covering, see esp_zb_zcl_window_covering_mode_t
  void setMode(bool motor_reversed, bool calibration_mode, bool maintenance_mode, bool leds_on);

  // Set limits of motion, see esp_zb_zcl_window_covering_info_attr_t
  void setLimits(uint16_t installed_open_limit_lift, uint16_t installed_closed_limit_lift,
                 uint16_t installed_open_limit_tilt, uint16_t installed_closed_limit_tilt);
private:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;
  void zbWindowCoveringMovementCmd(const esp_zb_zcl_window_covering_movement_message_t *message) override;

  void goToLiftPercentage(uint8_t);
  void stop();

  // callback function to be called on lift percentage change (lift percentage)
  void (*_on_go_to_lift_percentage)(uint8_t);
  void (*_on_stop)();

  /**
   * @brief  Create a standard HA window covering cluster list.
   *        Added here as not supported by ESP Zigbee library.
   *
   * @note This contains basic, identify, groups, scenes, window_covering, as server side.
   * @param[in] window_covering_cfg  Configuration parameters for this cluster lists defined by @ref zigbee_window_covering_cfg_t
   *
   * @return Pointer to cluster list  @ref esp_zb_cluster_list_s
   *
   */
  esp_zb_cluster_list_t *zigbee_window_covering_clusters_create(zigbee_window_covering_cfg_t *window_covering_cfg);

  uint8_t _current_lift_percentage;
  uint8_t _current_tilt_percentage;
  uint16_t _current_position_lift;
  uint16_t _installed_open_limit_lift;
  uint16_t _installed_closed_limit_lift;
  uint16_t _installed_open_limit_tilt;
  uint16_t _installed_closed_limit_tilt;
  uint16_t _physical_closed_limit_lift;
  uint16_t _physical_closed_limit_tilt;
};

#endif  // SOC_IEEE802154_SUPPORTED
