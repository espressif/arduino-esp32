// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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

#include "ZigbeeDoorLock.h"
#if CONFIG_ZB_ENABLED
#include "ezbee/zha.h"
#include "ezbee/zcl/cluster/door_lock.h"

ZigbeeDoorLock::ZigbeeDoorLock(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_DOOR_LOCK_DEVICE_ID;
  _on_lock = nullptr;
  _on_unlock = nullptr;
  _lock_state = DOOR_LOCK_STATE_UNDEFINED;
  _lock_type = DOOR_LOCK_TYPE_DEAD_BOLT;

  // ZHA template: Basic, Identify, Door Lock, Groups, Scenes.
  _ep_config = {.ep_id = _endpoint, .app_profile_id = EZB_AF_HA_PROFILE_ID, .app_device_id = EZB_ZHA_DOOR_LOCK_DEVICE_ID, .app_device_version = 0, .reserved = 0};
  ezb_zha_door_lock_config_t door_lock_cfg = EZB_ZHA_DOOR_LOCK_CONFIG();
  door_lock_cfg.door_lock_cfg.lock_state = _lock_state;
  door_lock_cfg.door_lock_cfg.lock_type = _lock_type;
  door_lock_cfg.door_lock_cfg.actuator_enabled = true;
  _ep_desc = ezb_zha_create_door_lock(_endpoint, &door_lock_cfg);
  if (_ep_desc == nullptr) {
    log_e("Failed to create door lock endpoint descriptor");
  }
}

bool ZigbeeDoorLock::setLockType(ZigbeeDoorLockType lock_type) {
  _lock_type = lock_type;
  return configureEpClusterAttr(
    "setLockType", EZB_ZCL_CLUSTER_ID_DOOR_LOCK, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_DOOR_LOCK_LOCK_TYPE_ID, (void *)&_lock_type,
    ezb_zcl_door_lock_cluster_desc_add_attr
  );
}

bool ZigbeeDoorLock::setLockState(ZigbeeDoorLockState state) {
  uint8_t value = (uint8_t)state;
  // Before Zigbee.begin() only the endpoint descriptor can be updated, nothing is reported yet.
  if (!Zigbee.stackRunning()) {
    if (!configureEpClusterAttr(
          "setLockState", EZB_ZCL_CLUSTER_ID_DOOR_LOCK, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_ID, (void *)&value,
          ezb_zcl_door_lock_cluster_desc_add_attr
        )) {
      return false;
    }
    _lock_state = state;
    return true;
  }

  log_v("Updating door lock state to %u", value);
  ezb_zcl_status_t ret = setClusterAttribute(EZB_ZCL_CLUSTER_ID_DOOR_LOCK, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_ID, &value, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set door lock state: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    return false;
  }
  _lock_state = state;
  return reportLockState();
}

bool ZigbeeDoorLock::reportLockState() {
  ezb_zcl_report_attr_cmd_t report_attr_cmd;
  memset(&report_attr_cmd, 0, sizeof(report_attr_cmd));
  ezb_address_set_none(&report_attr_cmd.cmd_ctrl.dst_addr);
  report_attr_cmd.cmd_ctrl.src_ep = _endpoint;
  report_attr_cmd.cmd_ctrl.cluster_id = EZB_ZCL_CLUSTER_ID_DOOR_LOCK;
  report_attr_cmd.cmd_ctrl.manuf_code = EZB_ZCL_STD_MANUF_CODE;
  report_attr_cmd.cmd_ctrl.fc.direction = EZB_ZCL_CMD_DIRECTION_TO_CLI;
  report_attr_cmd.payload.attr_id = EZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_ID;

  if (!reportClusterAttribute(&report_attr_cmd)) {
    log_e("Failed to send door lock state report");
    return false;
  }
  log_v("Door lock state report sent");
  return true;
}

// Lock Door / Unlock Door command received from the network (called in the Zigbee stack context, no Zigbee lock is taken here)
void ZigbeeDoorLock::zbDoorLockCmd(ezb_zcl_door_lock_lock_door_message_t *message, bool lock) {
  if (message->info.cluster_id != EZB_ZCL_CLUSTER_ID_DOOR_LOCK) {
    log_w("Received message ignored. Cluster ID: %u not supported for Door Lock", message->info.cluster_id);
    message->out.result = EZB_ZCL_STATUS_FAIL;
    return;
  }

  bool (*callback)() = lock ? _on_lock : _on_unlock;
  if (callback == nullptr) {
    log_w("No callback function set for door %s", lock ? "lock" : "unlock");
  } else if (!callback()) {
    log_w("Door %s rejected by the application", lock ? "lock" : "unlock");
    message->out.result = EZB_ZCL_STATUS_FAIL;
    return;
  }

  // Update the LockState attribute, the stack sends the report if reporting is configured by the coordinator.
  _lock_state = lock ? DOOR_LOCK_STATE_LOCKED : DOOR_LOCK_STATE_UNLOCKED;
  uint8_t value = (uint8_t)_lock_state;
  ezb_zcl_status_t ret =
    ezb_zcl_set_attr_value(_endpoint, EZB_ZCL_CLUSTER_ID_DOOR_LOCK, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_ID, EZB_ZCL_STD_MANUF_CODE, &value, false);
  if (ret != EZB_ZCL_STATUS_SUCCESS) {
    log_e("Failed to set door lock state: 0x%x: %s", ret, esp_zb_zcl_status_to_name(ret));
    message->out.result = ret;
    return;
  }
  message->out.result = EZB_ZCL_STATUS_SUCCESS;
}

#endif  // CONFIG_ZB_ENABLED
