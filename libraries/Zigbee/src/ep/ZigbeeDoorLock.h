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

/**
 * @brief Door lock LockState attribute values.
 */
enum ZigbeeDoorLockState {
  DOOR_LOCK_STATE_NOT_FULLY_LOCKED = 0x00, /**< Not fully locked. */
  DOOR_LOCK_STATE_LOCKED = 0x01,           /**< Locked. */
  DOOR_LOCK_STATE_UNLOCKED = 0x02,         /**< Unlocked. */
  DOOR_LOCK_STATE_UNDEFINED = 0xff,        /**< Initial / unknown state. */
};

/**
 * @brief Door lock hardware types defined by the Door Lock cluster.
 */
enum ZigbeeDoorLockType {
  DOOR_LOCK_TYPE_DEAD_BOLT = 0x00,           /**< Dead bolt (default). */
  DOOR_LOCK_TYPE_MAGNETIC = 0x01,            /**< Magnetic. */
  DOOR_LOCK_TYPE_OTHER = 0x02,               /**< Other. */
  DOOR_LOCK_TYPE_MORTISE = 0x03,             /**< Mortise. */
  DOOR_LOCK_TYPE_RIM = 0x04,                 /**< Rim. */
  DOOR_LOCK_TYPE_LATCH_BOLT = 0x05,          /**< Latch bolt. */
  DOOR_LOCK_TYPE_CYLINDRICAL_LOCK = 0x06,    /**< Cylindrical lock. */
  DOOR_LOCK_TYPE_TUBULAR_LOCK = 0x07,        /**< Tubular lock. */
  DOOR_LOCK_TYPE_INTERCONNECTED_LOCK = 0x08, /**< Interconnected lock. */
  DOOR_LOCK_TYPE_DEAD_LATCH = 0x09,          /**< Dead latch. */
  DOOR_LOCK_TYPE_DOOR_FURNITURE = 0x0a,      /**< Door furniture. */
};

/**
 * @brief User status of a PIN user (Home Assistant / ZHA manage users through the network).
 */
enum ZigbeeDoorLockUserStatus {
  DOOR_LOCK_USER_AVAILABLE = 0x00, /**< The user slot is not used. */
  DOOR_LOCK_USER_ENABLED = 0x01,   /**< The PIN is valid. */
  DOOR_LOCK_USER_DISABLED = 0x03,  /**< The PIN is stored but must not be accepted. */
};

/**
 * @brief Source of an operation event, reported with `reportOperationEvent()`.
 */
enum ZigbeeDoorLockOperationSource {
  DOOR_LOCK_SOURCE_KEYPAD = 0x00,        /**< Keypad. */
  DOOR_LOCK_SOURCE_RF = 0x01,            /**< Remote access over the Zigbee network. */
  DOOR_LOCK_SOURCE_MANUAL = 0x02,        /**< Manual (key or thumb turn). */
  DOOR_LOCK_SOURCE_RFID = 0x03,          /**< RFID. */
  DOOR_LOCK_SOURCE_INDETERMINATE = 0xff, /**< Source unknown. */
};

/**
 * @brief Operation event codes (ZCL Door Lock Operation Event Code table).
 *
 * All sources share this value mapping, but each code is only applicable to some
 * sources. The value is not the bit position in the *OperationEventMask attributes.
 */
enum ZigbeeDoorLockEvent {
  DOOR_LOCK_EVENT_UNKNOWN = 0x00,                         /**< All sources. */
  DOOR_LOCK_EVENT_LOCK = 0x01,                            /**< All sources. */
  DOOR_LOCK_EVENT_UNLOCK = 0x02,                          /**< All sources. */
  DOOR_LOCK_EVENT_LOCK_FAILURE_INVALID_PIN = 0x03,        /**< Keypad, RF, RFID. */
  DOOR_LOCK_EVENT_LOCK_FAILURE_INVALID_SCHEDULE = 0x04,   /**< Keypad, RF, RFID. */
  DOOR_LOCK_EVENT_UNLOCK_FAILURE_INVALID_PIN = 0x05,      /**< Keypad, RF, RFID. */
  DOOR_LOCK_EVENT_UNLOCK_FAILURE_INVALID_SCHEDULE = 0x06, /**< Keypad, RF, RFID. */
  DOOR_LOCK_EVENT_ONE_TOUCH_LOCK = 0x07,                  /**< Manual. */
  DOOR_LOCK_EVENT_KEY_LOCK = 0x08,                        /**< Manual. */
  DOOR_LOCK_EVENT_KEY_UNLOCK = 0x09,                      /**< Manual. */
  DOOR_LOCK_EVENT_AUTO_LOCK = 0x0a,                       /**< Manual. */
  DOOR_LOCK_EVENT_SCHEDULE_LOCK = 0x0b,                   /**< Manual. */
  DOOR_LOCK_EVENT_SCHEDULE_UNLOCK = 0x0c,                 /**< Manual. */
  DOOR_LOCK_EVENT_MANUAL_LOCK = 0x0d,                     /**< Manual (key or thumb turn). */
  DOOR_LOCK_EVENT_MANUAL_UNLOCK = 0x0e,                   /**< Manual (key or thumb turn). */
  DOOR_LOCK_EVENT_NON_ACCESS_USER = 0x0f,                 /**< Keypad. */
};

