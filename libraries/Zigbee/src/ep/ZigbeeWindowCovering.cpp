// Copyright 2025 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ZigbeeWindowCovering.h"
#if CONFIG_ZB_ENABLED

ZigbeeWindowCovering::ZigbeeWindowCovering(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_WINDOW_COVERING_DEVICE_ID;
  _on_open = nullptr;
  _on_close = nullptr;
  _on_go_to_lift_percentage = nullptr;
  _on_go_to_tilt_percentage = nullptr;
  _on_stop = nullptr;

  // set default values for window covering attributes
  // NOTE(zb-v2): v2.x defines no CurrentPositionLift/TiltPercentage DEFAULT_VALUE macro; only MIN/MAX
  // exist for the percentage attributes, so the spec minimum (0) is used as the initial value.
  _current_lift_percentage = EZB_ZCL_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_MIN_VALUE;
  _current_tilt_percentage = EZB_ZCL_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_MIN_VALUE;
  _installed_open_limit_lift = EZB_ZCL_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_LIFT_DEFAULT_VALUE;
  _installed_closed_limit_lift = EZB_ZCL_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_LIFT_DEFAULT_VALUE;
  _installed_open_limit_tilt = EZB_ZCL_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_TILT_DEFAULT_VALUE;
  _installed_closed_limit_tilt = EZB_ZCL_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_TILT_DEFAULT_VALUE;
  _current_lift_position = EZB_ZCL_WINDOW_COVERING_CURRENT_POSITION_LIFT_DEFAULT_VALUE;
  _current_tilt_position = EZB_ZCL_WINDOW_COVERING_CURRENT_POSITION_TILT_DEFAULT_VALUE;
  _physical_closed_limit_lift = EZB_ZCL_WINDOW_COVERING_PHYSICAL_CLOSED_LIMIT_LIFT_DEFAULT_VALUE;
  _physical_closed_limit_tilt = EZB_ZCL_WINDOW_COVERING_PHYSICAL_CLOSED_LIMIT_TILT_DEFAULT_VALUE;

  // v2.x data model: the ZHA template builds the full endpoint descriptor (basic, identify, groups,
  // scenes, window covering clusters) instead of the v1 cluster-list factory.
  ezb_zha_window_covering_config_t window_covering_cfg = EZB_ZHA_WINDOW_COVERING_CONFIG();
  _ep_desc = ezb_zha_create_window_covering(_endpoint, &window_covering_cfg);

  // The ZHA template only adds the mandatory window covering attributes (type, config status). Add the
  // optional position/limit attributes that this endpoint reports, matching the v1 cluster factory.
  ezb_zcl_cluster_desc_t window_covering_cluster =
    ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER);
  ezb_zcl_window_covering_cluster_desc_add_attr(
    window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID, &_current_lift_percentage
  );
  ezb_zcl_window_covering_cluster_desc_add_attr(
    window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_ID, &_current_tilt_percentage
  );
  ezb_zcl_window_covering_cluster_desc_add_attr(window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_ID, &_current_lift_position);
  ezb_zcl_window_covering_cluster_desc_add_attr(window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_ID, &_current_lift_position);
  ezb_zcl_window_covering_cluster_desc_add_attr(window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_LIFT_ID, &_installed_open_limit_lift);
  ezb_zcl_window_covering_cluster_desc_add_attr(window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_TILT_ID, &_installed_open_limit_tilt);
  ezb_zcl_window_covering_cluster_desc_add_attr(
    window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_LIFT_ID, &_installed_closed_limit_lift
  );
  ezb_zcl_window_covering_cluster_desc_add_attr(
    window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_TILT_ID, &_installed_closed_limit_tilt
  );
  ezb_zcl_window_covering_cluster_desc_add_attr(
    window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_PHYSICAL_CLOSED_LIMIT_LIFT_ID, &_physical_closed_limit_lift
  );
  ezb_zcl_window_covering_cluster_desc_add_attr(
    window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_PHYSICAL_CLOSED_LIMIT_TILT_ID, &_physical_closed_limit_lift
  );

  _ep_config = {
    .ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_WINDOW_COVERING_DEVICE_ID, .app_device_version = 0
  };
}

