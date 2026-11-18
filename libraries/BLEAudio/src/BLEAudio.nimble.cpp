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
 * @file BLEAudio.nimble.cpp
 * @brief Single NimBLE backend unit for `BLEAudio`:
 *  1. `BLEAudio::Impl` NimBLE methods (`BLEClient` central-connect fallback).
 *  2. Weak hooks consumed by core `BLE` (`core/BLEAudioHooks.h`).
 *  3. Pure-C host glue hooks (`bleAudioHost*`, `bleAudioEngine*`) consumed by `src/engine/`.
 */

#include "core/BLEGuards.h"
#if BLE_NIMBLE && BLE_AUDIO_SUPPORTED

#include "BLE.h"
#include "BLE.nimble.h"
#include "BLEAudio.nimble.h"
#include "core/BLEAudioHooks.h"
#include "core/BLEMutex.h"
#include "engine/BLEAudioEngine.h"
#if BLE_GATT_SERVER_SUPPORTED
#include "gatt/BLEGattDatabase.nimble.h"
#endif
#include "scan/BLEScan.h"
#include "esp32-hal-log.h"

#include <algorithm>
#include <cstring>
#include <errno.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <services/gap/ble_svc_gap.h>

extern "C" void esp_ble_audio_gatt_app_post_event(uint8_t type, void *param);

namespace {

bool isInactiveClient(const std::shared_ptr<BLEClient> &client) {
  return !client || !client->isConnected();
}

}  // namespace

// ==========================================================================
// 1. BLEAudio::Impl NimBLE methods
// ==========================================================================

void BLEAudio::Impl::backendOnControllerCreated(BLEClass::Impl &) {}

BTStatus BLEAudio::Impl::backendConnect(const BTAddress &address) {
  auto client = std::make_shared<BLEClient>(BLE.createClient());
  BTStatus st = client->connectAsync(address);
  if (st) {
    BLELockGuard lock(mtx.get());
    clients.erase(std::remove_if(clients.begin(), clients.end(), isInactiveClient), clients.end());
    clients.push_back(client);
  }
  return st;
}

void BLEAudio::Impl::backendPreEndTeardown() {
  std::vector<std::shared_ptr<BLEClient>> localClients;
  {
    BLELockGuard lock(mtx.get());
    localClients.swap(clients);
  }
  for (auto &client : localClients) {
    if (client) {
      (void)client->cancelConnect();
      if (client->isConnected()) {
        (void)client->disconnect();
      }
    }
  }
}

void BLEAudio::Impl::backendOnAclDisconnected(uint16_t connHandle) {
  // Called from BLEAudioImplCommon::trackLink without the controller lock;
  // take it here: connect() and pre-end teardown touch the same vector.
  auto matchesDisconnected = [connHandle](const std::shared_ptr<BLEClient> &client) {
    return !client || !client->isConnected() || client->getHandle() == connHandle;
  };
  BLELockGuard lock(mtx.get());
  clients.erase(std::remove_if(clients.begin(), clients.end(), matchesDisconnected), clients.end());
}

void BLEAudio::Impl::backendResetLinks() {
  clients.clear();
}

// ==========================================================================
// 2. Weak hooks consumed by core BLE (core/BLEAudioHooks.h)
// ==========================================================================

extern "C" void bleAudioForwardHostGattEvent(struct ble_gap_event *event) {
  if (event == nullptr || !bleAudioEngineIsInitialized()) {
    return;
  }
  switch (event->type) {
    case BLE_GAP_EVENT_MTU:
    case BLE_GAP_EVENT_NOTIFY_RX:
    case BLE_GAP_EVENT_NOTIFY_TX:
    case BLE_GAP_EVENT_SUBSCRIBE: esp_ble_audio_gatt_app_post_event(static_cast<uint8_t>(event->type), event); break;
    default:                      break;
  }
}

extern "C" bool bleAudioGattActive() {
  return bleAudioEngineIsInitialized();
}

extern "C" bool bleAudioGattFrozen() {
  return bleAudioEngineIsStarted();
}

// ==========================================================================
// 3. Engine C host hooks (engine/BLEAudioEngine.h)
// ==========================================================================

