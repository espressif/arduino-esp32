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
#include "nvs.h"

ZigbeeDoorLock::ZigbeeDoorLock(uint8_t endpoint) : ZigbeeEP(endpoint) {
  _device_id = EZB_ZHA_DOOR_LOCK_DEVICE_ID;
  _on_lock = nullptr;
  _on_unlock = nullptr;
  _on_user_change = nullptr;
  _max_users = ZB_DOOR_LOCK_DEFAULT_USERS;
  _users = (User *)calloc(_max_users, sizeof(User));
  if (_users == nullptr) {
    log_e("Failed to allocate the door lock users");
    _max_users = 0;
  }
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
    return;
  }
  addPinAttributes();
}

ZigbeeDoorLock::~ZigbeeDoorLock() {
  free(_users);
  if (_store_lock != nullptr) {
    vSemaphoreDelete(_store_lock);
  }
}

// Stored users: blob "users" = [version (1 byte)][max_users (2 bytes)][User * max_users]
#define ZB_DOOR_LOCK_STORE_VERSION 1
#define ZB_DOOR_LOCK_STORE_HEADER  3

static void userStoreNamespace(char *ns, size_t size, uint8_t endpoint) {
  snprintf(ns, size, "zbdl%u", endpoint);
}

bool ZigbeeDoorLock::setUserStorage(bool enable) {
  if (!enable) {
    _store_users = false;
    return true;
  }
  if (_store_lock == nullptr) {
    _store_lock = xSemaphoreCreateMutex();
    if (_store_lock == nullptr) {
      log_e("Failed to create the user storage lock");
      return false;
    }
  }
  _store_users = true;
  return loadUsers();
}

bool ZigbeeDoorLock::loadUsers() {
  char ns[16];
  userStoreNamespace(ns, sizeof(ns), _endpoint);
  nvs_handle_t handle;
  if (nvs_open(ns, NVS_READONLY, &handle) != ESP_OK) {
    return true;  // Nothing stored yet
  }
  bool ok = false;
  size_t size = 0;
  if (nvs_get_blob(handle, "users", nullptr, &size) == ESP_OK && size >= ZB_DOOR_LOCK_STORE_HEADER) {
    uint8_t *buf = (uint8_t *)malloc(size);
    if (buf != nullptr && nvs_get_blob(handle, "users", buf, &size) == ESP_OK && buf[0] == ZB_DOOR_LOCK_STORE_VERSION) {
      uint16_t stored_users = buf[1] | (buf[2] << 8);
      if (size == ZB_DOOR_LOCK_STORE_HEADER + stored_users * sizeof(User)) {
        uint16_t copy = stored_users < _max_users ? stored_users : _max_users;
        portENTER_CRITICAL(&_users_lock);
        memcpy(_users, buf + ZB_DOOR_LOCK_STORE_HEADER, copy * sizeof(User));
        for (uint16_t i = 0; i < copy; i++) {
          _users[i].pin[ZB_DOOR_LOCK_MAX_PIN_LENGTH] = '\0';
        }
        portEXIT_CRITICAL(&_users_lock);
        ok = true;
        log_i("Restored %u door lock users from NVS", copy);
      }
    }
    free(buf);
  }
  nvs_close(handle);
  if (!ok) {
    log_w("Stored door lock users are invalid, ignored");
  }
  return true;
}

void ZigbeeDoorLock::saveUsers() {
  if (!_store_users || _users == nullptr) {
    return;
  }
  size_t size = ZB_DOOR_LOCK_STORE_HEADER + _max_users * sizeof(User);
  uint8_t *buf = (uint8_t *)malloc(size);
  if (buf == nullptr) {
    log_e("Failed to allocate the door lock users for storing");
    return;
  }
  portENTER_CRITICAL(&_users_lock);
  buf[0] = ZB_DOOR_LOCK_STORE_VERSION;
  buf[1] = _max_users & 0xff;
  buf[2] = _max_users >> 8;
  memcpy(buf + ZB_DOOR_LOCK_STORE_HEADER, _users, _max_users * sizeof(User));
  portEXIT_CRITICAL(&_users_lock);

  char ns[16];
  userStoreNamespace(ns, sizeof(ns), _endpoint);
  xSemaphoreTake(_store_lock, portMAX_DELAY);
  nvs_handle_t handle;
  esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
  if (err == ESP_OK) {
    err = nvs_set_blob(handle, "users", buf, size);
    if (err == ESP_OK) {
      err = nvs_commit(handle);
    }
    nvs_close(handle);
  }
  xSemaphoreGive(_store_lock);
  free(buf);
  if (err != ESP_OK) {
    log_e("Failed to store the door lock users: %s", esp_err_to_name(err));
  }
}

