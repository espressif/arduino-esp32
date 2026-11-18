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

#include "advertising/BLEAdvertisingImpl.h"
#include "core/BLESync.h"

#include <esp_gap_ble_api.h>
#include <esp_timer.h>
#include <freertos/semphr.h>
#include <atomic>
#include <vector>

struct BLEAdvertising::Impl : BLEAdvertisingImplCommon {
  BLESync advSync{"advSync"};
  SemaphoreHandle_t callbackMtx = xSemaphoreCreateRecursiveMutex();
  esp_timer_handle_t durationTimer = nullptr;
  std::atomic<bool> durationExpiryStop{false};

  ~Impl() {
    if (durationTimer) {
      esp_timer_stop_blocking(durationTimer, portMAX_DELAY);
      esp_timer_delete(durationTimer);
    }
    if (callbackMtx) {
      vSemaphoreDelete(callbackMtx);
    }
  }

  String name;
  bool scanResp = true;
  bool scanRequestWhitelistOnly = false;
  bool connectWhitelistOnly = false;

  bool customAdvData = false;
  bool customScanRespData = false;
  std::vector<uint8_t> rawAdvData;
  std::vector<uint8_t> rawScanRespData;

  static BLEAdvertising::Impl *s_instance;
  static void durationTimerCb(void *arg);
  static void handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

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
  struct ExtInstance {
    bool slotInitialized = false;
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
  static constexpr uint8_t kMaxExtInstances = BLE_MAX_EXT_ADV_INSTANCES;  // instance 0 + reserved legacy set
  static constexpr uint8_t kLegacyInstance = kMaxExtInstances - 1;
  ExtInstance extInstances[kMaxExtInstances];

  ExtInstance *extInstanceAt(uint8_t instance);
  BTStatus ensureExtConfigured(uint8_t instance);
  BTStatus applySetRandomAddr(uint8_t instance);
  BTStatus startLegacyViaExtAdv(uint32_t durationMs);
  void stopAllExtendedThenClear();
#if BLE_PERIODIC_ADV_SUPPORTED
  BTStatus ensurePeriodicConfigured(uint8_t instance);
#endif
#endif
};

#endif /* BLE_BLUEDROID */