#define ZB_DOOR_LOCK_MAX_PIN_LENGTH 16      // Maximum number of characters of a PIN code
#define ZB_DOOR_LOCK_MIN_PIN_LENGTH 4       // Minimum number of characters of a PIN code (advertised in the MinPINCodeLength attribute)
#define ZB_DOOR_LOCK_DEFAULT_USERS  10      // Default number of PIN users, see setMaxUsers()
#define ZB_DOOR_LOCK_ALL_USERS      0xFFFF  // user_id passed to the onUserChange() callback when all the users were removed

/**
 * @brief Door lock endpoint (Zigbee HA Door Lock cluster server).
 *
 * Handles Lock Door / Unlock Door commands and PIN code users. Schedules, RFID
 * users, user types, and event masks are not supported yet.
 */
class ZigbeeDoorLock : public ZigbeeEP {
public:
  /**
   * @brief Create a Zigbee door lock endpoint.
   * @param endpoint Endpoint number (1-254).
   */
  ZigbeeDoorLock(uint8_t endpoint);
  /**
   * @brief Destroy the door lock endpoint and free PIN user storage.
   */
  ~ZigbeeDoorLock();

  /**
   * @brief Set the callback for Lock Door commands from the network.
   *
   * The callback performs the physical action and returns true on success. When it
   * returns false, the command is answered with a failure status and LockState is
   * left unchanged. Without a callback the command is accepted and only the
   * attribute is updated. Runs in the Zigbee stack context — keep it short and do
   * not call Zigbee APIs from it.
   *
   * @param callback Function that performs the lock action.
   */
  void onLock(bool (*callback)()) {
    _on_lock = callback;
  }
  /**
   * @brief Set the callback for Unlock Door commands from the network.
   *
   * Same rules as `onLock()`: return true on success, keep it short, do not call
   * Zigbee APIs from the callback.
   *
   * @param callback Function that performs the unlock action.
   */
  void onUnlock(bool (*callback)()) {
    _on_unlock = callback;
  }

  /**
   * @brief Set the number of PIN users supported.
   *
   * Must be called before `Zigbee.addEndpoint()`. Default is `ZB_DOOR_LOCK_DEFAULT_USERS`.
   *
   * @param max_users Maximum number of PIN users.
   * @return true if successful.
   */
  bool setMaxUsers(uint16_t max_users);
  /**
   * @brief Get the configured maximum number of PIN users.
   * @return Maximum user count.
   */
  uint16_t getMaxUsers() {
    return _max_users;
  }

  /**
   * @brief Set a callback invoked when a user is added, changed, or removed by the network.
   *
   * Runs in the Zigbee stack context — keep it short. `user_id` is
   * `ZB_DOOR_LOCK_ALL_USERS` if all users were removed (ClearAllPINCodes).
   *
   * @param callback Function called with the affected user ID.
   */
  void onUserChange(void (*callback)(uint16_t user_id)) {
    _on_user_change = callback;
  }

  /**
   * @brief Store PIN users in NVS and restore them now.
   *
   * Call in `setup()`, after `setMaxUsers()`. From then on every user change
   * (from the network or by `setUser()`/`clearUsers()`) is stored. PIN codes are
   * stored as plain text; use flash and NVS encryption if they must be protected.
   *
   * @param enable true to enable NVS storage.
   * @return true if successful.
   */
  bool setUserStorage(bool enable);
  /**
   * @brief Remove all users (including stored ones).
   *
   * Call before `Zigbee.factoryReset()`, which only clears the Zigbee dataset.
   */
  void clearUsers();

  /**
   * @brief Get the status of a PIN user.
   * @param user_id Zero-based user ID.
   * @return User status (`DOOR_LOCK_USER_AVAILABLE`, `_ENABLED`, or `_DISABLED`).
   */
  ZigbeeDoorLockUserStatus getUserStatus(uint16_t user_id);
  /**
   * @brief Copy the PIN of a user as a null-terminated string.
   * @param user_id Zero-based user ID.
   * @param pin Output buffer.
   * @param pin_size Size of the output buffer.
   * @return false if the user does not exist.
   */
  bool getUserPin(uint16_t user_id, char *pin, size_t pin_size);
  /**
   * @brief Set a user locally (for example to restore after a restart).
   *
   * The PIN must have `ZB_DOOR_LOCK_MIN_PIN_LENGTH` to `ZB_DOOR_LOCK_MAX_PIN_LENGTH`
   * characters. The same PIN cannot be used by two users.
   *
   * @param user_id Zero-based user ID.
   * @param status User status.
   * @param pin PIN code string.
   * @return true if successful.
   */
  bool setUser(uint16_t user_id, ZigbeeDoorLockUserStatus status, const char *pin);
  /**
   * @brief Check a PIN entered locally (for example on a keypad).
   * @param pin PIN code string.
   * @return User ID of the enabled user with this PIN, or -1.
   */
  int32_t checkPin(const char *pin);

