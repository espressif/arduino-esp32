/* Class of Zigbee Window Covering endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"
#include "ha/esp_zigbee_ha_standard.h"

// Window covering types supported by Zigbee Window Covering cluster
enum ZigbeeWindowCoveringType {
  ROLLERSHADE = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_ROLLERSHADE,                                    // LIFT support
  ROLLERSHADE_2_MOTOR = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_ROLLERSHADE_2_MOTOR,                    // LIFT support
  ROLLERSHADE_EXTERIOR = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_ROLLERSHADE_EXTERIOR,                  // LIFT support
  ROLLERSHADE_EXTERIOR_2_MOTOR = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_ROLLERSHADE_EXTERIOR_2_MOTOR,  // LIFT support
  DRAPERY = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_DRAPERY,                                            // LIFT support
  AWNING = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_AWNING,                                              // LIFT support
  SHUTTER = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_SHUTTER,                                            // TILT support
  BLIND_TILT_ONLY = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_TILT_BLIND_TILT_ONLY,                       // TILT support
  BLIND_LIFT_AND_TILT = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_TILT_BLIND_LIFT_AND_TILT,               // LIFT and TILT support
  PROJECTOR_SCREEN = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_PROJECTOR_SCREEN,                          // LIFT support
};

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

typedef struct zigbee_window_covering_cfg_s {
  esp_zb_basic_cluster_cfg_t basic_cfg;
  esp_zb_identify_cluster_cfg_t identify_cfg;
  esp_zb_groups_cluster_cfg_t groups_cfg;
  esp_zb_scenes_cluster_cfg_t scenes_cfg;
  esp_zb_window_covering_cluster_cfg_t window_covering_cfg;
} zigbee_window_covering_cfg_t;

class ZigbeeWindowCovering : public ZigbeeEP {
public:
  ZigbeeWindowCovering(uint8_t endpoint);
  ~ZigbeeWindowCovering() {}

  // Set the callback functions for the window covering commands
  void onOpen(void (*callback)()) {
    _on_open = callback;
  }
  void onClose(void (*callback)()) {
    _on_close = callback;
  }
  void onGoToLiftPercentage(void (*callback)(uint8_t)) {
    _on_go_to_lift_percentage = callback;
  }
  void onGoToTiltPercentage(void (*callback)(uint8_t)) {
    _on_go_to_tilt_percentage = callback;
  }
  void onStop(void (*callback)()) {
    _on_stop = callback;
  }

  // Set the window covering position in centimeters or percentage (0-100)
  bool setLiftPosition(uint16_t lift_position);
  bool setLiftPercentage(uint8_t lift_percentage);
  bool setTiltPosition(uint16_t tilt_position);
  bool setTiltPercentage(uint8_t tilt_percentage);

  // Set the window covering type (see ZigbeeWindowCoveringType)
  bool setCoveringType(ZigbeeWindowCoveringType covering_type);

  // Set window covering config/status, for more info see esp_zb_zcl_window_covering_config_status_t
  bool setConfigStatus(
    bool operational, bool online, bool commands_reversed, bool lift_closed_loop, bool tilt_closed_loop, bool lift_encoder_controlled,
    bool tilt_encoder_controlled
  );

  // Set configuration mode of window covering, for more info see esp_zb_zcl_window_covering_mode_t
  bool setMode(bool motor_reversed, bool calibration_mode, bool maintenance_mode, bool leds_on);

  // Set limits of motion, for more info see esp_zb_zcl_window_covering_info_attr_t
  bool setLimits(
    uint16_t installed_open_limit_lift, uint16_t installed_closed_limit_lift, uint16_t installed_open_limit_tilt, uint16_t installed_closed_limit_tilt
  );

private:
  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override;
  void zbWindowCoveringMovementCmd(const esp_zb_zcl_window_covering_movement_message_t *message) override;

  // Create window covering cluster list
  esp_zb_cluster_list_t *zigbee_window_covering_clusters_create(zigbee_window_covering_cfg_t *window_covering_cfg);

  void open();
  void close();
  void goToLiftPercentage(uint8_t);
  void goToTiltPercentage(uint8_t);
  void stop();

  // callback function to be called on lift percentage change (lift percentage)
  void (*_on_open)();
  void (*_on_close)();
  void (*_on_go_to_lift_percentage)(uint8_t);
  void (*_on_go_to_tilt_percentage)(uint8_t);
  void (*_on_stop)();

  // Widows covering lift attributes
  uint8_t _current_lift_percentage;
  uint16_t _current_lift_position;
  uint16_t _installed_open_limit_lift;
  uint16_t _installed_closed_limit_lift;
  uint16_t _physical_closed_limit_lift;

  // Windows covering tilt attributes
  uint8_t _current_tilt_percentage;
  uint16_t _current_tilt_position;
  uint16_t _installed_open_limit_tilt;
  uint16_t _installed_closed_limit_tilt;
  uint16_t _physical_closed_limit_tilt;
};

#endif  // CONFIG_ZB_ENABLED
