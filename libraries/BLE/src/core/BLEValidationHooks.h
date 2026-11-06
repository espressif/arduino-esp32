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
#if BLE_VALIDATION_HOOKS

#include <atomic>
#include <cstdint>

/**
 * @file
 * @brief Test-only injectors for `tests/validation/ble/`. Not a public API.
 *
 * Compiled in only when @c BLE_VALIDATION_HOOKS is set. Production builds
 * compile this header to nothing. The singleton is defined in
 * @c BLEValidationHooks.cpp so sketches and library TUs share one object
 * instead of each sketch redeclaring the same @c extern "C" atomics.
 */
class BLEValidationHooks {
public:
  struct Lifecycle {
    std::atomic<int> endWorkerStartFailures{0};
  } lifecycle;

  struct Stream {
    std::atomic<int> workerStartFailures{0};
    std::atomic<int> teardownFailures{0};
    std::atomic<int> startFailures{0};
    std::atomic<int> removeFailures{0};
    std::atomic<int> disconnectRaces{0};
    std::atomic<bool> holdFinalRoleRelease{false};
    std::atomic<bool> finalRoleReleaseReached{false};
    std::atomic<bool> hostShutdownEntered{false};
    std::atomic<bool> holdWriteChunks{false};
    std::atomic<bool> writeChunksEntered{false};
    std::atomic<bool> writeSawClearedHandle{false};
  } stream;

  struct Gatt {
    std::atomic<bool> holdNotifyCompletions{false};
    std::atomic<bool> forceTerminalClose{false};
    std::atomic<bool> holdTerminalWorker{false};
    std::atomic<bool> terminalWorkerEntered{false};
    std::atomic<bool> terminalShutdownEntered{false};
    std::atomic<int> terminalWorkerStartFailures{0};
    std::atomic<bool> holdNotifySubmit{false};
    std::atomic<bool> notifySubmitEntered{false};
    std::atomic<int> injectRebuildFails{0};
  } gatt;

  struct Scan {
    std::atomic<bool> forcePreemptedStop{false};
    std::atomic<bool> bluedroidShutdownEntered{false};
    std::atomic<bool> holdBluedroidShutdown{false};
    std::atomic<int> cancelFailures{0};
    std::atomic<int> heldContexts{0};
    std::atomic<int> retiredContexts{0};
  } scan;

  struct Adv {
    std::atomic<bool> forcePreemptedComplete{false};
    std::atomic<bool> nativeStopKeepingFlag{false};
    std::atomic<bool> holdPreemptResume{false};
    std::atomic<bool> preemptResumeEntered{false};
    std::atomic<uint32_t> resumeDurationMs{0};
    std::atomic<bool> injectStaleExtStopComplete{false};
    std::atomic<bool> injectStaleLegacyStopComplete{false};
    std::atomic<bool> injectStalePeriodicStopComplete{false};
    std::atomic<bool> advertisingAfterStaleStop{false};
    std::atomic<bool> periodicRunningAfterStaleStop{false};
    std::atomic<bool> holdReAdvStart{false};
    std::atomic<bool> reAdvStartEntered{false};
    std::atomic<bool> holdReAdvBeforeMutex{false};
    std::atomic<bool> reAdvBeforeMutexEntered{false};
    std::atomic<bool> injectPreemptedComplete{false};
    std::atomic<bool> injectNativeStopFail{false};
    std::atomic<bool> injectExtStopFail{false};
    std::atomic<int> lastNativeStartRc{0};
  } adv;

  struct Client {
    std::atomic<bool> holdNimbleRefReset{false};
    std::atomic<bool> nimbleRefHoldEntered{false};
    std::atomic<bool> nimbleRefAssigned{false};
    std::atomic<bool> nimbleRefEmptyOnConnect{false};
    std::atomic<bool> resetWhileConnected{false};
  } client;

  struct Security {
    // When true, applySecurityParams advertises the SMP AuthReq OOB flag so a
    // peer can select legacy OOB / OOB-SC. Production builds never set this;
    // the library still rejects those methods (no public OOB TK API).
    std::atomic<bool> advertiseOob{false};
  } security;

  struct Remote {
    std::atomic<uint32_t> writeTimeoutMs{0};
    std::atomic<bool> holdWriteCommandCompletion{false};
    std::atomic<bool> releaseWriteCommandCompletion{false};
    std::atomic<bool> holdWriteResponseRetirement{false};
    std::atomic<bool> writeCommandOverlappedWaiter{false};
    std::atomic<int> writeCommandSubmitFailures{0};
    std::atomic<int> writeCommandCompletionFailures{0};
    std::atomic<bool> writeResponseStatusPreserved{false};
    std::atomic<bool> combinedWriteOwnerObserved{false};
    std::atomic<bool> combinedWriteOwnerHeld{true};
    std::atomic<bool> injectCccdWriteFail{false};
  } remote;

  static bool consumeFailure(std::atomic<int> &remaining);
  static void holdWhile(const std::atomic<bool> &flag);
};

extern BLEValidationHooks BLEHooks;

#endif /* BLE_VALIDATION_HOOKS */