// Configuration methods for window covering
bool ZigbeeWindowCovering::setCoveringType(ZigbeeWindowCoveringType covering_type) {
  ezb_zcl_cluster_desc_t window_covering_cluster =
    ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret =
    ezb_zcl_window_covering_cluster_desc_add_attr(window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_WINDOW_COVERING_TYPE_ID, (void *)&covering_type);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set covering type: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeWindowCovering::setConfigStatus(
  bool operational, bool online, bool commands_reversed, bool lift_closed_loop, bool tilt_closed_loop, bool lift_encoder_controlled,
  bool tilt_encoder_controlled
) {
  uint8_t config_status =
    (operational ? EZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_OPERATIONAL : 0) | (online ? EZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_ONLINE : 0)
    | (commands_reversed ? EZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_OPEN_AND_UP_COMMANDS_REVERSED : 0)
    | (lift_closed_loop ? EZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_LIFT_CLOSED_LOOP : 0)
    | (tilt_closed_loop ? EZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_TILT_CLOSED_LOOP : 0)
    | (lift_encoder_controlled ? EZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_LIFT_ENCODER_CONTROLLED : 0)
    | (tilt_encoder_controlled ? EZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_TILT_ENCODER_CONTROLLED : 0);

  log_v("Updating window covering config status to %u", config_status);

  ezb_zcl_cluster_desc_t window_covering_cluster =
    ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret = ezb_zcl_window_covering_cluster_desc_add_attr(window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_STATUS_ID, (void *)&config_status);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set config status: 0x%x", ret);
    return false;
  }
  return true;
}

bool ZigbeeWindowCovering::setMode(bool motor_reversed, bool calibration_mode, bool maintenance_mode, bool leds_on) {
  uint8_t mode = (motor_reversed ? EZB_ZCL_WINDOW_COVERING_MODE_MOTOR_DIRECTION_REVERSED : 0)
                 | (calibration_mode ? EZB_ZCL_WINDOW_COVERING_MODE_CALIBRATION_MODE : 0)
                 | (maintenance_mode ? EZB_ZCL_WINDOW_COVERING_MODE_MAINTENANCE_MODE : 0) | (leds_on ? EZB_ZCL_WINDOW_COVERING_MODE_LED_FEEDBACK : 0);

  log_v("Updating window covering mode to %u", mode);

  ezb_zcl_cluster_desc_t window_covering_cluster =
    ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret = ezb_zcl_window_covering_cluster_desc_add_attr(window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_MODE_ID, (void *)&mode);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set mode: 0x%x", ret);
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

  ezb_zcl_cluster_desc_t window_covering_cluster =
    ezb_af_endpoint_get_cluster_desc(_ep_desc, EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER);
  ezb_err_t ret =
    ezb_zcl_window_covering_cluster_desc_add_attr(window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_LIFT_ID, (void *)&_installed_open_limit_lift);
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set installed open limit lift: 0x%x", ret);
    return false;
  }
  ret = ezb_zcl_window_covering_cluster_desc_add_attr(
    window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_LIFT_ID, (void *)&_installed_closed_limit_lift
  );
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set installed closed limit lift: 0x%x", ret);
    return false;
  }
  ret = ezb_zcl_window_covering_cluster_desc_add_attr(
    window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_OPEN_LIMIT_TILT_ID, (void *)&_installed_open_limit_tilt
  );
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set installed open limit tilt: 0x%x", ret);
    return false;
  }
  ret = ezb_zcl_window_covering_cluster_desc_add_attr(
    window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_INSTALLED_CLOSED_LIMIT_TILT_ID, (void *)&_installed_closed_limit_tilt
  );
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set installed closed limit tilt: 0x%x", ret);
    return false;
  }
  ret = ezb_zcl_window_covering_cluster_desc_add_attr(
    window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_PHYSICAL_CLOSED_LIMIT_LIFT_ID, (void *)&_physical_closed_limit_lift
  );
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set physical closed limit lift: 0x%x", ret);
    return false;
  }
  ret = ezb_zcl_window_covering_cluster_desc_add_attr(
    window_covering_cluster, EZB_ZCL_ATTR_WINDOW_COVERING_PHYSICAL_CLOSED_LIMIT_TILT_ID, (void *)&_physical_closed_limit_tilt
  );
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to set physical closed limit tilt: 0x%x", ret);
    return false;
  }
  return true;
}

