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
// Stage a server's services into the (already-initialised) attribute table.
// add_svcs only; no reset / init / start.
BTStatus addServerServices(BLEServer::Impl &impl) {
  if (impl.services.empty()) {
    return BTStatus::OK;
  }
  int rc = nimbleRegisterGattServices(impl.services);
  if (rc != 0) {
    log_e("nimbleRegisterGattServices: rc=%d", rc);
    return BTStatus::Fail;
  }
  for (auto &s : impl.services) {
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

BTStatus stageServer(BLEServer::Impl &impl) {
  // In audio mode the engine already ran svc_gap_init/svc_gatt_init in
  // common_init and will perform the single ble_gatts_start in common_start
  // (BLEAudio::start()). Here we only contribute the server's services.
  BTStatus st = addServerServices(impl);
  if (st != BTStatus::OK) {
    return st;
  }
  applyDeviceName();
  impl.started = true;
  return BTStatus::OK;
}

BTStatus commitServerStandalone(BLEServer::Impl &impl) {
  // Classic single-owner path (no LE Audio): full rebuild + commit. This is
  // the behaviour the previous nimbleRebuildGattDatabase() provided.
  ble_gatts_reset();
  ble_svc_gap_init();
  ble_svc_gatt_init();

  BTStatus st = addServerServices(impl);
  if (st != BTStatus::OK) {
    return st;
  }

  int rc = ble_gatts_start();
  if (rc != 0) {
    log_e("ble_gatts_start: rc=%d", rc);
    return BTStatus::Fail;
  }

  applyDeviceName();
  impl.started = true;
  return BTStatus::OK;
}

}  // namespace BLEGattDatabase

#endif /* BLE_GATT_SERVER_SUPPORTED */
#endif /* BLE_NIMBLE */
