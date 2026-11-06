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

#include "esp32-hal-bt.h"

#if SOC_BT_SUPPORTED
#if (defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)) && __has_include("esp_bt.h")

// Flags set by constructors in esp32-hal-alloc-ble-mem.h and esp32-hal-alloc-bt-classic-mem.h
// when BLE and/or BT Classic libraries are linked
bool _bleLibraryInUse = false;
bool _btClassicLibraryInUse = false;

// Track whether BT memory has been released per mode.
// ESP-IDF provides no API to query release state, so we track it here.
static bool _bleMemReleased = false;
#if defined(CONFIG_BT_CLASSIC_ENABLED)
static bool _classicMemReleased = false;
#else
static bool _classicMemReleased = true;  // No Classic BT on this chip
#endif

// DEPRECATED: Remove this function in v4.0. Use btClassicInUse() and bleInUse() instead.
// Only for backwards compatibility with existing code.
bool _btInUse_default(void) {
  return false;
}

// DEPRECATED: Remove this function in v4.0. Use btClassicInUse() and bleInUse() instead.
// Only for backwards compatibility with existing code.
__attribute__((weak, alias("_btInUse_default"))) bool btInUse(void);

// Default behavior: release BTDM memory (~36KB) unless a BT library is used or user overrides.
// BT libraries include esp32-hal-alloc-ble-mem.h and esp32-hal-alloc-bt-classic-mem.h which set
// _bleLibraryInUse and _btClassicLibraryInUse to true via constructor.
// Users can also provide their own strong *InUse() implementation.
__attribute__((weak)) bool btClassicInUse(void) {
  return _btClassicLibraryInUse;
}

__attribute__((weak)) bool bleInUse(void) {
  return _bleLibraryInUse;
}

#include "esp_bt.h"

#ifdef CONFIG_BTDM_CONTROLLER_MODE_BTDM
#define BT_MODE ESP_BT_MODE_BTDM
#elif defined(CONFIG_BTDM_CONTROLLER_MODE_BR_EDR_ONLY)
#define BT_MODE ESP_BT_MODE_CLASSIC_BT
#else
#define BT_MODE ESP_BT_MODE_BLE
#endif

static void _btUpdateMemReleasedFlags(esp_bt_mode_t mode);
static esp_bt_mode_t _btAvailableMemForMode(esp_bt_mode_t mode);

// Convert a bt_mode value to the corresponding ESP-IDF mode bitmask.
static esp_bt_mode_t _btModeToEspMode(bt_mode mode) {
  switch (mode) {
    case BT_MODE_BLE:        return ESP_BT_MODE_BLE;
    case BT_MODE_CLASSIC_BT: return ESP_BT_MODE_CLASSIC_BT;
    case BT_MODE_BTDM:
    case BT_MODE_DEFAULT:
    default:                 return ESP_BT_MODE_BTDM;
  }
}

bool btStarted() {
  return (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED);
}

bool btStart() {
  return btStartMode(BT_MODE);
}

bool btStartMode(bt_mode mode) {
  esp_bt_mode_t esp_bt_mode;
  esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
#if CONFIG_SOC_BT_CLASSIC_SUPPORTED
  switch (mode) {
    case BT_MODE_BLE:        esp_bt_mode = ESP_BT_MODE_BLE; break;
    case BT_MODE_CLASSIC_BT: esp_bt_mode = ESP_BT_MODE_CLASSIC_BT; break;
    case BT_MODE_BTDM:       esp_bt_mode = ESP_BT_MODE_BTDM; break;
    default:                 esp_bt_mode = BT_MODE; break;
  }
  // esp_bt_controller_enable(MODE) This mode must be equal as the mode in “cfg” of esp_bt_controller_init().
  cfg.mode = esp_bt_mode;
  if (cfg.mode == ESP_BT_MODE_CLASSIC_BT) {
    btMemRelease(BT_MODE_BLE);
  }
#else
  // other esp variants dont support BT-classic / DM.
  esp_bt_mode = BT_MODE;
#endif

  // If any memory required for this mode has already been released,
  // esp_bt_controller_init() will crash — refuse the request gracefully.
  // For BTDM mode, gracefully downgrade to whichever sub-mode still has
  // memory (e.g. BLE-only when Classic memory was released by initArduino).
  esp_bt_mode_t available = _btAvailableMemForMode(esp_bt_mode);
  if (available != esp_bt_mode) {
#if CONFIG_SOC_BT_CLASSIC_SUPPORTED
    if (esp_bt_mode == ESP_BT_MODE_BTDM && available != ESP_BT_MODE_IDLE) {
      esp_bt_mode = available;
      cfg.mode = esp_bt_mode;
    } else
#endif
    {
      return false;
    }
  }

  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
    return true;
  }
  esp_err_t ret;
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
    if ((ret = esp_bt_controller_init(&cfg)) != ESP_OK) {
      log_e("initialize controller failed: %s", esp_err_to_name(ret));
      return false;
    }
    while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {}
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
    if ((ret = esp_bt_controller_enable(esp_bt_mode)) != ESP_OK) {
      log_e("BT Enable mode=%d failed %s", BT_MODE, esp_err_to_name(ret));
      return false;
    }
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
    return true;
  }
  log_e("BT Start failed");
  return false;
}