// Callback for handling incoming messages and commands
void ZigbeeWindowCovering::zbAttributeSet(const ezb_zcl_set_attr_value_message_t *message) {
  //check the data and call right method
  if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_WINDOW_COVERING) {
    log_v("Received attribute id: 0x%u / data.type: 0x%u", message->in.attribute.id, message->in.attribute.data.type);
    if (message->in.attribute.id == EZB_ZCL_ATTR_WINDOW_COVERING_MODE_ID && message->in.attribute.data.type == EZB_ZCL_ATTR_TYPE_MAP8) {
      uint8_t mode = *(uint8_t *)message->in.attribute.data.value;
      bool motor_reversed = mode & EZB_ZCL_WINDOW_COVERING_MODE_MOTOR_DIRECTION_REVERSED;
      bool calibration_mode = mode & EZB_ZCL_WINDOW_COVERING_MODE_CALIBRATION_MODE;
      bool maintenance_mode = mode & EZB_ZCL_WINDOW_COVERING_MODE_MAINTENANCE_MODE;
      bool leds_on = mode & EZB_ZCL_WINDOW_COVERING_MODE_LED_FEEDBACK;
      log_v(
        "Updating window covering mode to motor reversed: %u, calibration mode: %u, maintenance mode: %u, leds on: %u", motor_reversed, calibration_mode,
        maintenance_mode, leds_on
      );
      setMode(motor_reversed, calibration_mode, maintenance_mode, leds_on);
      // Update configuration status with motor reversed status (stack callback: no Zigbee lock).
      // NOTE(zb-v2): runtime attribute access goes through the opaque attribute descriptor: read with
      // ezb_zcl_get_attr_desc()/ezb_zcl_attr_desc_get_value() and write with ezb_zcl_set_attr_value()
      // (the lock-free variant wrapped by setClusterAttribute()), preserving the no-lock callback flow.
      uint8_t config_status = 0;
      ezb_zcl_attr_desc_t config_status_attr =
        ezb_zcl_get_attr_desc(_endpoint, EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_STATUS_ID, EZB_ZCL_STD_MANUF_CODE);
      if (config_status_attr != nullptr) {
        ezb_zcl_attr_desc_get_value(config_status_attr, &config_status);
      }
      config_status = motor_reversed ? config_status | EZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_OPEN_AND_UP_COMMANDS_REVERSED
                                     : config_status & ~EZB_ZCL_WINDOW_COVERING_CONFIG_STATUS_OPEN_AND_UP_COMMANDS_REVERSED;
      log_v("Updating window covering config status to %u", config_status);
      ezb_zcl_status_t ret = ezb_zcl_set_attr_value(
        _endpoint, EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_WINDOW_COVERING_CONFIG_STATUS_ID, EZB_ZCL_STD_MANUF_CODE,
        &config_status, false
      );
      if (ret != EZB_ZCL_STATUS_SUCCESS) {
        log_e("Failed to set window covering config status: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
      }
      return;
    }
  } else {
    log_w("Received message ignored. Cluster ID: %u not supported for Window Covering", message->info.cluster_id);
  }
}

