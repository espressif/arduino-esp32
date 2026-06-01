// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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

#ifndef _ESP32_ESP32_HAL_BT_H_
#define _ESP32_ESP32_HAL_BT_H_

#include "soc/soc_caps.h"
#if SOC_BT_SUPPORTED

#include "esp32-hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  BT_MODE_DEFAULT,
  BT_MODE_BLE,
  BT_MODE_CLASSIC_BT,
  BT_MODE_BTDM
} bt_mode;

// Returns true if BT memory should be kept, false to release it (~36KB).
// Default (weak): returns false unless a BT library is linked.
// BT libraries include esp32-hal-alloc-ble-mem.h and esp32-hal-alloc-bt-classic-mem.h which set
// _bleLibraryInUse and _btClassicLibraryInUse to true via constructor.
// Users may also provide their own strong *InUse() to override.
bool _btInUse_default(void);
bool btInUse(void);
bool btClassicInUse(void);
bool bleInUse(void);

bool btStarted();
bool btStart();
bool btStartMode(bt_mode mode);
bool btStop();

// Release BT memory for the given mode and track the release.
// ESP-IDF provides no API to query whether memory has been released,
// so this function maintains the state internally.
// Returns true on success or if memory was already released.
bool btMemRelease(bt_mode mode);

// Returns true if BT memory for the given mode has already been released.
bool btMemReleased(bt_mode mode);

// Mark BT memory for the given mode as released without calling ESP-IDF.
// Use this when an external component (e.g. the ESP-IDF Matter stack) has
// already released BT memory on its own, so that btMemReleased() reflects
// the true state and btMemRelease() does not attempt a double-release.
void btMarkMemReleased(bt_mode mode);

#ifdef __cplusplus
}
#endif

#endif /* SOC_BT_SUPPORTED */

#endif /* _ESP32_ESP32_HAL_BT_H_ */
