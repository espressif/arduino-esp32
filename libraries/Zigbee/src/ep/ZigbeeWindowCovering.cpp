
#include "ZigbeeWindowCovering.h"
#if SOC_IEEE802154_SUPPORTED && CONFIG_ZB_ENABLED

#include "esp_zigbee_cluster.h"

ZigbeeWindowCovering::ZigbeeWindowCovering(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_WINDOW_COVERING_DEVICE_ID;

  zigbee_window_covering_cfg_t window_covering_cfg = ZIGBEE_DEFAULT_WINDOW_COVERING_CONFIG();
  _cluster_list = zigbee_window_covering_clusters_create(&window_covering_cfg);

  _ep_config = {.endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_WINDOW_COVERING_DEVICE_ID, .app_device_version = 0};

  // set default values
  _current_lift_percentage = ESP_ZB_ZCL_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_DEFAULT_VALUE;
  _current_tilt_percentage = ESP_ZB_ZCL_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_DEFAULT_VALUE;
  _installed_open_limit_lift =  ESP_ZB_ZCL_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_LIFT_DEFAULT_VALUE;
  _installed_closed_limit_lift = ESP_ZB_ZCL_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_LIFT_DEFAULT_VALUE;
  _installed_open_limit_tilt = ESP_ZB_ZCL_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_TILT_DEFAULT_VALUE;
  _installed_closed_limit_tilt = ESP_ZB_ZCL_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_TILT_DEFAULT_VALUE;
  _current_position_lift = ESP_ZB_ZCL_WINDOW_COVERING_CURRENT_POSITION_LIFT_DEFAULT_VALUE;
  _physical_closed_limit_lift = ESP_ZB_ZCL_WINDOW_COVERING_PHYSICAL_CLOSED_LIMIT_LIFT_DEFAULT_VALUE;
  _physical_closed_limit_tilt =  ESP_ZB_ZCL_WINDOW_COVERING_PHY_CLOSED_LIMIT_TILT_DEFAULT_VALUE;
}

// set attribute method -> method overridden in child class
void ZigbeeWindowCovering::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
  //check the data and call right method
  log_w("Received message ignored. Cluster ID: %d not supported for Window Covering", message->info.cluster);
}

void ZigbeeWindowCovering::zbWindowCoveringMovementCmd(const esp_zb_zcl_window_covering_movement_message_t *message) {
  // check the data and call right method
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING) {
    if (message->command == ESP_ZB_ZCL_CMD_WINDOW_COVERING_UP_OPEN) {
      goToLiftPercentage(0);
      return;
    } else if (message->command == ESP_ZB_ZCL_CMD_WINDOW_COVERING_DOWN_CLOSE) {
      goToLiftPercentage(100);
      return;
    } else if (message->command == ESP_ZB_ZCL_CMD_WINDOW_COVERING_STOP) {
      stop();
      return;
    } else if (message->command == ESP_ZB_ZCL_CMD_WINDOW_COVERING_GO_TO_LIFT_PERCENTAGE) {
      if (_current_lift_percentage != message->payload.percentage_lift_value) {
        _current_lift_percentage = message->payload.percentage_lift_value;
        goToLiftPercentage(_current_lift_percentage);
      }
      return;
    } else {
      log_w("Received message ignored. Command: %d not supported for Window Covering", message->command);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %d not supported for Window Covering", message->info.cluster);
  }
}

void ZigbeeWindowCovering::goToLiftPercentage(uint8_t lift_percentage) {
  if (_on_go_to_lift_percentage) {
    _on_go_to_lift_percentage(lift_percentage);
  }
}

void ZigbeeWindowCovering::stop() {
  if (_on_stop) {
    _on_stop();
  }
}

void ZigbeeWindowCovering::setLiftPosition(uint16_t lift_position) {
  // Update all attributes
  _current_position_lift = lift_position;
  _current_lift_percentage = ((lift_position - _installed_open_limit_lift) * 100) / (_installed_closed_limit_lift - _installed_open_limit_lift);

  log_v("Updating window covering lift position to %d (%d%)", _current_position_lift, _current_lift_percentage);
  // set lift state
  esp_zb_lock_acquire(portMAX_DELAY);
  esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_ID, &_current_position_lift, false
  );
  esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID, &_current_lift_percentage, false
  );

  esp_zb_zcl_report_attr_cmd_t report_attr_cmd;
  report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
  report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING;
  report_attr_cmd.zcl_basic_cmd.src_endpoint = _endpoint;

  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_ID;
  esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
  report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID;
  esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);

  esp_zb_lock_release();
}

void ZigbeeWindowCovering::setCoveringType(WindowCoveringType covering_type) {
  esp_zb_attribute_list_t *window_covering_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_WINDOW_COVERING_TYPE_ID, (void *)&covering_type);
}

