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

/* Class of Zigbee Door Lock endpoint inherited from common EP class */

#pragma once

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if CONFIG_ZB_ENABLED

#include "ZigbeeEP.h"

// Arduino-friendly enums for the Door Lock cluster attributes
enum ZigbeeDoorLockState {
  DOOR_LOCK_STATE_NOT_FULLY_LOCKED = 0x00,
  DOOR_LOCK_STATE_LOCKED = 0x01,
  DOOR_LOCK_STATE_UNLOCKED = 0x02,
  DOOR_LOCK_STATE_UNDEFINED = 0xff,
};

enum ZigbeeDoorLockType {
  DOOR_LOCK_TYPE_DEAD_BOLT = 0x00,
  DOOR_LOCK_TYPE_MAGNETIC = 0x01,
  DOOR_LOCK_TYPE_OTHER = 0x02,
  DOOR_LOCK_TYPE_MORTISE = 0x03,
  DOOR_LOCK_TYPE_RIM = 0x04,
  DOOR_LOCK_TYPE_LATCH_BOLT = 0x05,
  DOOR_LOCK_TYPE_CYLINDRICAL_LOCK = 0x06,
  DOOR_LOCK_TYPE_TUBULAR_LOCK = 0x07,
  DOOR_LOCK_TYPE_INTERCONNECTED_LOCK = 0x08,
  DOOR_LOCK_TYPE_DEAD_LATCH = 0x09,
  DOOR_LOCK_TYPE_DOOR_FURNITURE = 0x0a,
};

// User status of a PIN user (Home Assistant / ZHA manage the users through the Zigbee network)
enum ZigbeeDoorLockUserStatus {
  DOOR_LOCK_USER_AVAILABLE = 0x00,  // The user slot is not used
  DOOR_LOCK_USER_ENABLED = 0x01,    // The PIN is valid
  DOOR_LOCK_USER_DISABLED = 0x03,   // The PIN is stored but must not be accepted
};

// Source of an operation event, reported with reportOperationEvent()
enum ZigbeeDoorLockOperationSource {
  DOOR_LOCK_SOURCE_KEYPAD = 0x00,
  DOOR_LOCK_SOURCE_RF = 0x01,  // Remote access over the Zigbee network
  DOOR_LOCK_SOURCE_MANUAL = 0x02,
  DOOR_LOCK_SOURCE_RFID = 0x03,
  DOOR_LOCK_SOURCE_INDETERMINATE = 0xff,
};

// Operation event codes (ZCL Door Lock operation event codes, as used by ZHA)
enum ZigbeeDoorLockEvent {
  DOOR_LOCK_EVENT_UNKNOWN = 0x00,
  DOOR_LOCK_EVENT_LOCK = 0x01,    // Keypad, RF, RFID
  DOOR_LOCK_EVENT_UNLOCK = 0x02,  // Keypad, RF, RFID
  DOOR_LOCK_EVENT_LOCK_FAILURE_INVALID_PIN = 0x03,
  DOOR_LOCK_EVENT_LOCK_FAILURE_INVALID_SCHEDULE = 0x04,
  DOOR_LOCK_EVENT_UNLOCK_FAILURE_INVALID_PIN = 0x05,
  DOOR_LOCK_EVENT_UNLOCK_FAILURE_INVALID_SCHEDULE = 0x06,
  DOOR_LOCK_EVENT_ONE_TOUCH_LOCK = 0x07,  // Manual
  DOOR_LOCK_EVENT_KEY_LOCK = 0x08,        // Manual
  DOOR_LOCK_EVENT_KEY_UNLOCK = 0x09,      // Manual
  DOOR_LOCK_EVENT_AUTO_LOCK = 0x0a,
  DOOR_LOCK_EVENT_SCHEDULE_LOCK = 0x0b,
  DOOR_LOCK_EVENT_SCHEDULE_UNLOCK = 0x0c,
  DOOR_LOCK_EVENT_MANUAL_LOCK = 0x0d,    // Manual (key or thumb turn)
  DOOR_LOCK_EVENT_MANUAL_UNLOCK = 0x0e,  // Manual (key or thumb turn)
  DOOR_LOCK_EVENT_NON_ACCESS_USER = 0x0f,
};

#define ZB_DOOR_LOCK_MAX_PIN_LENGTH 16  // Maximum number of characters of a PIN code
#define ZB_DOOR_LOCK_MIN_PIN_LENGTH 4   // Minimum number of characters of a PIN code (advertised in the MinPINCodeLength attribute)
#define ZB_DOOR_LOCK_DEFAULT_USERS  10  // Default number of PIN users, see setMaxUsers()

class ZigbeeDoorLock : public ZigbeeEP {
public:
  ZigbeeDoorLock(uint8_t endpoint);
  ~ZigbeeDoorLock();

  // Set the callbacks for the Lock Door / Unlock Door commands received from the Zigbee network.
  // The callback performs the physical action and returns true on success. When it returns false,
  // the command is answered with a failure status and the LockState attribute is left unchanged.
  // Without a callback the command is accepted and only the LockState attribute is updated.
  // The callbacks run in the Zigbee stack context, keep them short and do not call Zigbee APIs from them.
  void onLock(bool (*callback)()) {
    _on_lock = callback;
  }
  void onUnlock(bool (*callback)()) {
    _on_unlock = callback;
  }