bool btStop() {
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
    return true;
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
    if (esp_bt_controller_disable()) {
      log_e("BT Disable failed");
      return false;
    }
    while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED);
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
    if (esp_bt_controller_deinit()) {
      log_e("BT deint failed");
      return false;
    }
    vTaskDelay(1);
    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_IDLE) {
      return false;
    }
    return true;
  }
  log_e("BT Stop failed");
  return false;
}

bool btMemReleased(bt_mode mode) {
  switch (mode) {
    case BT_MODE_BLE:        return _bleMemReleased;
    case BT_MODE_CLASSIC_BT: return _classicMemReleased;
    case BT_MODE_BTDM:
    case BT_MODE_DEFAULT:
    default:                 return _bleMemReleased && _classicMemReleased;
  }
}

void btMarkMemReleased(bt_mode mode) {
  _btUpdateMemReleasedFlags(_btModeToEspMode(mode));
}

// Update tracking flags based on an ESP-IDF BT mode bitmask.
static void _btUpdateMemReleasedFlags(esp_bt_mode_t mode) {
  if (mode & ESP_BT_MODE_BLE) {
    _bleMemReleased = true;
  }
  if (mode & ESP_BT_MODE_CLASSIC_BT) {
    _classicMemReleased = true;
  }
}

// Return the subset of `mode` bits whose memory has not yet been released.
//
// Each bit in the ESP-IDF `esp_bt_mode_t` bitmask represents an independent BT
// sub-system (BLE, Classic, or both). Once memory for a sub-system has been
// freed via esp_bt_mem_release() or esp_bt_controller_mem_release(), attempting
// to use it again (e.g. by passing the same bits to a real release function or
// to the controller initializer) will cause an ESP-IDF error or memory
// corruption.
//
// This helper strips out any bits that are already tracked as released, so
// callers only ever act on bits that are still backed by physical memory.
//
// @param mode  Bitmask of BT mode bits to query (e.g. ESP_BT_MODE_BLE,
//              ESP_BT_MODE_CLASSIC_BT, or ESP_BT_MODE_BTDM).
// @return      The subset of `mode` bits whose memory is still available.
//              Returns 0 (ESP_BT_MODE_IDLE) if all requested memory has already
//              been released.
static esp_bt_mode_t _btAvailableMemForMode(esp_bt_mode_t mode) {
  if (_bleMemReleased) {
    mode = (esp_bt_mode_t)(mode & ~ESP_BT_MODE_BLE);
  }
  if (_classicMemReleased) {
    mode = (esp_bt_mode_t)(mode & ~ESP_BT_MODE_CLASSIC_BT);
  }
  return mode;
}

