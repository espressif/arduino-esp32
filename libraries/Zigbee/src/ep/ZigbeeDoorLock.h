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

class ZigbeeDoorLock : public ZigbeeEP {
public:
  ZigbeeDoorLock(uint8_t endpoint);
  ~ZigbeeDoorLock() {}

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

  // Set the lock type (see ZigbeeDoorLockType), must be called before Zigbee.addEndpoint()
  bool setLockType(ZigbeeDoorLockType lock_type);

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
  bool reportLockState();

  bool (*_on_lock)();
  bool (*_on_unlock)();

  ZigbeeDoorLockState _lock_state;
  uint8_t _lock_type;
};

#endif  // CONFIG_ZB_ENABLED