  // PIN code users
  // The PIN codes are stored in RAM and managed by the Zigbee coordinator (e.g. Home Assistant). User IDs are zero based.
  // Set the number of PIN users supported, must be called before Zigbee.addEndpoint(). Default is ZB_DOOR_LOCK_DEFAULT_USERS.
  bool setMaxUsers(uint16_t max_users);
  uint16_t getMaxUsers() {
    return _max_users;
  }

  // Callback called when a user is added, changed or removed by the Zigbee network (in the Zigbee stack context, keep it short).
  // Use it to persist the users (getUserStatus() / getUserPin()). Return values are not used.
  void onUserChange(void (*callback)(uint16_t user_id)) {
    _on_user_change = callback;
  }

  // Store the PIN users in NVS (namespace "zbdl<endpoint>") and restore them now. Call it in setup(), after setMaxUsers().
  // From then on every change of the users (from the network or by setUser()/clearUsers()) is stored.
  // The PIN codes are stored as plain text, use flash encryption and NVS encryption if the device needs to protect them.
  bool setUserStorage(bool enable);
  // Remove all users (and the stored ones). Call it before Zigbee.factoryReset(), which only clears the Zigbee dataset.
  void clearUsers();

  ZigbeeDoorLockUserStatus getUserStatus(uint16_t user_id);
  // Copy the PIN of the user as a null terminated string, returns false if the user does not exist
  bool getUserPin(uint16_t user_id, char *pin, size_t pin_size);
  // Set a user locally (e.g. restore it after a restart). The PIN must have ZB_DOOR_LOCK_MIN_PIN_LENGTH to ZB_DOOR_LOCK_MAX_PIN_LENGTH characters.
  bool setUser(uint16_t user_id, ZigbeeDoorLockUserStatus status, const char *pin);
  // Check the PIN entered locally (e.g. on a keypad). Returns the user ID of the enabled user that has this PIN, or -1.
  int32_t checkPin(const char *pin);

  // Send an operation event notification (what ZHA/Home Assistant shows as a lock event) to the bound devices.
  // event_code is one of ZigbeeDoorLockEvent, e.g. DOOR_LOCK_EVENT_LOCK / DOOR_LOCK_EVENT_UNLOCK for the keypad, RF and RFID sources.
  // user_id is 0xffff if there is no user. The PIN is only sent if the coordinator enabled SendPINOverTheAir.
  bool reportOperationEvent(ZigbeeDoorLockOperationSource source, uint8_t event_code, uint16_t user_id = 0xffff, const char *pin = nullptr);

  // Set the lock type (see ZigbeeDoorLockType), must be called before Zigbee.addEndpoint()
  bool setLockType(ZigbeeDoorLockType lock_type);

  // Read the LockState attribute back into the cached state after Zigbee.begin(). Use it after a reboot if the LockState was marked
  // persistent with setAttributePersistent(EZB_ZCL_CLUSTER_ID_DOOR_LOCK, EZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_ID).
  // Returns DOOR_LOCK_STATE_UNDEFINED if nothing was stored. The stored state may not match the real position of the lock, check it.
  ZigbeeDoorLockState restoreLockState();

  // Set the LockState attribute. After Zigbee.begin() the new state is also reported to the bound devices.
  bool setLockState(ZigbeeDoorLockState state);
  bool setLocked() {
    return setLockState(DOOR_LOCK_STATE_LOCKED);
  }
  bool setUnlocked() {
    return setLockState(DOOR_LOCK_STATE_UNLOCKED);
  }

  ZigbeeDoorLockState getLockState() {
    return _lock_state;
  }
  bool isLocked() {
    return _lock_state == DOOR_LOCK_STATE_LOCKED;
  }

private:
  void zbDoorLockCmd(ezb_zcl_door_lock_lock_door_message_t *message, bool lock) override;
  void zbDoorLockSetPinCode(ezb_zcl_door_lock_set_pin_code_message_t *message) override;
  void zbDoorLockGetPinCode(ezb_zcl_door_lock_get_pin_code_message_t *message) override;
  void zbDoorLockClearPinCode(ezb_zcl_door_lock_clear_pin_code_message_t *message) override;
  void zbDoorLockSetUserStatus(ezb_zcl_door_lock_set_user_status_message_t *message) override;
  bool reportLockState();
  bool addPinAttributes();

  struct User {
    uint8_t status;
    uint8_t type;
    char pin[ZB_DOOR_LOCK_MAX_PIN_LENGTH + 1];
  };
  static bool pinIsValid(const char *pin);
  bool loadUsers();
  void saveUsers();
  int32_t findPinLocked(const char *pin, int32_t skip_user, bool enabled_only);

  bool (*_on_lock)();
  bool (*_on_unlock)();
  void (*_on_user_change)(uint16_t user_id);

  User *_users;
  uint16_t _max_users;
  portMUX_TYPE _users_lock = portMUX_INITIALIZER_UNLOCKED;
  bool _store_users = false;
  SemaphoreHandle_t _store_lock = nullptr;

  ZigbeeDoorLockState _lock_state;
  uint8_t _lock_type;
};

#endif  // CONFIG_ZB_ENABLED