void ZigbeeWindowCovering::setConfigStatus(bool operational, bool online, bool commands_reversed,
                       bool lift_closed_loop, bool tilt_closed_loop,
                       bool lift_encoder_controlled, bool tilt_encoder_controlled) {
  uint8_t config_status =  (operational ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_OPERATIONAL : 0)
                         | (online ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_ONLINE : 0)
                         | (commands_reversed ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_REVERSE_COMMANDS : 0)
                         | (lift_closed_loop ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_LIFT_CONTROL_IS_CLOSED_LOOP : 0)
                         | (tilt_closed_loop ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_TILT_CONTROL_IS_CLOSED_LOOP : 0)
                         | (lift_encoder_controlled ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_LIFT_ENCODER_CONTROLLED : 0)
                         | (tilt_encoder_controlled ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_TILT_ENCODER_CONTROLLED : 0);

  log_v("Updating window covering config status to %d", config_status);

  esp_zb_attribute_list_t *window_covering_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_STATUS_ID, (void *)&config_status);
}

void ZigbeeWindowCovering::setMode(bool motor_reversed, bool calibration_mode, bool maintenance_mode, bool leds_on) {
  uint8_t mode = (motor_reversed ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_REVERSED_MOTOR_DIRECTION : 0)
                         | (calibration_mode ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_RUN_IN_CALIBRATION_MODE : 0)
                         | (maintenance_mode ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_MOTOR_IS_RUNNING_IN_MAINTENANCE_MODE : 0)
                         | (leds_on ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_LEDS_WILL_DISPLAY_FEEDBACK : 0);

  log_v("Updating window covering mode to %d", mode);

  esp_zb_attribute_list_t *window_covering_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_MODE_ID, (void *)&mode);
}

void ZigbeeWindowCovering::setLimits(uint16_t installed_open_limit_lift, uint16_t installed_closed_limit_lift,
                                     uint16_t installed_open_limit_tilt, uint16_t installed_closed_limit_tilt) {
  _installed_open_limit_lift = installed_open_limit_lift;
  _installed_closed_limit_lift = installed_closed_limit_lift;
  _physical_closed_limit_lift = installed_closed_limit_lift;
  _installed_open_limit_tilt = installed_open_limit_tilt;
  _installed_closed_limit_tilt = installed_closed_limit_tilt;
  _physical_closed_limit_tilt = installed_closed_limit_tilt;

  esp_zb_attribute_list_t *window_covering_cluster =
      esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_LIFT_ID, (void *)&_installed_open_limit_lift);
  esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_LIFT_ID, (void *)&_installed_closed_limit_lift);
  esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_TILT_ID, (void *)&_installed_open_limit_tilt);
  esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_TILT_ID, (void *)&_installed_closed_limit_tilt);
  esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_PHYSICAL_CLOSED_LIMIT_LIFT_ID, (void *)&_physical_closed_limit_lift);
  esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_PHY_CLOSED_LIMIT_TILT_ID, (void *)&_physical_closed_limit_tilt);
}

esp_zb_cluster_list_t *ZigbeeWindowCovering::zigbee_window_covering_clusters_create(zigbee_window_covering_cfg_t *window_covering_cfg) {
  esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_basic_cluster_create(&window_covering_cfg->basic_cfg);
  esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_identify_cluster_create(&window_covering_cfg->identify_cfg);
  esp_zb_attribute_list_t *esp_zb_groups_cluster = esp_zb_groups_cluster_create(&window_covering_cfg->groups_cfg);
  esp_zb_attribute_list_t *esp_zb_scenes_cluster = esp_zb_scenes_cluster_create(&window_covering_cfg->scenes_cfg);
  esp_zb_attribute_list_t *esp_zb_window_covering_cluster = esp_zb_window_covering_cluster_create(&window_covering_cfg->window_covering_cfg);

  // ------------------------------ Create cluster list ------------------------------
  esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_groups_cluster(esp_zb_cluster_list, esp_zb_groups_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_scenes_cluster(esp_zb_cluster_list, esp_zb_scenes_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_window_covering_cluster(esp_zb_cluster_list, esp_zb_window_covering_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  esp_zb_window_covering_cluster_add_attr(esp_zb_window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID, &_current_lift_percentage);
  esp_zb_window_covering_cluster_add_attr(esp_zb_window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_LIFT_ID, &_installed_open_limit_lift);
  esp_zb_window_covering_cluster_add_attr(esp_zb_window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_LIFT_ID, &_installed_closed_limit_lift);
  esp_zb_window_covering_cluster_add_attr(esp_zb_window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_TILT_ID, &_installed_open_limit_tilt);
  esp_zb_window_covering_cluster_add_attr(esp_zb_window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_TILT_ID, &_installed_closed_limit_tilt);
  esp_zb_window_covering_cluster_add_attr(esp_zb_window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_ID, &_current_position_lift);
  esp_zb_window_covering_cluster_add_attr(esp_zb_window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_PHYSICAL_CLOSED_LIMIT_LIFT_ID, &_physical_closed_limit_lift);
  return esp_zb_cluster_list;
}

#endif  // SOC_IEEE802154_SUPPORTED
