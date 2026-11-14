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

#include "scan/BLEScanImpl.h"

#include <esp_gap_ble_api.h>
#include <esp_timer.h>

#include <atomic>

struct BLEScan::Impl : BLEScanImplCommon {
  BLESync paramSync{"scanParamSync"};
  BLESync stopSync{"scanStopSync"};

  // Legacy Bluedroid scanning takes whole seconds; a sub-second or fractional
  // duration is run as a continuous scan plus this one-shot timer (created
  // lazily by start()). timerExpiryStop is set by the timer so the resulting
  // SCAN_STOP_COMPLETE dispatches onComplete like a natural duration expiry.
  esp_timer_handle_t durationTimer = nullptr;
  std::atomic<bool> timerExpiryStop{false};

  ~Impl() {
    if (durationTimer) {
      esp_timer_stop(durationTimer);
      esp_timer_delete(durationTimer);
    }
  }

  static void durationTimerCb(void *arg);

  static BLEScan::Impl *s_instance;
  static void handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  static BTStatus setPeriodicSyncTransferParams(uint16_t connHandle, const BTAddress &peerAddress, uint8_t mode, uint16_t skipCount, uint16_t timeoutMs);
  static BTStatus receivePeriodicSyncFromPeer(uint16_t connHandle, const BTAddress &peerAddress, uint16_t skipCount, uint16_t timeoutMs);
};

#endif /* BLE_BLUEDROID */