void ZigbeeDoorLock::clearUsers() {
  portENTER_CRITICAL(&_users_lock);
  if (_users != nullptr) {
    memset(_users, 0, _max_users * sizeof(User));
  }
  portEXIT_CRITICAL(&_users_lock);
  saveUsers();
}

ZigbeeDoorLockState ZigbeeDoorLock::restoreLockState() {
  uint8_t value = DOOR_LOCK_STATE_UNDEFINED;
  if (getAttribute(EZB_ZCL_CLUSTER_ID_DOOR_LOCK, EZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_ID, &value, sizeof(value))) {
    _lock_state = (ZigbeeDoorLockState)value;
  }
  return _lock_state;
}

// Advertise the PIN code capabilities (optional DoorLock attributes)
bool ZigbeeDoorLock::addPinAttributes() {
  uint16_t users = _max_users;
  uint8_t max_len = ZB_DOOR_LOCK_MAX_PIN_LENGTH;
  uint8_t min_len = ZB_DOOR_LOCK_MIN_PIN_LENGTH;
  bool ok = configureEpClusterAttr(
    "setMaxUsers", EZB_ZCL_CLUSTER_ID_DOOR_LOCK, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_DOOR_LOCK_NUMBER_OF_PIN_USERS_SUPPORTED_ID, (void *)&users,
    ezb_zcl_door_lock_cluster_desc_add_attr
  );
  ok &= configureEpClusterAttr(
    "setMaxUsers", EZB_ZCL_CLUSTER_ID_DOOR_LOCK, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_DOOR_LOCK_MAX_PIN_CODE_LENGTH_ID, (void *)&max_len,
    ezb_zcl_door_lock_cluster_desc_add_attr
  );
  ok &= configureEpClusterAttr(
    "setMaxUsers", EZB_ZCL_CLUSTER_ID_DOOR_LOCK, EZB_ZCL_CLUSTER_SERVER, EZB_ZCL_ATTR_DOOR_LOCK_MIN_PIN_CODE_LENGTH_ID, (void *)&min_len,
    ezb_zcl_door_lock_cluster_desc_add_attr
  );
  if (!ok) {
    log_e("Failed to add the PIN code attributes to the door lock endpoint");
  }
  return ok;
}

bool ZigbeeDoorLock::setMaxUsers(uint16_t max_users) {
  if (!requireBeforeAddEndpoint("setMaxUsers")) {
    return false;
  }
  if (max_users == 0 || max_users == 0xffff) {
    log_e("Invalid number of users: %u", max_users);
    return false;
  }
  User *users = (User *)calloc(max_users, sizeof(User));
  if (users == nullptr) {
    log_e("Failed to allocate %u door lock users", max_users);
    return false;
  }
  portENTER_CRITICAL(&_users_lock);
  User *old_users = _users;
  uint16_t copy = _max_users < max_users ? _max_users : max_users;
  if (old_users != nullptr) {
    memcpy(users, old_users, copy * sizeof(User));
  }
  _users = users;
  _max_users = max_users;
  portEXIT_CRITICAL(&_users_lock);
  free(old_users);
  if (_store_users) {
    loadUsers();  // The stored users depend on the table size
  }
  return addPinAttributes();
}

bool ZigbeeDoorLock::pinIsValid(const char *pin) {
  if (pin == nullptr) {
    return false;
  }
  size_t len = strnlen(pin, ZB_DOOR_LOCK_MAX_PIN_LENGTH + 1);
  return len >= ZB_DOOR_LOCK_MIN_PIN_LENGTH && len <= ZB_DOOR_LOCK_MAX_PIN_LENGTH;
}

// Must be called with _users_lock held
int32_t ZigbeeDoorLock::findPinLocked(const char *pin, int32_t skip_user, bool enabled_only) {
  for (uint16_t i = 0; i < _max_users; i++) {
    if ((int32_t)i == skip_user || _users[i].status == DOOR_LOCK_USER_AVAILABLE) {
      continue;
    }
    if (enabled_only && _users[i].status != DOOR_LOCK_USER_ENABLED) {
      continue;
    }
    if (strcmp(_users[i].pin, pin) == 0) {
      return i;
    }
  }
  return -1;
}

ZigbeeDoorLockUserStatus ZigbeeDoorLock::getUserStatus(uint16_t user_id) {
  ZigbeeDoorLockUserStatus status = DOOR_LOCK_USER_AVAILABLE;
  portENTER_CRITICAL(&_users_lock);
  if (user_id < _max_users) {
    status = (ZigbeeDoorLockUserStatus)_users[user_id].status;
  }
  portEXIT_CRITICAL(&_users_lock);
  return status;
}

