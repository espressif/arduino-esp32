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

/**
 * @file BLEGattDatabase.nimble.cpp
 * @brief NimBLE implementation of the unified GATT registration coordinator.
 */

#include "core/BLEGuards.h"
#if BLE_NIMBLE

#include "gatt/BLEGattDatabase.h"

#if BLE_GATT_SERVER_SUPPORTED

#include "BLE.h"
#include "server/BLEServer.nimble.h"
#include "gatt/BLEGattAttributes.nimble.h"
#include "gatt/BLECharacteristicValidation.h"
#include "esp32-hal-log.h"

#if BLE_AUDIO_SUPPORTED
#include "audio/BLEAudioEngine.h"
#endif

#include <host/ble_hs.h>
#include <host/ble_att.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

namespace BLEGattDatabase {

namespace {
BLEServer::Impl *sAudioServer = nullptr;
bool sAudioDefsAdded = false;

std::vector<std::shared_ptr<BLEService::Impl>> snapshotServices(BLEServer::Impl &impl) {
  BLELockGuard lock(impl.mtx);
  return impl.services;
}

bool validateServices(BLEServer::Impl &impl, const std::vector<std::shared_ptr<BLEService::Impl>> &services) {
  for (const auto &svc : services) {
    std::vector<std::shared_ptr<BLECharacteristic::Impl>> chrs;
    {
      BLELockGuard lock(impl.mtx);
      chrs = svc->characteristics;
    }
    for (const auto &chr : chrs) {
      if (!bleValidateCharForRegistration(*chr, /*stackIsNimble=*/true)) {
        log_e("GATT: characteristic %s rejected by validation", chr->uuid.toString().c_str());
        return false;
      }
    }
  }
  return true;
}

void clearRegistration(BLEServer::Impl &impl, const std::vector<std::shared_ptr<BLEService::Impl>> &services) {
  BLELockGuard lock(impl.mtx);
  impl.started = false;
  for (auto &svc : services) {
    svc->started = false;
    svc->handle = 0;
    for (auto &chr : svc->characteristics) {
      chr->handle = 0;
      BLELockGuard chrLock(chr->mtx);
      for (auto &dsc : chr->descriptors) {
        dsc->handle = 0;
      }
    }
  }
}

BTStatus addServerServices(
  BLEServer::Impl &impl, const std::vector<std::shared_ptr<BLEService::Impl>> &services
) {
  if (services.empty()) {
    nimbleClearRetainedGattDefs();
    return BTStatus::OK;
  }
  int rc = nimbleRegisterGattServices(services);
  if (rc != 0) {
    log_e("nimbleRegisterGattServices: rc=%d", rc);
    return BTStatus::Fail;
  }
  for (auto &s : services) {
    s->started = true;
  }
  return BTStatus::OK;
}

void applyDeviceName() {
  String name = BLE.getDeviceName();
  if (name.length() > 0) {
    ble_svc_gap_device_name_set(name.c_str());
  }
}
}  // namespace

bool audioModeActive() {
  // Single-sourced from the engine's own lifecycle: audio mode is exactly the
  // window between audio.begin() (common_init) and audio.end(). This keeps the
  // LE Audio engine free of any GATT-coordinator (NimBLE-only) knowledge.
#if BLE_AUDIO_SUPPORTED
  return bleAudioEngineIsInitialized();
#else
  return false;
#endif
}

bool audioModeFrozen() {
#if BLE_AUDIO_SUPPORTED
  return bleAudioEngineIsStarted();
#else
  return false;
#endif
}

BTStatus stageServer(BLEServer::Impl &impl) {
  // Do not call ble_gatts_add_svcs here. NimBLE retains that call's svc_def
  // pointer, so rebuilding our vectors on a second start() would dangle the
  // first pointer and adding the complete tree again would duplicate services.
  // Keep only the contributor; audio.start() publishes its latest tree once.
  auto services = snapshotServices(impl);
  if (!validateServices(impl, services)) {
    return BTStatus::InvalidState;
  }
  if (sAudioDefsAdded) {
    log_w("GATT: audio commit preparation is already in progress");
    return BTStatus::InvalidState;
  }
  if (sAudioServer != nullptr && sAudioServer != &impl) {
    log_e("GATT: only one BLEServer may contribute to the audio database");
    return BTStatus::InvalidState;
  }
  sAudioServer = &impl;
  for (auto &svc : services) {
    svc->started = true;
  }
  applyDeviceName();
  impl.started = true;
  return BTStatus::OK;
}

BTStatus prepareAudioCommit() {
  if (sAudioServer == nullptr || sAudioDefsAdded) {
    return BTStatus::OK;
  }
  auto services = snapshotServices(*sAudioServer);
  if (!validateServices(*sAudioServer, services)) {
    return BTStatus::InvalidState;
  }
  BTStatus st = addServerServices(*sAudioServer, services);
  if (st != BTStatus::OK) {
    clearRegistration(*sAudioServer, services);
    return st;
  }
  sAudioDefsAdded = true;
  return BTStatus::OK;
}

void finishAudioCommit(bool success) {
  if (sAudioServer == nullptr) {
    return;
  }
  auto services = snapshotServices(*sAudioServer);
  if (!success) {
    BLELockGuard lock(sAudioServer->mtx);
    sAudioServer->started = false;
    return;
  }
  for (auto &svc : services) {
    if (ble_gatts_find_svc(&svc->nimbleUUID.u, &svc->handle) != 0) {
      svc->handle = 0;
    }
  }
  sAudioServer->started = true;
}

void resetAudioStaging() {
  if (sAudioServer != nullptr) {
    clearRegistration(*sAudioServer, snapshotServices(*sAudioServer));
  }
  sAudioServer = nullptr;
  sAudioDefsAdded = false;
  nimbleClearRetainedGattDefs();
}

BTStatus commitServerStandalone(BLEServer::Impl &impl) {
  // Classic single-owner path (no LE Audio): full rebuild + commit. This is
  // the behaviour the previous nimbleRebuildGattDatabase() provided.
  auto services = snapshotServices(impl);
  if (!validateServices(impl, services)) {
    return BTStatus::InvalidState;
  }
  int rc = ble_gatts_reset();
  if (rc != 0) {
    return rc == BLE_HS_EBUSY ? BTStatus::Busy : BTStatus::Fail;
  }
  ble_svc_gap_init();
  ble_svc_gatt_init();

  BTStatus st = addServerServices(impl, services);
  if (st != BTStatus::OK) {
    clearRegistration(impl, services);
    return st;
  }

  rc = ble_gatts_start();
  if (rc != 0) {
    log_e("ble_gatts_start: rc=%d", rc);
    clearRegistration(impl, services);
    return BTStatus::Fail;
  }

  for (auto &svc : services) {
    if (ble_gatts_find_svc(&svc->nimbleUUID.u, &svc->handle) != 0) {
      svc->handle = 0;
    }
  }

  applyDeviceName();
  impl.started = true;
  return BTStatus::OK;
}

}  // namespace BLEGattDatabase

#endif /* BLE_GATT_SERVER_SUPPORTED */
#endif /* BLE_NIMBLE */
