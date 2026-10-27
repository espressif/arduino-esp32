/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "core/BLEGuards.h"
#if BLE_BLUEDROID

#include "advertising/BLEAdvertising.h"
#include "BTAddress.h"
#include "core/BLESync.h"
#include "core/BLEAdmission.h"

#include <esp_gap_ble_api.h>
#include <esp_timer.h>
#include <freertos/semphr.h>
#include <atomic>
#include <vector>

// BLEAdvertising is entirely Bluedroid-specific (no cross-backend shared state), so
// BLEAdvertising::Impl is defined directly here (a .bluedroid.* file) with no common base.
struct BLEAdvertising::Impl {
  BLEAdmissionGate nativeAdmission;
  std::atomic<bool> isAdvertising{false};
  BLESync advSync{"advSync"};
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();
  SemaphoreHandle_t callbackMtx = xSemaphoreCreateRecursiveMutex();
  BLEAdvertising::CompleteHandler onCompleteCb = nullptr;
  esp_timer_handle_t durationTimer = nullptr;
  std::atomic<bool> durationExpiryStop{false};

  ~Impl() {
    if (durationTimer) {
      esp_timer_stop_blocking(durationTimer, portMAX_DELAY);
      esp_timer_delete(durationTimer);
    }
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
    if (callbackMtx) {
      vSemaphoreDelete(callbackMtx);
    }
  }

  std::vector<BLEUUID> serviceUUIDs;
  String name;
  bool scanResp = true;
  BLEAdvType advType = BLEAdvType::ConnectableScannable;
  BTAddress directedPeer;
  uint16_t minInterval = 0x20;
  uint16_t maxInterval = 0x40;
  uint16_t minPreferred = 0;
  uint16_t maxPreferred = 0;
  bool includeTxPower = false;
  uint16_t appearance = 0;
  bool scanRequestWhitelistOnly = false;
  bool connectWhitelistOnly = false;

  bool customAdvData = false;
  bool customScanRespData = false;
  std::vector<uint8_t> rawAdvData;
  std::vector<uint8_t> rawScanRespData;

  static BLEAdvertising::Impl *s_instance;
  static void durationTimerCb(void *arg);
  static void handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

  // Which GAP complete may give() advSync. Unmatched stop/clear completes must
  // not release a waiter for a different command, and must not clear
  // isAdvertising / periodicAdvRunning (a leftover stop after start would
  // leave the radio up while stop() no-ops).
  enum class AdvWait : uint8_t {
    None,
    LegacyStop,
    ExtStop,
    ExtClear,
    PeriodicStop
  };
  std::atomic<AdvWait> advWait{AdvWait::None};
  void armAdvWait(AdvWait event);
  bool giveIfAdvWait(AdvWait event, BTStatus status);
  BTStatus finishAdvWait();

#if BLE5_SUPPORTED
  // --- BLE5 extended advertising (TX) per-instance state ---
  // The public "legacy" start()/stop() run over the extended API on a reserved
  // instance (kLegacyInstance) so an ext-adv-enabled controller actually radiates
  // them; user extended sets use instances below it. Compiled only on BLE5-capable
  // Bluedroid silicon (BLE5_SUPPORTED).
  struct ExtInstance {
    bool slotInitialized = false;
    // Cleared whenever a setter mutates params; ensureExtConfigured() re-applies
    // them to the controller on the next data/start call and sets it true again.
    bool extAdvRegistered = false;
    std::atomic<bool> isAdvertising{false};
    esp_ble_gap_ext_adv_params_t params{};
#if BLE_PERIODIC_ADV_SUPPORTED
    bool periodicAdvRegistered = false;
    std::atomic<bool> periodicAdvRunning{false};
    esp_ble_gap_periodic_adv_params_t periodicParams{};
#endif
    void reset() {
      slotInitialized = false;
      extAdvRegistered = false;
      isAdvertising.store(false);
      params = {};
#if BLE_PERIODIC_ADV_SUPPORTED
      periodicAdvRegistered = false;
      periodicAdvRunning.store(false);
      periodicParams = {};
#endif
    }
  };
  static constexpr uint8_t kMaxExtInstances = 2;  // instance 0 + reserved legacy set
  static constexpr uint8_t kLegacyInstance = kMaxExtInstances - 1;
  ExtInstance extInstances[kMaxExtInstances];

  // Per-instance slot (lazily initialized), or nullptr when @p instance is the
  // reserved legacy set or out of range.
  ExtInstance *extInstanceAt(uint8_t instance);
  // Push accumulated params to the controller only when dirty, then wait for
  // completion. Idempotent no-op (returns OK) when already configured.
  BTStatus ensureExtConfigured(uint8_t instance);
  // Configure + set data + start the reserved legacy-PDU instance. Blocking.
  BTStatus startLegacyViaExtAdv(uint32_t durationMs);
  // Stop every extended/periodic set, wait for the async completions, then
  // HCI_LE_Clear_Advertising_Sets. Clear is Command Disallowed while advertising
  // or periodic advertising is still enabled on any set (Vol 4, Part E, §7.8.60).
  void stopAllExtendedThenClear();
#if BLE_PERIODIC_ADV_SUPPORTED
  // Push periodic params when dirty; waits on advSync.
  BTStatus ensurePeriodicConfigured(uint8_t instance);
#endif
#endif
};

#endif /* BLE_BLUEDROID */