bool ZigbeeDoorLock::getUserPin(uint16_t user_id, char *pin, size_t pin_size) {
  if (pin == nullptr || pin_size == 0) {
    return false;
  }
  bool found = false;
  pin[0] = '\0';
  portENTER_CRITICAL(&_users_lock);
  if (user_id < _max_users && _users[user_id].status != DOOR_LOCK_USER_AVAILABLE) {
    strlcpy(pin, _users[user_id].pin, pin_size);
    found = true;
  }
  portEXIT_CRITICAL(&_users_lock);
  return found;
}

bool ZigbeeDoorLock::setUser(uint16_t user_id, ZigbeeDoorLockUserStatus status, const char *pin) {
  if (user_id >= _max_users) {
    log_e("Invalid user ID: %u", user_id);
    return false;
  }
  if (status != DOOR_LOCK_USER_AVAILABLE) {
    if ((status != DOOR_LOCK_USER_ENABLED && status != DOOR_LOCK_USER_DISABLED) || !pinIsValid(pin)) {
      log_e("Invalid user status or PIN code");
      return false;
    }
  }
  portENTER_CRITICAL(&_users_lock);
  bool duplicate = status != DOOR_LOCK_USER_AVAILABLE && findPinLocked(pin, user_id, false) >= 0;
  if (!duplicate) {
    memset(&_users[user_id], 0, sizeof(User));
    _users[user_id].status = status;
    if (status != DOOR_LOCK_USER_AVAILABLE) {
      strlcpy(_users[user_id].pin, pin, sizeof(_users[user_id].pin));
    }
  }
  portEXIT_CRITICAL(&_users_lock);
  if (duplicate) {
    log_e("The PIN code is already used by another user");
    return false;
  }
  saveUsers();
  return true;
}

int32_t ZigbeeDoorLock::checkPin(const char *pin) {
  if (!pinIsValid(pin)) {
    return -1;
  }
  portENTER_CRITICAL(&_users_lock);
  int32_t user_id = findPinLocked(pin, -1, true);
  portEXIT_CRITICAL(&_users_lock);
  return user_id;
}

