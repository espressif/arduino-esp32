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

#include "scan/BLEScanImpl.h"

#include <host/ble_gap.h>

#include <vector>

struct BLEScan::Impl : BLEScanImplCommon {
  struct EventContext {
    BLEScan::Impl *impl;
    uint32_t generation;
    bool scanSession;
    bool retireByStop = false;  // Protected by the owning Impl::mtx.
  };

  uint32_t lastDurationMs = 0;
  // Absolute session deadline. A controller preemption resumes only for the
  // remaining interval instead of starting lastDurationMs again. Zero denotes
  // an unlimited scan. Protected by mtx.
  int64_t sessionDeadlineUs = 0;
  // Identifies the logical scan whose completion may update state and release
  // scanSync. Each native resume after preemption keeps the same generation;
  // a user stop/restart advances it. Protected by mtx.
  uint32_t activeGeneration = 0;
  EventContext *activeScanContext = nullptr;
  std::vector<EventContext *> retiredScanContexts;
  // stop() can receive BLE_HS_EALREADY while NimBLE has temporarily moved the
  // operation into its privacy-preemption slot. Native code still owns the
  // callback argument in that state, so keep it alive until DISC_COMPLETE or
  // until the host has fully stopped.
  std::vector<EventContext *> nativeHeldScanContexts;
  struct ble_npl_event retireScanEvent {};
  bool retireScanEventInited = false;
#if BLE5_SUPPORTED
  bool hasUncodedConfig = false;
  uint16_t uncodedInterval = 0;
  uint16_t uncodedWindow = 0;
  bool hasCodedConfig = false;
  uint16_t codedInterval = 0;
  uint16_t codedWindow = 0;
#endif
  EventContext periodicContext{this, 0, false};

  static int gapEventHandler(struct ble_gap_event *event, void *arg);

  BLEAdvertisedDevice parseDiscEvent(const struct ble_gap_disc_desc *disc);
#if BLE5_SUPPORTED
  BLEAdvertisedDevice parseExtDiscEvent(const struct ble_gap_ext_disc_desc *disc);
#endif
};

#endif /* BLE_NIMBLE */