void ZigbeeWindowCovering::zbWindowCoveringMovementCmd(const ezb_zcl_window_covering_movement_message_t *message) {
  // check the data and call right method
  if (message->info.cluster_id == EZB_ZCL_CLUSTER_ID_WINDOW_COVERING) {
    // NOTE(zb-v2): the movement command ID is carried in the ZCL header (in.header->cmd_id) and the
    // payload is a union (lift/tilt value or percentage) selected by that command ID.
    const ezb_zcl_cmd_hdr_t *header = message->in.header;
    uint8_t command = header ? header->cmd_id : 0xff;
    if (command == EZB_ZCL_CMD_WINDOW_COVERING_UP_OPEN_ID) {
      open();
      return;
    } else if (command == EZB_ZCL_CMD_WINDOW_COVERING_DOWN_CLOSE_ID) {
      close();
      return;
    } else if (command == EZB_ZCL_CMD_WINDOW_COVERING_STOP_ID) {
      stop();
      return;
    } else if (command == EZB_ZCL_CMD_WINDOW_COVERING_GO_TO_LIFT_PERCENTAGE_ID) {
      if (_current_lift_percentage != message->in.payload.lift_percentage) {
        _current_lift_percentage = message->in.payload.lift_percentage;
        goToLiftPercentage(_current_lift_percentage);
        return;
      }
    } else if (command == EZB_ZCL_CMD_WINDOW_COVERING_GO_TO_TILT_PERCENTAGE_ID) {
      if (_current_tilt_percentage != message->in.payload.tilt_percentage) {
        _current_tilt_percentage = message->in.payload.tilt_percentage;
        goToTiltPercentage(_current_tilt_percentage);
        return;
      }
    } else {
      log_w("Received message ignored. Command: %u not supported for Window Covering", command);
    }
  } else {
    log_w("Received message ignored. Cluster ID: %u not supported for Window Covering", message->info.cluster_id);
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
  _current_lift_position = lift_position;
  _current_lift_percentage = ((lift_position - _installed_open_limit_lift) * 100) / (_installed_closed_limit_lift - _installed_open_limit_lift);
  log_v("Updating window covering lift position to %u (%u%)", _current_lift_position, _current_lift_percentage);

  ezb_zcl_status_t ret = setClusterAttribute(
    EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_ID, &_current_lift_position, false
  );
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set lift position: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  ret = setClusterAttribute(
    EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID, &_current_lift_percentage,
    false
  );
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set lift percentage: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeWindowCovering::setLiftPercentage(uint8_t lift_percentage) {
  _current_lift_percentage = lift_percentage;
  _current_lift_position = _installed_open_limit_lift + ((_installed_closed_limit_lift - _installed_open_limit_lift) * lift_percentage) / 100;
  log_v("Updating window covering lift percentage to %u%% (%u)", _current_lift_percentage, _current_lift_position);

  ezb_zcl_status_t ret = setClusterAttribute(
    EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_ID, &_current_lift_position, false
  );
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set lift position: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  ret = setClusterAttribute(
    EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_LIFT_PERCENTAGE_ID, &_current_lift_percentage,
    false
  );
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set lift percentage: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeWindowCovering::setTiltPosition(uint16_t tilt_position) {
  _current_tilt_position = tilt_position;
  _current_tilt_percentage = ((tilt_position - _installed_open_limit_tilt) * 100) / (_installed_closed_limit_tilt - _installed_open_limit_tilt);
  log_v("Updating window covering tilt position to %u (%u%)", _current_tilt_position, _current_tilt_percentage);

  ezb_zcl_status_t ret = setClusterAttribute(
    EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_ID, &_current_tilt_position, false
  );
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set tilt position: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  ret = setClusterAttribute(
    EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_ID, &_current_tilt_percentage,
    false
  );
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set tilt percentage: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

bool ZigbeeWindowCovering::setTiltPercentage(uint8_t tilt_percentage) {
  _current_tilt_percentage = tilt_percentage;
  _current_tilt_position = _installed_open_limit_tilt + ((_installed_closed_limit_tilt - _installed_open_limit_tilt) * tilt_percentage) / 100;
  log_v("Updating window covering tilt percentage to %u%% (%u)", _current_tilt_percentage, _current_tilt_position);

  ezb_zcl_status_t ret = setClusterAttribute(
    EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_ID, &_current_tilt_position, false
  );
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set tilt position: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  ret = setClusterAttribute(
    EZB_ZCL_CLUSTER_ID_WINDOW_COVERING, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_WINDOW_COVERING_CURRENT_POSITION_TILT_PERCENTAGE_ID, &_current_tilt_percentage,
    false
  );
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set tilt percentage: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  return true;
}

#endif  // CONFIG_ZB_ENABLED
