#include "ZigbeeWindowCovering.h"
#if CONFIG_ZB_ENABLED

esp_zb_cluster_list_t *ZigbeeWindowCovering::zigbee_window_covering_clusters_create(zigbee_window_covering_cfg_t *window_covering_cfg) {
  esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_basic_cluster_create(&window_covering_cfg->basic_cfg);
  esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_identify_cluster_create(&window_covering_cfg->identify_cfg);
  esp_zb_attribute_list_t *esp_zb_groups_cluster = esp_zb_groups_cluster_create(&window_covering_cfg->groups_cfg);
  esp_zb_attribute_list_t *esp_zb_scenes_cluster = esp_zb_scenes_cluster_create(&window_covering_cfg->scenes_cfg);
  esp_zb_attribute_list_t *esp_zb_window_covering_cluster = esp_zb_window_covering_cluster_create(&window_covering_cfg->window_covering_cfg);

  esp_zb_window_covering_cluster_add_attr(
    esp_zb_window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID, &_current_lift_percentage
  );
  esp_zb_window_covering_cluster_add_attr(
    esp_zb_window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_ID, &_current_tilt_percentage
  );
  esp_zb_window_covering_cluster_add_attr(esp_zb_window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_ID, &_current_lift_position);
  esp_zb_window_covering_cluster_add_attr(esp_zb_window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_ID, &_current_lift_position);
  esp_zb_window_covering_cluster_add_attr(
    esp_zb_window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_LIFT_ID, &_installed_open_limit_lift
  );
  esp_zb_window_covering_cluster_add_attr(
    esp_zb_window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_TILT_ID, &_installed_open_limit_tilt
  );
  esp_zb_window_covering_cluster_add_attr(
    esp_zb_window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_LIFT_ID, &_installed_closed_limit_lift
  );
  esp_zb_window_covering_cluster_add_attr(
    esp_zb_window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_TILT_ID, &_installed_closed_limit_tilt
  );
  esp_zb_window_covering_cluster_add_attr(
    esp_zb_window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_PHYSICAL_CLOSED_LIMIT_LIFT_ID, &_physical_closed_limit_lift
  );
  esp_zb_window_covering_cluster_add_attr(
    esp_zb_window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_PHY_CLOSED_LIMIT_TILT_ID, &_physical_closed_limit_lift
  );

  // ------------------------------ Create cluster list ------------------------------
  esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
  esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_groups_cluster(esp_zb_cluster_list, esp_zb_groups_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_scenes_cluster(esp_zb_cluster_list, esp_zb_scenes_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_zb_cluster_list_add_window_covering_cluster(esp_zb_cluster_list, esp_zb_window_covering_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);

  return esp_zb_cluster_list;
}

ZigbeeWindowCovering::ZigbeeWindowCovering(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = ESP_ZB_HA_WINDOW_COVERING_DEVICE_ID;

  // set default values for window covering attributes
  _current_lift_percentage = ESP_ZB_ZCL_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_DEFAULT_VALUE;
  _current_tilt_percentage = ESP_ZB_ZCL_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_DEFAULT_VALUE;
  _installed_open_limit_lift = ESP_ZB_ZCL_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_LIFT_DEFAULT_VALUE;
  _installed_closed_limit_lift = ESP_ZB_ZCL_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_LIFT_DEFAULT_VALUE;
  _installed_open_limit_tilt = ESP_ZB_ZCL_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_TILT_DEFAULT_VALUE;
  _installed_closed_limit_tilt = ESP_ZB_ZCL_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_TILT_DEFAULT_VALUE;
  _current_lift_position = ESP_ZB_ZCL_WINDOW_COVERING_CURRENT_POSITION_LIFT_DEFAULT_VALUE;
  _current_tilt_position = ESP_ZB_ZCL_WINDOW_COVERING_CURRENT_POSITION_TILT_DEFAULT_VALUE;
  _physical_closed_limit_lift = ESP_ZB_ZCL_WINDOW_COVERING_PHYSICAL_CLOSED_LIMIT_LIFT_DEFAULT_VALUE;
  _physical_closed_limit_tilt = ESP_ZB_ZCL_WINDOW_COVERING_PHY_CLOSED_LIMIT_TILT_DEFAULT_VALUE;

  // Create custom window covering configuration
  zigbee_window_covering_cfg_t window_covering_cfg = ZIGBEE_DEFAULT_WINDOW_COVERING_CONFIG();
  _cluster_list = zigbee_window_covering_clusters_create(&window_covering_cfg);

  _ep_config = {
    .endpoint = _endpoint, .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID, .app_device_id = ESP_ZB_HA_WINDOW_COVERING_DEVICE_ID, .app_device_version = 0
  };
}

// Configuration methods for window covering
bool ZigbeeWindowCovering::setCoveringType(ZigbeeWindowCoveringType covering_type) {
  esp_zb_attribute_list_t *window_covering_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_err_t ret = esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_WINDOW_COVERING_TYPE_ID, (void *)&covering_type);
  if (ret != ESP_OK) {
    log_e("Failed to set covering type: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeWindowCovering::setConfigStatus(
  bool operational, bool online, bool commands_reversed, bool lift_closed_loop, bool tilt_closed_loop, bool lift_encoder_controlled,
  bool tilt_encoder_controlled
) {
  uint8_t config_status = (operational ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_OPERATIONAL : 0) | (online ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_ONLINE : 0)
                          | (commands_reversed ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_REVERSE_COMMANDS : 0)
                          | (lift_closed_loop ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_LIFT_CONTROL_IS_CLOSED_LOOP : 0)
                          | (tilt_closed_loop ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_TILT_CONTROL_IS_CLOSED_LOOP : 0)
                          | (lift_encoder_controlled ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_LIFT_ENCODER_CONTROLLED : 0)
                          | (tilt_encoder_controlled ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_TILT_ENCODER_CONTROLLED : 0);

  log_v("Updating window covering config status to %d", config_status);

  esp_zb_attribute_list_t *window_covering_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_err_t ret = esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_STATUS_ID, (void *)&config_status);
  if (ret != ESP_OK) {
    log_e("Failed to set config status: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeWindowCovering::setMode(bool motor_reversed, bool calibration_mode, bool maintenance_mode, bool leds_on) {
  uint8_t mode = (motor_reversed ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_REVERSED_MOTOR_DIRECTION : 0)
                 | (calibration_mode ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_RUN_IN_CALIBRATION_MODE : 0)
                 | (maintenance_mode ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_MOTOR_IS_RUNNING_IN_MAINTENANCE_MODE : 0)
                 | (leds_on ? ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_LEDS_WILL_DISPLAY_FEEDBACK : 0);

  log_v("Updating window covering mode to %d", mode);

  esp_zb_attribute_list_t *window_covering_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_err_t ret = esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_MODE_ID, (void *)&mode);
  if (ret != ESP_OK) {
    log_e("Failed to set mode: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeWindowCovering::setLimits(
  uint16_t installed_open_limit_lift, uint16_t installed_closed_limit_lift, uint16_t installed_open_limit_tilt, uint16_t installed_closed_limit_tilt
) {
  _installed_open_limit_lift = installed_open_limit_lift;
  _installed_closed_limit_lift = installed_closed_limit_lift;
  _physical_closed_limit_lift = installed_closed_limit_lift;
  _installed_open_limit_tilt = installed_open_limit_tilt;
  _installed_closed_limit_tilt = installed_closed_limit_tilt;
  _physical_closed_limit_tilt = installed_closed_limit_tilt;

  esp_zb_attribute_list_t *window_covering_cluster =
    esp_zb_cluster_list_get_cluster(_cluster_list, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
  esp_err_t ret =
    esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_LIFT_ID, (void *)&_installed_open_limit_lift);
  if (ret != ESP_OK) {
    log_e("Failed to set installed open limit lift: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  ret =
    esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_LIFT_ID, (void *)&_installed_closed_limit_lift);
  if (ret != ESP_OK) {
    log_e("Failed to set installed closed limit lift: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  ret = esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_TILT_ID, (void *)&_installed_open_limit_tilt);
  if (ret != ESP_OK) {
    log_e("Failed to set installed open limit tilt: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  ret =
    esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_TILT_ID, (void *)&_installed_closed_limit_tilt);
  if (ret != ESP_OK) {
    log_e("Failed to set installed closed limit tilt: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  ret =
    esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_PHYSICAL_CLOSED_LIMIT_LIFT_ID, (void *)&_physical_closed_limit_lift);
  if (ret != ESP_OK) {
    log_e("Failed to set physical closed limit lift: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  ret = esp_zb_cluster_update_attr(window_covering_cluster, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_PHY_CLOSED_LIMIT_TILT_ID, (void *)&_physical_closed_limit_tilt);
  if (ret != ESP_OK) {
    log_e("Failed to set physical closed limit tilt: 0x%x: %s", ret, esp_err_to_name(ret));
    return false;
  }
  return true;
}

// Callback for handling incoming messages and commands
void ZigbeeWindowCovering::zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) {
  //check the data and call right method
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING) {
    log_v("Received attribute id: 0x%x / data.type: 0x%x", message->attribute.id, message->attribute.data.type);
    if (message->attribute.id == ESP_ZB_ZCL_ATTR_WINDOW_COVERING_MODE_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_8BITMAP) {
      uint8_t mode = *(uint8_t *)message->attribute.data.value;
      bool motor_reversed = mode & ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_REVERSED_MOTOR_DIRECTION;
      bool calibration_mode = mode & ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_RUN_IN_CALIBRATION_MODE;
      bool maintenance_mode = mode & ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_MOTOR_IS_RUNNING_IN_MAINTENANCE_MODE;
      bool leds_on = mode & ESP_ZB_ZCL_ATTR_WINDOW_COVERING_TYPE_LEDS_WILL_DISPLAY_FEEDBACK;
      log_v(
        "Updating window covering mode to motor reversed: %d, calibration mode: %d, maintenance mode: %d, leds on: %d", motor_reversed, calibration_mode,
        maintenance_mode, leds_on
      );
      setMode(motor_reversed, calibration_mode, maintenance_mode, leds_on);
      //Update Configuration status with motor reversed status
      uint8_t config_status;
      config_status = (*(uint8_t *)esp_zb_zcl_get_attribute(
                          _endpoint, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_STATUS_ID
      )
                          ->data_p);
      config_status = motor_reversed ? config_status | ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_REVERSE_COMMANDS
                                     : config_status & ~ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_REVERSE_COMMANDS;
      log_v("Updating window covering config status to %d", config_status);
      esp_zb_lock_acquire(portMAX_DELAY);
      esp_zb_zcl_set_attribute_val(
        _endpoint, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_STATUS_ID, &config_status,
        false
      );
      esp_zb_lock_release();
      return;
    }
  } else {
    log_w("Received message ignored. Cluster ID: %d not supported for Window Covering", message->info.cluster);
  }
}

void ZigbeeWindowCovering::zbWindowCoveringMovementCmd(const esp_zb_zcl_window_covering_movement_message_t *message) {
  // check the data and call right method
  if (message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING) {
    if (message->command == ESP_ZB_ZCL_CMD_WINDOW_COVERING_UP_OPEN) {
      open();
      return;
    } else if (message->command == ESP_ZB_ZCL_CMD_WINDOW_COVERING_DOWN_CLOSE) {
      close();
      return;
    } else if (message->command == ESP_ZB_ZCL_CMD_WINDOW_COVERING_STOP) {
      stop();
      return;
    } else if (message->command == ESP_ZB_ZCL_CMD_WINDOW_COVERING_GO_TO_LIFT_PERCENTAGE) {
      if (_current_lift_percentage != message->payload.percentage_lift_value) {
        _current_lift_percentage = message->payload.percentage_lift_value;
        goToLiftPercentage(_current_lift_percentage);
        return;
      }
    } else if (message->command == ESP_ZB_ZCL_CMD_WINDOW_COVERING_GO_TO_TILT_PERCENTAGE) {
      if (_current_tilt_percentage != message->payload.percentage_tilt_value) {
        _current_tilt_percentage = message->payload.percentage_tilt_value;
        goToTiltPercentage(_current_tilt_percentage);
        return;
      }
    } else {
      log_w("Received message ignored. Command: %d not supported for Window Covering", message->command);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %d not supported for Window Covering", message->info.cluster);
  }
}

void ZigbeeWindowCovering::open() {
  if (_on_open) {
    _on_open();
  }
}

void ZigbeeWindowCovering::close() {
  if (_on_close) {
    _on_close();
  }
}

void ZigbeeWindowCovering::goToLiftPercentage(uint8_t lift_percentage) {
  if (_on_go_to_lift_percentage) {
    _on_go_to_lift_percentage(lift_percentage);
  }
}

void ZigbeeWindowCovering::goToTiltPercentage(uint8_t tilt_percentage) {
  if (_on_go_to_tilt_percentage) {
    _on_go_to_tilt_percentage(tilt_percentage);
  }
}

void ZigbeeWindowCovering::stop() {
  if (_on_stop) {
    _on_stop();
  }
}

// Methods to control window covering from user application
bool ZigbeeWindowCovering::setLiftPosition(uint16_t lift_position) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  // Update both lift attributes
  _current_lift_position = lift_position;
  _current_lift_percentage = ((lift_position - _installed_open_limit_lift) * 100) / (_installed_closed_limit_lift - _installed_open_limit_lift);
  log_v("Updating window covering lift position to %d (%d%)", _current_lift_position, _current_lift_percentage);

  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_ID,
    &_current_lift_position, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set lift position: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID,
    &_current_lift_percentage, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set lift percentage: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
unlock_and_return:
  esp_zb_lock_release();
  return ret == ESP_ZB_ZCL_STATUS_SUCCESS;
}

bool ZigbeeWindowCovering::setLiftPercentage(uint8_t lift_percentage) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  // Update both lift attributes
  _current_lift_percentage = lift_percentage;
  _current_lift_position = _installed_open_limit_lift + ((_installed_closed_limit_lift - _installed_open_limit_lift) * lift_percentage) / 100;
  log_v("Updating window covering lift percentage to %d%% (%d)", _current_lift_percentage, _current_lift_position);

  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_ID,
    &_current_lift_position, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set lift position: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID,
    &_current_lift_percentage, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set lift percentage: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
unlock_and_return:
  esp_zb_lock_release();
  return ret == ESP_ZB_ZCL_STATUS_SUCCESS;
}

bool ZigbeeWindowCovering::setTiltPosition(uint16_t tilt_position) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  // Update both tilt attributes
  _current_tilt_position = tilt_position;
  _current_tilt_percentage = ((tilt_position - _installed_open_limit_tilt) * 100) / (_installed_closed_limit_tilt - _installed_open_limit_tilt);

  log_v("Updating window covering tilt position to %d (%d%)", _current_tilt_position, _current_tilt_percentage);

  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_ID,
    &_current_tilt_position, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set tilt position: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_ID,
    &_current_tilt_percentage, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set tilt percentage: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
unlock_and_return:
  esp_zb_lock_release();
  return ret == ESP_ZB_ZCL_STATUS_SUCCESS;
}

bool ZigbeeWindowCovering::setTiltPercentage(uint8_t tilt_percentage) {
  esp_zb_zcl_status_t ret = ESP_ZB_ZCL_STATUS_SUCCESS;
  // Update both tilt attributes
  _current_tilt_percentage = tilt_percentage;
  _current_tilt_position = _installed_open_limit_tilt + ((_installed_closed_limit_tilt - _installed_open_limit_tilt) * tilt_percentage) / 100;

  log_v("Updating window covering tilt percentage to %d%% (%d)", _current_tilt_percentage, _current_tilt_position);

  esp_zb_lock_acquire(portMAX_DELAY);
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_ID,
    &_current_tilt_position, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set tilt position: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
  ret = esp_zb_zcl_set_attribute_val(
    _endpoint, ESP_ZB_ZCL_CLUSTER_ID_WINDOW_COVERING, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_ID,
    &_current_tilt_percentage, false
  );
  if (ret != ESP_ZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set tilt percentage: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    goto unlock_and_return;
  }
unlock_and_return:
  esp_zb_lock_release();
  return ret == ESP_ZB_ZCL_STATUS_SUCCESS;
}

#endif  // CONFIG_ZB_ENABLED