bool ZigbeeDoorLock::reportOperationEvent(ZigbeeDoorLockOperationSource source, uint8_t event_code, uint16_t user_id, const char *pin) {
  if (!Zigbee.stackRunning()) {
    log_w("Cannot report operation event: Zigbee stack not running");
    return false;
  }
  if (Zigbee.paused()) {
    log_w("Cannot report operation event: Zigbee stack is paused");
    return false;
  }
  size_t pin_len = pin ? strnlen(pin, ZB_DOOR_LOCK_MAX_PIN_LENGTH + 1) : 0;
  if (pin_len > ZB_DOOR_LOCK_MAX_PIN_LENGTH) {
    log_e("The PIN code is too long");
    return false;
  }

  ezb_zcl_door_lock_operation_event_notif_cmd_t cmd;
  memset(&cmd, 0, sizeof(cmd));
  ezb_address_set_none(&cmd.cmd_ctrl.dst_addr);
  cmd.cmd_ctrl.src_ep = _endpoint;
  cmd.payload.event_source = source;
  cmd.payload.event_code = event_code;
  cmd.payload.user_id = user_id;
  cmd.payload.pin_code[0] = pin_len;
  if (pin_len > 0) {
    memcpy(&cmd.payload.pin_code[1], pin, pin_len);
  }
  cmd.payload.local_time = 0xffffffff;  // Unknown time

  if (!esp_zigbee_lock_acquire(portMAX_DELAY)) {
    log_w("Cannot report operation event: failed to acquire Zigbee lock");
    return false;
  }
  ezb_err_t ret = ezb_zcl_door_lock_operation_event_notif_cmd_req(&cmd);
  esp_zigbee_lock_release();
  if (ret != EZB_ERR_NONE) {
    log_e("Failed to send operation event: 0x%x", ret);
    return false;
  }
  return true;
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

// PIN code / user commands received from the network (called in the Zigbee stack context, no Zigbee lock is taken here)
void ZigbeeDoorLock::zbDoorLockSetPinCode(ezb_zcl_door_lock_set_pin_code_message_t *message) {
  const ezb_zcl_door_lock_set_pin_code_payload_t *req = &message->in.payload;
  message->out.result = EZB_ZCL_STATUS_SUCCESS;
  message->out.status = EZB_ZCL_DOOR_LOCK_CMD_STATUS_FAIL;

  uint16_t user_id = req->user_id;
  uint8_t pin_len = req->pin_code[0];
  if (pin_len > ZB_DOOR_LOCK_MAX_PIN_LENGTH) {
    return;  // Already rejected by the stack, kept as a guard for the buffer below
  }
  char pin[ZB_DOOR_LOCK_MAX_PIN_LENGTH + 1];
  memcpy(pin, &req->pin_code[1], pin_len);
  pin[pin_len] = '\0';

  if (user_id >= _max_users) {
    message->out.status = EZB_ZCL_DOOR_LOCK_CMD_STATUS_MEMORY_FULL;
    return;
  }
  if ((req->user_status != DOOR_LOCK_USER_ENABLED && req->user_status != DOOR_LOCK_USER_DISABLED) || !pinIsValid(pin)) {
    log_w("SetPINCode rejected: user %u, status %u, PIN length %u", user_id, req->user_status, pin_len);
    return;
  }

  portENTER_CRITICAL(&_users_lock);
  if (findPinLocked(pin, user_id, false) >= 0) {
    message->out.status = EZB_ZCL_DOOR_LOCK_CMD_STATUS_DUPLICATE_CODE;
  } else {
    _users[user_id].status = req->user_status;
    _users[user_id].type = req->user_type;
    strlcpy(_users[user_id].pin, pin, sizeof(_users[user_id].pin));
    message->out.status = EZB_ZCL_DOOR_LOCK_CMD_STATUS_SUCCESS;
  }
  portEXIT_CRITICAL(&_users_lock);

  if (message->out.status == EZB_ZCL_DOOR_LOCK_CMD_STATUS_SUCCESS) {
    saveUsers();
    if (_on_user_change) {
      _on_user_change(user_id);
    }
  }
}

void ZigbeeDoorLock::zbDoorLockGetPinCode(ezb_zcl_door_lock_get_pin_code_message_t *message) {
  uint16_t user_id = message->in.payload.user_id;
  message->out.result = EZB_ZCL_STATUS_SUCCESS;
  // Defaults of the message are used for unknown users: available, user type not supported, no PIN.
  portENTER_CRITICAL(&_users_lock);
  if (user_id < _max_users && _users[user_id].status != DOOR_LOCK_USER_AVAILABLE) {
    size_t pin_len = strlen(_users[user_id].pin);
    message->out.user_status = _users[user_id].status;
    message->out.user_type = _users[user_id].type;
    message->out.pin_code[0] = pin_len;
    memcpy(&message->out.pin_code[1], _users[user_id].pin, pin_len);
  }
  portEXIT_CRITICAL(&_users_lock);
}

void ZigbeeDoorLock::zbDoorLockClearPinCode(ezb_zcl_door_lock_clear_pin_code_message_t *message) {
  uint16_t user_id = message->in.payload.user_id;
  message->out.result = EZB_ZCL_STATUS_SUCCESS;
  message->out.status = EZB_ZCL_DOOR_LOCK_CMD_STATUS_FAIL;
  if (user_id >= _max_users) {
    return;
  }
  portENTER_CRITICAL(&_users_lock);
  memset(&_users[user_id], 0, sizeof(User));
  portEXIT_CRITICAL(&_users_lock);
  message->out.status = EZB_ZCL_DOOR_LOCK_CMD_STATUS_SUCCESS;
  saveUsers();
  if (_on_user_change) {
    _on_user_change(user_id);
  }
}

void ZigbeeDoorLock::zbDoorLockClearAllPinCodes(ezb_zcl_door_lock_clear_all_pin_codes_message_t *message) {
  message->out.result = EZB_ZCL_STATUS_SUCCESS;
  clearUsers();
  message->out.status = EZB_ZCL_DOOR_LOCK_CMD_STATUS_SUCCESS;
  if (_on_user_change) {
    _on_user_change(ZB_DOOR_LOCK_ALL_USERS);
  }
}

void ZigbeeDoorLock::zbDoorLockSetUserStatus(ezb_zcl_door_lock_set_user_status_message_t *message) {
  uint16_t user_id = message->in.payload.user_id;
  uint8_t status = message->in.payload.user_status;
  message->out.result = EZB_ZCL_STATUS_SUCCESS;
  message->out.status = EZB_ZCL_DOOR_LOCK_CMD_STATUS_FAIL;
  if (user_id >= _max_users || (status != DOOR_LOCK_USER_ENABLED && status != DOOR_LOCK_USER_DISABLED)) {
    return;
  }
  portENTER_CRITICAL(&_users_lock);
  if (_users[user_id].status != DOOR_LOCK_USER_AVAILABLE) {
    _users[user_id].status = status;
    message->out.status = EZB_ZCL_DOOR_LOCK_CMD_STATUS_SUCCESS;
  }
  portEXIT_CRITICAL(&_users_lock);
  if (message->out.status == EZB_ZCL_DOOR_LOCK_CMD_STATUS_SUCCESS) {
    saveUsers();
    if (_on_user_change) {
      _on_user_change(user_id);
    }
  }
}

#endif  // CONFIG_ZB_ENABLED
