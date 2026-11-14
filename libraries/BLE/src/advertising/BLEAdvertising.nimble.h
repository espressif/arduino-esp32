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

#include "advertising/BLEAdvertisingImpl.h"

#include <host/ble_gap.h>
#include <atomic>

struct BLEAdvertising::Impl : BLEAdvertisingImplCommon {
  // Duration of the current legacy session, kept so a stack-preempted set
  // (ADV_COMPLETE / BLE_HS_EPREEMPTED) resumes with the same limit.
  uint32_t legacyDurationMs = 0;
  int64_t sessionDeadlineUs = 0;
  // Bumped on every public stop() (even when the flag is already clear) so a
  // pending preemption-resume cannot restart a session the app canceled.
  std::atomic<uint32_t> advSession{0};
  bool scanResponseEnabled = true;
  uint8_t filterPolicy = 0;

  String deviceName;
  BLEAdvertisementData customAdvData;
  BLEAdvertisementData customScanRspData;
  bool useCustomAdvData = false;
  bool useCustomScanRsp = false;

  static int gapEventCallback(struct ble_gap_event *event, void *arg);

#if BLE5_SUPPORTED
  struct ExtInstance {
    bool slotInitialized = false;
    bool extAdvRegistered = false;
    std::atomic<bool> isAdvertising{false};
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
  static constexpr uint8_t kMaxExtInstances = BLE_MAX_EXT_ADV_INSTANCES;
  static constexpr uint8_t kLegacyInstance = kMaxExtInstances - 1;
  ExtInstance extInstances[kMaxExtInstances];

  ExtInstance *extInstanceAt(uint8_t instance);
  int extEnsureConfigured(uint8_t instance);
  int startLegacyViaExtAdv(uint32_t durationMs);
#endif
};

#endif /* BLE_NIMBLE */