extern "C" int bleAudioHostSecureLink(uint16_t conn_handle, const uint8_t[6], uint8_t) {
  int rc = ble_gap_security_initiate(conn_handle);
  return rc == BLE_HS_EALREADY ? 0 : rc;
}

extern "C" int bleAudioHostExchangeMtu(uint16_t conn_handle) {
  int rc = ble_gattc_exchange_mtu(conn_handle, nullptr, nullptr);
  return rc == BLE_HS_EALREADY ? 0 : rc;
}

extern "C" int bleAudioHostOpenLink(uint8_t, const uint8_t[6]) {
  // NimBLE central links are opened via BLEAudio::Impl::backendConnect().
  return ESP_ERR_NOT_SUPPORTED;
}

extern "C" void bleAudioAddrFromGap(uint8_t out[6], const uint8_t in[6]) {
  memcpy(out, in, 6);
}

extern "C" int bleAudioEngineDisconnect(uint16_t conn_handle) {
  int rc = ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
  if (rc == 0 || rc == BLE_HS_EALREADY) {
    return 0;
  }
  if (rc == BLE_HS_ENOTCONN) {
    return static_cast<int>(BTStatus::NotConnected);
  }
  log_e("audio link: terminate handle=%u rc=%d", (unsigned)conn_handle, rc);
  return static_cast<int>(BTStatus::Fail);
}

extern "C" int bleAudioHostPrepareInit(void) {
#if BLE_GATT_SERVER_SUPPORTED
  String name = BLE.getDeviceName();
  if (name.length() > 0) {
    (void)ble_svc_gap_device_name_set(name.c_str());
  }
#endif
  return 0;
}

extern "C" int bleAudioHostPrepareGatt(void) {
#if BLE_GATT_SERVER_SUPPORTED
  BTStatus status = BLEGattDatabase::prepareAudioCommit();
  return status ? 0 : static_cast<int>(status);
#else
  return 0;
#endif
}

extern "C" void bleAudioHostFinishGatt(bool success) {
#if BLE_GATT_SERVER_SUPPORTED
  BLEGattDatabase::finishAudioCommit(success);
#else
  (void)success;
#endif
}

extern "C" void bleAudioHostResetGatt(void) {
#if BLE_GATT_SERVER_SUPPORTED
  BLEGattDatabase::resetAudioStaging();
#endif
}

// ==========================================================================
// 4. Periodic Advertising Sync Transfer (PAST) hooks & feature stubs
// ==========================================================================

extern "C" int bleAudioEngineReceivePeriodicSync(uint16_t conn_handle, uint8_t peer_addr_type, const uint8_t peer_addr[6], uint16_t skip, uint16_t timeout_ms) {
  (void)peer_addr_type;
  if (!peer_addr) {
    return static_cast<int>(BTStatus::InvalidParam);
  }
  return static_cast<int>(BLE.getScan().receivePeriodicSync(conn_handle, skip, timeout_ms));
}

extern "C" int bleAudioEngineStopPeriodicSyncReceive(uint16_t conn_handle) {
  return static_cast<int>(BLE.getScan().stopPeriodicSyncReceive(conn_handle));
}

#if BLE_PAST_SUPPORTED

extern "C" int bleAudioEngineSendPast(uint8_t adv, uint16_t conn, uint16_t service_data, const uint8_t[6], uint8_t) {
  return ble_gap_periodic_adv_sync_set_info(adv, conn, service_data);
}

extern "C" int bleAudioEngineSendPastSync(uint16_t sync_handle, uint16_t conn, uint16_t service_data, const uint8_t[6], uint8_t) {
  return ble_gap_periodic_adv_sync_transfer(sync_handle, conn, service_data);
}

#else /* !BLE_PAST_SUPPORTED */

extern "C" int bleAudioEngineSendPast(uint8_t, uint16_t, uint16_t, const uint8_t[6], uint8_t) {
  return ESP_ERR_NOT_SUPPORTED;
}

extern "C" int bleAudioEngineSendPastSync(uint16_t, uint16_t, uint16_t, const uint8_t[6], uint8_t) {
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* BLE_PAST_SUPPORTED */

#endif /* BLE_NIMBLE && BLE_AUDIO_SUPPORTED */