// ============================================================================
// Linker --wrap intercepts for esp_bt_mem_release /
//                               esp_bt_controller_mem_release
// ============================================================================
//
// WHY THIS IS NEEDED
// ------------------
// ESP-IDF's BT memory-release functions can be called by code outside of
// Arduino (most notably the Matter stack, via
//   connectedhomeip/.../bluedroid/BLEManagerImpl::InitESPBleLayer(), which
// calls esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)).
// Without interception, those external calls would free memory that our
// tracking flags do not know about, making btMemReleased() return stale
// values.  Worse, a later call to btMemRelease() could try to release memory
// that is already gone, causing an ESP-IDF error or memory corruption.
//
// HOW --wrap WORKS
// ----------------
// The GNU linker --wrap=<symbol> flag redirects every reference to <symbol>
// in the final link to __wrap_<symbol> instead, and makes the original
// function available as __real_<symbol>.  The flags are added in platform.txt:
//
//   -Wl,--wrap=esp_bt_mem_release
//   -Wl,--wrap=esp_bt_controller_mem_release
//
// This works because IDF component libraries ship as independent static
// archives (.a files, built by esp32-arduino-lib-builder) whose object files
// carry unresolved cross-archive references.  Those references are resolved
// only at the final Arduino sketch link step, so --wrap is applied in time.
//
// WHY IT WORKS WITH esp-matter
// ----------------------------
// The connectedhomeip chip library (libespressif__esp_matter.a / libCHIP.a)
// is also shipped as a plain static archive with no partial linking (ld -r).
// The call to esp_bt_controller_mem_release inside BLEManagerImpl.o therefore
// remains an unresolved external reference until the final sketch link, where
// our --wrap flag redirects it to __wrap_esp_bt_controller_mem_release.
//
// DOUBLE-RELEASE GUARD
// --------------------
// Each wrapper first strips out any mode bits that are already tracked as
// released.  If nothing remains to release it returns ESP_OK immediately,
// turning a would-be double-release into a harmless no-op.  If there are
// unreleased bits it calls the real function for only those bits, then
// updates the tracking flags on success.
//
// WHAT COULD BREAK THIS MECHANISM
// --------------------------------
// 1. Partial linking (ld -r / --relocatable):
//    If a future version of esp32-arduino-lib-builder or esp-matter merges
//    the BLEManagerImpl object directly into the bt library via a partial link
//    step, the call to esp_bt_controller_mem_release would be resolved inside
//    the partially-linked archive before the final link, bypassing --wrap.
//    Monitor both repos for any ld -r / OBJECT_LIBRARY / link_whole usage
//    that touches BLEManagerImpl.
//
// 2. Inlining or LTO:
//    If link-time optimization (LTO) is ever enabled across the IDF libraries,
//    the linker may inline esp_bt_controller_mem_release directly at the
//    call site before --wrap runs, bypassing the intercept.  LTO across
//    prebuilt static archives is currently not used in this SDK.
//
// 3. Direct ROM calls:
//    If Espressif were to move esp_bt_mem_release or
//    esp_bt_controller_mem_release into ROM (as some boot/sys functions are),
//    the call would resolve against the ROM symbol, not the RAM copy, and
//    --wrap would not intercept it.  This has not happened as of ESP-IDF 5.x,
//    but check the ROM symbol map when upgrading IDF major versions.
//
// 4. dlopen / dynamic loading:
//    --wrap is a static-link-time mechanism.  Any code loaded dynamically at
//    runtime that calls the ESP-IDF functions directly would bypass it.
//    The ESP32 does not support dynamic loading in normal Arduino builds, so
//    this is not currently a concern.
// ============================================================================

extern esp_err_t __real_esp_bt_mem_release(esp_bt_mode_t mode);
esp_err_t __wrap_esp_bt_mem_release(esp_bt_mode_t mode) {
  mode = _btAvailableMemForMode(mode);
  if (!mode) {
    return ESP_OK;
  }
  esp_err_t ret = __real_esp_bt_mem_release(mode);
  if (ret == ESP_OK) {
    _btUpdateMemReleasedFlags(mode);
  }
  return ret;
}

extern esp_err_t __real_esp_bt_controller_mem_release(esp_bt_mode_t mode);
esp_err_t __wrap_esp_bt_controller_mem_release(esp_bt_mode_t mode) {
  mode = _btAvailableMemForMode(mode);
  if (!mode) {
    return ESP_OK;
  }
  esp_err_t ret = __real_esp_bt_controller_mem_release(mode);
  if (ret == ESP_OK) {
    _btUpdateMemReleasedFlags(mode);
  }
  return ret;
}

bool btMemRelease(bt_mode mode) {
#if CONFIG_BT_CONTROLLER_ENABLED
  esp_bt_mode_t esp_mode = _btAvailableMemForMode(_btModeToEspMode(mode));
  if (!esp_mode) {
    return true;  // Already released
  }

  // esp_bt_mem_release() releases both the controller memory and the host stack
  // memory for the given mode. It is a superset of esp_bt_controller_mem_release()
  // and is the correct API to use here to maximize freed memory.
  // The --wrap intercept above will update the tracking flags on success.
  esp_err_t ret = esp_bt_mem_release(esp_mode);
  if (ret != ESP_OK) {
    log_e("BT memory release failed: %s", esp_err_to_name(ret));
    return false;
  }

  return true;
#else
  return false;
#endif
}

#else  // !__has_include("esp_bt.h") || !(defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED))
bool btStarted() {
  return false;
}

bool btStart() {
  return false;
}

bool btStop() {
  return false;
}

bool btMemRelease(bt_mode mode) {
  return true;
}

bool btMemReleased(bt_mode mode) {
  return true;
}

void btMarkMemReleased(bt_mode mode) {}

#endif /* !__has_include("esp_bt.h") || !(defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)) */

#endif /* SOC_BT_SUPPORTED */
