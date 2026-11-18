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
bool btClassicInUse(void);
bool bleInUse(void);

bool btStarted();
bool btStart();

// Bluedroid host lifecycle. Bluedroid is a single host shared by BLE and
// BluetoothSerial; the first user starts the controller and host, and the last
// one stops them. A host already enabled by an external component is adopted
// and never stopped here.
typedef enum {
  BT_HOST_STILL_IN_USE = 0,
  BT_HOST_STOPPED,   ///< Last library user; the controller and host were stopped.
  BT_HOST_EXTERNAL,  ///< Last library user; an external owner keeps the host up.
} bt_host_release_result_t;

// Take a reference to the shared host, starting it when this is the first user.
// @return false when the host could not be started.
bool btBluedroidHostAcquire(void);

// Release a reference. The last user stops the host unless it was external;
// the controller stop is deferred to btBluedroidHostFinalize() so the library
// can run its post-end hooks while the controller is still up.
bt_host_release_result_t btBluedroidHostRelease(void);

// Complete a controller stop deferred by the last btBluedroidHostRelease().
// Safe to call when nothing is pending.
void btBluedroidHostFinalize(void);
// Start the controller in the given mode.
//
// On the ESP32 a request narrower than BTDM is honored: the freed mode's memory
// is really gone (btMemRelease is physical) and the controller runs the
// surviving half. On the split-controller targets (ESP32-S31, ESP32-H4) the
// halves are fixed at build time and esp_bt_mem_release() frees nothing, so the
// request is clamped to that built-in mode (with a log) instead of narrowing:
// a single-half controller under the shared dual-mode Bluedroid host asserts or
// hangs. When the S31 controller gains runtime mode selection, remove this
// clamp together with the BT_MODE_MEM_RELEASE_PHYSICAL gate and the BTDM early
// return in btMemRelease(); see DESIGN.md, "To reclaim the unused half in a
// BTDM build".
bool btStartMode(bt_mode mode);
bool btStop();

// Release BT memory for the given mode and track the release.
// ESP-IDF provides no API to query whether memory has been released,
// so this function maintains the state internally.
// Returns true on success or if memory was already released.
//
// The tracked state is the end(true) contract: the mode's library is terminally
// released and cannot be restarted this boot. On the split-controller targets
// this is a pure contract — the stub under it frees nothing — so it never
// narrows a later btStartMode() request. See DESIGN.md, "To reclaim the unused
// half in a BTDM build", for the coordinated controller/core/test changes that
// enable a real BTDM reclaim.
bool btMemRelease(bt_mode mode);

// Returns true if BT memory for the given mode has already been released.
bool btMemReleased(bt_mode mode);

// Mark BT memory for the given mode as released without calling ESP-IDF.
// Use this when an external component (e.g. the ESP-IDF Matter stack) has
// already released BT memory on its own, so that btMemReleased() reflects
// the true state and btMemRelease() does not attempt a double-release.
void btMarkMemReleased(bt_mode mode);

/**
 * @brief Query the current BT controller mode.
 * @return The active bt_mode, or BT_MODE_DEFAULT if the controller is idle/uninitialized.
 */
bt_mode btGetMode(void);

#ifdef __cplusplus
}
#endif

#endif /* SOC_BT_SUPPORTED */

#endif /* _ESP32_ESP32_HAL_BT_H_ */