  /**
   * @brief Send an operation event notification to bound devices.
   *
   * ZHA / Home Assistant shows this as a lock event with its source and user.
   * The PIN is only sent if the coordinator enabled SendPINOverTheAir; otherwise
   * the stack masks it (`0xFF`). Not every event code is applicable to every source.
   *
   * @param source Event source (`DOOR_LOCK_SOURCE_KEYPAD`, `_RF`, `_MANUAL`, `_RFID`, or `_INDETERMINATE`).
   * @param event_code `ZigbeeDoorLockEvent` value (e.g. `DOOR_LOCK_EVENT_LOCK`).
   * @param user_id User that operated the lock (`0xffff` if unknown).
   * @param pin PIN code used (optional).
   * @return true if successful.
   */
  bool reportOperationEvent(ZigbeeDoorLockOperationSource source, uint8_t event_code, uint16_t user_id = 0xffff, const char *pin = nullptr);

  /**
   * @brief Set the lock type. Must be called before `Zigbee.addEndpoint()`.
   * @param lock_type Lock type (see `ZigbeeDoorLockType`).
   * @return true if successful.
   */
  bool setLockType(ZigbeeDoorLockType lock_type);

  /**
   * @brief Read the LockState attribute back after `Zigbee.begin()`.
   *
   * Use after a reboot if LockState was marked persistent with
   * `setAttributePersistent(EZB_ZCL_CLUSTER_ID_DOOR_LOCK, EZB_ZCL_ATTR_DOOR_LOCK_LOCK_STATE_ID)`
   * (requires esp-zigbee-lib 2.0.5 or newer). The stored state may not match the
   * real position of the lock — check it.
   *
   * @return Stored state, or `DOOR_LOCK_STATE_UNDEFINED` if nothing was stored.
   */
  ZigbeeDoorLockState restoreLockState();

  /**
   * @brief Set the LockState attribute.
   *
   * After `Zigbee.begin()` the new state is also reported to bound devices.
   * Use this to report local operations (key, button, keypad).
   *
   * @param state New lock state.
   * @return true if successful.
   */
  bool setLockState(ZigbeeDoorLockState state);
  /**
   * @brief Set LockState to locked and report it.
   * @return true if successful.
   */
  bool setLocked() {
    return setLockState(DOOR_LOCK_STATE_LOCKED);
  }
  /**
   * @brief Set LockState to unlocked and report it.
   * @return true if successful.
   */
  bool setUnlocked() {
    return setLockState(DOOR_LOCK_STATE_UNLOCKED);
  }

  /**
   * @brief Get the current LockState.
   * @return Current lock state.
   */
  ZigbeeDoorLockState getLockState() {
    return _lock_state;
  }
  /**
   * @brief Check whether the lock is in the locked state.
   * @return true if LockState is `DOOR_LOCK_STATE_LOCKED`.
   */
  bool isLocked() {
    return _lock_state == DOOR_LOCK_STATE_LOCKED;
  }

private:
  void zbDoorLockCmd(ezb_zcl_door_lock_lock_door_message_t *message, bool lock) override;
  void zbDoorLockSetPinCode(ezb_zcl_door_lock_set_pin_code_message_t *message) override;
  void zbDoorLockGetPinCode(ezb_zcl_door_lock_get_pin_code_message_t *message) override;
  void zbDoorLockClearPinCode(ezb_zcl_door_lock_clear_pin_code_message_t *message) override;
  void zbDoorLockClearAllPinCodes(ezb_zcl_door_lock_clear_all_pin_codes_message_t *message) override;
  void zbDoorLockSetUserStatus(ezb_zcl_door_lock_set_user_status_message_t *message) override;
  void zbDoorLockGetUserStatus(ezb_zcl_door_lock_get_user_status_message_t *message) override;
  bool reportLockState();
  bool addPinAttributes();

  struct User {
    uint8_t status;
    uint8_t type;
    char pin[ZB_DOOR_LOCK_MAX_PIN_LENGTH + 1];
  };
  static bool pinIsValid(const char *pin);
  static bool pinOctetIsValid(const uint8_t *pin_code);
  bool ensureStoreLock();
  bool loadUsers();
  void saveUsers();
  void eraseStoredUsers();
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
