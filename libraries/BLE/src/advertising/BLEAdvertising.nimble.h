/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
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
#if BLE_NIMBLE

#include "advertising/BLEAdvertising.h"
#include "BTAddress.h"

#include <host/ble_gap.h>
#include "core/BLEMutex.h"
#include "core/BLEAdmission.h"
#include <atomic>
#include <vector>

// BLEAdvertising is entirely NimBLE-specific (no cross-backend shared state), so
// BLEAdvertising::Impl is defined directly here (a .nimble.* file) with no common base.
struct BLEAdvertising::Impl {
  BLEAdmissionGate nativeAdmission;
  std::atomic<bool> isAdvertising{false};
  // Duration of the current legacy session, kept so a stack-preempted set
  // (ADV_COMPLETE / BLE_HS_EPREEMPTED) resumes with the same limit.
  uint32_t legacyDurationMs = 0;
  int64_t sessionDeadlineUs = 0;
  // Bumped on every public stop() (even when the flag is already clear) so a
  // pending preemption-resume cannot restart a session the app canceled.
  std::atomic<uint32_t> advSession{0};
  bool scanResponseEnabled = true;
  bool includeTxPower = false;
  uint16_t appearance = 0;
  uint16_t minInterval = 0x20;
  uint16_t maxInterval = 0x40;
  uint16_t minPreferred = 0;
  uint16_t maxPreferred = 0;
  BLEAdvType advType = BLEAdvType::ConnectableScannable;
  BTAddress directedPeer;
  uint8_t filterPolicy = 0;

  std::vector<BLEUUID> serviceUUIDs;
  String deviceName;
  BLEAdvertisementData customAdvData;
  BLEAdvertisementData customScanRspData;
  bool useCustomAdvData = false;
  bool useCustomScanRsp = false;

  BLEAdvertising::CompleteHandler onCompleteCb = nullptr;
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  ~Impl() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  static int gapEventCallback(struct ble_gap_event *event, void *arg);

#if BLE5_SUPPORTED
  // --- BLE5 extended / periodic advertising (TX) per-instance state ---
  // Setters accumulate parameters here; the controller instance is (re)configured
  // lazily on the first data/start call after a parameter change, because NimBLE
  // requires ble_gap_ext_adv_configure() to run before set_data()/start().
  struct ExtInstance {
    bool slotInitialized = false;
    bool extAdvRegistered = false;
    std::atomic<bool> isAdvertising{false};
    // Session limits of the current start, for preempt resume (see Impl).
    uint32_t durationMs = 0;
    int64_t sessionDeadlineUs = 0;
    uint8_t maxEvents = 0;
    struct ble_gap_ext_adv_params params {};
#if BLE_PERIODIC_ADV_SUPPORTED
    bool periodicAdvRegistered = false;
    struct ble_gap_periodic_adv_params periodicParams {};
#endif
    void reset() {
      slotInitialized = false;
      extAdvRegistered = false;
      isAdvertising.store(false);
      durationMs = 0;
      sessionDeadlineUs = 0;
      maxEvents = 0;
      params = {};
#if BLE_PERIODIC_ADV_SUPPORTED
      periodicAdvRegistered = false;
      periodicParams = {};
#endif
    }
  };
#if defined(CONFIG_BT_NIMBLE_MAX_EXT_ADV_INSTANCES)
  static constexpr uint8_t kMaxExtInstances = CONFIG_BT_NIMBLE_MAX_EXT_ADV_INSTANCES + 1;
#else
  static constexpr uint8_t kMaxExtInstances = 2;  // instance 0 + one extended set
#endif
  // On BLE5 builds the public "legacy" start()/stop() run over the extended
  // controller (the only path the ext-adv-enabled controller actually radiates).
  // The top controller instance is reserved for that
  // legacy-PDU set so it can broadcast concurrently with user extended sets
  // (setExt*/startExtended, which are clamped to instances below it). On S3
  // (kMaxExtInstances == 2) this leaves instance 0 for one user extended set.
  static constexpr uint8_t kLegacyInstance = kMaxExtInstances - 1;
  ExtInstance extInstances[kMaxExtInstances];

  // Returns the per-instance slot (lazily initialized with sane defaults), or
  // nullptr when @p instance is out of range (>= kLegacyInstance, the reserved
  // legacy set). Marks the slot dirty so the next data/start call reconfigures
  // the controller.
  ExtInstance *extInstanceAt(uint8_t instance);
  // Pushes the accumulated params to the controller (ble_gap_ext_adv_configure).
  // Returns a NimBLE rc (0 == success). Idempotent: skips when already configured.
  int extEnsureConfigured(uint8_t instance);
  // Configure + set data + start the reserved legacy-PDU instance over the
  // extended controller API. Returns a NimBLE rc (0 == success).
  int startLegacyViaExtAdv(uint32_t durationMs);
#endif
};

#endif /* BLE_NIMBLE */
