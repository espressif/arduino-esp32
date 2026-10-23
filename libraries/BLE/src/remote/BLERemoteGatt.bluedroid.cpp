/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2017 Neil Kolban
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
 * @file BLERemoteGatt.bluedroid.cpp
 * @brief Bluedroid GATT client: remote service, characteristic, and descriptor implementations.
 */

#include "core/BLEGuards.h"
#if BLE_BLUEDROID

// ==========================================================================
// BLERemoteService - remote service discovery and characteristic lookup
// ==========================================================================

#if BLE_GATT_CLIENT_SUPPORTED

#include "BLE.h"
#include "client/BLEClient.bluedroid.h"
#include "remote/BLERemoteGatt.bluedroid.h"
#include "types/BLEUUID.bluedroid.h"
#include "core/BLEImplHelpers.h"
#include "esp32-hal-log.h"

#include <esp_gattc_api.h>

// API contract is documented on the declarations in the public BLE*.h headers; the definitions below carry implementation notes only.

namespace {

struct RemoteGattTarget {
  std::shared_ptr<BLERemoteService::Impl> service;
  std::shared_ptr<BLERemoteCharacteristic::Impl> characteristic;
  std::shared_ptr<BLERemoteDescriptor::Impl> descriptor;
};

// Bluedroid's private GATTC queue has 30 entries and can reject an accepted
// BTC write later without an operation token. Keep callback-safe Write Command
// admission well below that native limit; all other public GATT procedures are
// serialized, so this leaves ample headroom for their control traffic.
static constexpr uint16_t kMaxPendingWriteCommands = 8;

bool remoteTargetConnectedLocked(const RemoteGattTarget &target, const std::shared_ptr<BLEClient::Impl> &client) {
  if (!target.service || !client || target.service->clientOwner.lock() != client || target.service->client != client.get()
      || !client->connected || client->connectionGeneration.load() != target.service->clientGeneration) {
    return false;
  }
  if (target.characteristic
      && (target.characteristic->serviceOwner.lock() != target.service || target.characteristic->service != target.service.get())) {
    return false;
  }
  return !target.descriptor
         || (target.descriptor->chrOwner.lock() == target.characteristic && target.descriptor->chr == target.characteristic.get());
}

std::shared_ptr<BLEClient::Impl> remoteClient(const RemoteGattTarget &target) {
  auto client = target.service ? target.service->clientOwner.lock() : nullptr;
  BLELockGuard lock(client ? client->mtx : nullptr);
  return remoteTargetConnectedLocked(target, client) ? client : nullptr;
}

}  // namespace

// getClient() is stack-agnostic and lives in the common src/BLERemoteService.cpp.

// Blocking: may call esp_ble_gattc_get_attr_count / esp_ble_gattc_get_all_char (synchronous attribute queries) while connected.
BLERemoteCharacteristic BLERemoteService::getCharacteristic(const BLEUUID &uuid) const {
  BLE_CHECK_IMPL(BLERemoteCharacteristic());

  // Discover characteristics if not yet done
  RemoteGattTarget target{_impl, {}, {}};
  auto client = remoteClient(target);
  BLELockGuard lock(client ? client->mtx : nullptr);
  if (!remoteTargetConnectedLocked(target, client)) {
    return BLERemoteCharacteristic();
  }
  if (!impl.characteristicsDiscovered) {
    uint16_t count = 0;
    esp_gatt_status_t st =
      esp_ble_gattc_get_attr_count(client->gattcIf, client->connId, ESP_GATT_DB_CHARACTERISTIC, impl.startHandle, impl.endHandle, 0, &count);

    if (st == ESP_GATT_OK && count > 0) {
      std::vector<esp_gattc_char_elem_t> chars(count);
      st = esp_ble_gattc_get_all_char(client->gattcIf, client->connId, impl.startHandle, impl.endHandle, chars.data(), &count, 0);

      if (st == ESP_GATT_OK) {
        for (uint16_t i = 0; i < count; i++) {
          auto cImpl = std::make_shared<BLERemoteCharacteristic::Impl>();
          cImpl->uuid = bluedroidUuidToPublic(chars[i].uuid);
          cImpl->handle = chars[i].char_handle;
          cImpl->defHandle = chars[i].char_handle;
          cImpl->properties = chars[i].properties;
          cImpl->service = _impl.get();
          cImpl->serviceOwner = _impl;
          cImpl->cacheMtx = impl.cacheMtx;
          impl.characteristics.push_back(cImpl);
        }
      }
    }
    // Only latch discovery on success; a failed query must be retryable
    // rather than permanently reporting an empty characteristic list.
    if (st == ESP_GATT_OK) {
      impl.characteristicsDiscovered = true;
    }
  }

  for (auto &c : impl.characteristics) {
    if (c->uuid == uuid) {
      return BLERemoteCharacteristic(c);
    }
  }
  return BLERemoteCharacteristic();
}

// May invoke getCharacteristic with a placeholder UUID to force the same one-time discovery.
std::vector<BLERemoteCharacteristic> BLERemoteService::getCharacteristics() const {
  std::vector<BLERemoteCharacteristic> result;
  if (!_impl) {
    return result;
  }

  RemoteGattTarget target{_impl, {}, {}};
  auto client = remoteClient(target);
  if (!_impl->characteristicsDiscovered && client) {
    (void)getCharacteristic(BLEUUID(static_cast<uint16_t>(0)));  // triggers discovery
  }

  BLELockGuard lock(client ? client->mtx : nullptr);
  if (!remoteTargetConnectedLocked(target, client)) {
    return result;
  }
  for (auto &c : _impl->characteristics) {
    result.push_back(BLERemoteCharacteristic(c));
  }
  return result;
}

#else /* !BLE_GATT_CLIENT_SUPPORTED -- stubs */

#include "esp32-hal-log.h"

// Stubs for BLE_GATT_CLIENT_SUPPORTED == 0: empty characteristic handles and lists.

BLERemoteCharacteristic BLERemoteService::getCharacteristic(const BLEUUID &) const {
  return BLERemoteCharacteristic();
}

std::vector<BLERemoteCharacteristic> BLERemoteService::getCharacteristics() const {
  return {};
}

#endif /* BLE_GATT_CLIENT_SUPPORTED */

// ==========================================================================
// BLERemoteCharacteristic - remote characteristic read/write/notify/discovery
// ==========================================================================

#if BLE_GATT_CLIENT_SUPPORTED

#include "BLE.h"
#include "client/BLEClient.bluedroid.h"
#if BLE_SMP_SUPPORTED
#include "security/BLESecurity.bluedroid.h"
#endif
#include "remote/BLERemoteGatt.bluedroid.h"
#include "types/BLEUUID.bluedroid.h"
#include "core/BLEImplHelpers.h"
#include "core/BLEMutex.h"
#include "esp32-hal-log.h"

#include <esp_gattc_api.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_defs.h>
#include <string.h>

#if defined(BLE_GATT_VALIDATION_HOOKS)
extern "C" {
extern std::atomic<uint32_t> bleRemoteTestWriteTimeoutMs;
extern std::atomic<bool> bleRemoteTestHoldWriteCommandCompletion;
extern std::atomic<bool> bleRemoteTestReleaseWriteCommandCompletion;
extern std::atomic<bool> bleRemoteTestHoldWriteResponseRetirement;
extern std::atomic<bool> bleRemoteTestWriteCommandOverlappedWaiter;
extern std::atomic<int> bleRemoteTestWriteCommandSubmitFailures;
extern std::atomic<bool> bleRemoteTestWriteResponseStatusPreserved;
}
#endif

// --------------------------------------------------------------------------
// BLERemoteCharacteristic -- Bluedroid backend
// --------------------------------------------------------------------------

// getHandle / can*() / readRawData are stack-agnostic and live in the common src/BLERemoteCharacteristic.cpp.

// --------------------------------------------------------------------------
// Read
// --------------------------------------------------------------------------

/**
 * @brief Heuristic for "must encrypt/authenticate" style GATT status codes.
 * @param s Status from a GATTC operation.
 * @return True if pairing/encryption can clear the status.
 * @note Insufficient authorization is an application deny; retrying SMP will not help.
 */
static bool isAuthGattStatus(esp_gatt_status_t s) {
  return s == ESP_GATT_INSUF_AUTHENTICATION || s == ESP_GATT_INSUF_ENCRYPTION;
}

/**
 * @brief Triggers link encryption and waits for SMP completion (when built with SMP support).
 * @param client GATT client implementation bound to the connection.
 * @return True if authentication-wait returns OK; false on stack errors or when SMP is unavailable.
 */
static bool initiateSecurityAndWait(const RemoteGattTarget &target, const std::shared_ptr<BLEClient::Impl> &client) {
#if BLE_SMP_SUPPORTED
  if (!client) {
    return false;
  }
  if (!BLESecurity::Impl::s_instance) {
    (void)BLE.getSecurity();
  }
  auto *secImpl = BLESecurity::Impl::s_instance;
  if (!secImpl) {
    return false;
  }

  esp_err_t err;
  {
    BLELockGuard lock(client->mtx);
    if (!remoteTargetConnectedLocked(target, client)) {
      return false;
    }
    // Prepare the sync BEFORE triggering encryption so the AUTH_CMPL_EVT
    // give() can't race ahead of our wait().
    secImpl->authSync.take();
    esp_bd_addr_t bda;
    client->peerAddress.toEspBdAddr(bda);
    err = esp_ble_set_encryption(bda, ESP_BLE_SEC_ENCRYPT_MITM);
  }
  if (err != ESP_OK) {
    secImpl->authSync.give(BTStatus::Fail);
    return false;
  }

  BTStatus st = secImpl->authSync.wait(10000);
  return st == BTStatus::OK;
#else
  (void)target;
  (void)client;
  return false;
#endif
}

static bool performRemoteRead(
  const RemoteGattTarget &target, const std::shared_ptr<BLEClient::Impl> &client, uint16_t handle, bool isDescr,
  std::vector<uint8_t> &value, uint32_t timeoutMs
) {
  if (!client) {
    return false;
  }

  BLELockGuard operationLock(client->gattOpMtx);
  for (int retry = 0; retry < 2; retry++) {
    esp_err_t err;
    {
      BLELockGuard lock(client->mtx);
      if (!remoteTargetConnectedLocked(target, client)) {
        return false;
      }
      // A timed-out request remains identifiable until its native completion
      // arrives. Do not reuse the shared completion slot in the meantime: a
      // late event for the same handle is otherwise indistinguishable from
      // the new operation.
      if (client->pendingReadHandle != 0) {
        return false;
      }
      client->readBuf.clear();
      client->lastReadStatus = ESP_GATT_OK;
      client->pendingReadHandle = handle;
      client->readSync.take();
      err = isDescr ? esp_ble_gattc_read_char_descr(client->gattcIf, client->connId, handle, ESP_GATT_AUTH_REQ_NONE)
                    : esp_ble_gattc_read_char(client->gattcIf, client->connId, handle, ESP_GATT_AUTH_REQ_NONE);
      if (err != ESP_OK) {
        client->pendingReadHandle = 0;
        client->readSync.give(BTStatus::Fail);
      }
    }

    if (err != ESP_OK) {
      log_e("esp_ble_gattc_read: %s", esp_err_to_name(err));
      return false;
    }

    BTStatus st = client->readSync.wait(timeoutMs);
    esp_gatt_status_t lastStatus = ESP_GATT_OK;
    {
      BLELockGuard lock(client->mtx);
      if (st != BTStatus::Timeout) {
        client->pendingReadHandle = 0;
      }
      lastStatus = client->lastReadStatus;
      if (st == BTStatus::OK && remoteTargetConnectedLocked(target, client)) {
        value = std::move(client->readBuf);
      } else if (st == BTStatus::OK) {
        st = BTStatus::InvalidState;
      }
    }
    if (st == BTStatus::OK) {
      return true;
    }

    if (retry == 0 && isAuthGattStatus(lastStatus)) {
      if (!initiateSecurityAndWait(target, client)) {
        return false;
      }
      continue;
    }
    break;
  }
  return false;
}

static BTStatus performRemoteWrite(
  const RemoteGattTarget &target, const std::shared_ptr<BLEClient::Impl> &client, uint16_t handle, bool isDescr,
  const uint8_t *data, size_t len, bool withResponse
) {
  if (!client) {
    return BTStatus::InvalidState;
  }

  if (!withResponse) {
    // A no-response write is a single ATT Write Command and cannot exceed
    // MTU-3; fail explicitly instead of issuing an oversized single write
    // (parity with the NimBLE backend, which must not silently upgrade to a
    // long write either).
    uint16_t mtu;
    {
      BLELockGuard lock(client->mtx);
      if (!remoteTargetConnectedLocked(target, client)) {
        return BTStatus::InvalidState;
      }
      mtu = client->mtu.load();
    }
    size_t maxSingle = (mtu > 3) ? static_cast<size_t>(mtu - 3) : 0;
    if (len > maxSingle) {
      log_e("esp_ble_gattc_write no-rsp len=%u exceeds MTU-3 (%u)", (unsigned)len, (unsigned)maxSingle);
      return BTStatus::InvalidParam;
    }

    // Write Command must remain callable from the Bluedroid callback task.
    // Admission and native submission are atomic under the client mutex, but
    // completion remains asynchronous: another write gets Busy until the
    // stack reports this command complete.
    esp_err_t err;
    {
      BLELockGuard lock(client->mtx);
      if (!remoteTargetConnectedLocked(target, client)) {
        return BTStatus::InvalidState;
      }
#if defined(BLE_GATT_VALIDATION_HOOKS)
      if (client->requestWaiter && client->requestHandle != handle) {
        bleRemoteTestWriteCommandOverlappedWaiter.store(true, std::memory_order_relaxed);
      }
      if (client->validationHoldWriteCommand && client->validationHeldWriteCommandEventSeen
          && bleRemoteTestReleaseWriteCommandCompletion.exchange(false, std::memory_order_relaxed)) {
        client->validationHoldWriteCommand = false;
        client->validationHeldWriteCommandEventSeen = false;
        if (client->commandCount > 0) {
          --client->commandCount;
        }
        if (client->requestHandle == 0 && client->commandCount == 0) {
          client->writeOwner.reset();
        }
      }
#endif
      // A callback-issued Write Command may arrive while an unrelated
      // response write is completing (commonly the CCCD write that enabled
      // the callback). Different handles are unambiguous in WRITE_*_EVT, so
      // allow that case; retain Busy for the same handle.
      if (client->commandCount >= kMaxPendingWriteCommands || client->requestHandle == handle
#if defined(BLE_GATT_VALIDATION_HOOKS)
          || client->validationHoldWriteCommand
#endif
      ) {
        return BTStatus::Busy;
      }
      // Do not disturb an unrelated response write's waiter/status.
      if (!client->requestWaiter) {
        client->lastWriteStatus = ESP_GATT_OK;
      }
#if defined(BLE_GATT_VALIDATION_HOOKS)
      client->validationHoldWriteCommand =
        bleRemoteTestHoldWriteCommandCompletion.exchange(false, std::memory_order_relaxed);
      client->validationHeldWriteCommandEventSeen = false;
#endif
      bool forceSubmitFailure = false;
#if defined(BLE_GATT_VALIDATION_HOOKS)
      int remainingFailures = bleRemoteTestWriteCommandSubmitFailures.load(std::memory_order_relaxed);
      while (remainingFailures > 0
             && !bleRemoteTestWriteCommandSubmitFailures.compare_exchange_weak(
               remainingFailures, remainingFailures - 1, std::memory_order_relaxed
             )) {}
      forceSubmitFailure = remainingFailures > 0;
#endif
      err = forceSubmitFailure
              ? ESP_FAIL
              : isDescr ? esp_ble_gattc_write_char_descr(
                            client->gattcIf, client->connId, handle, len, const_cast<uint8_t *>(data),
                            ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE
                          )
                        : esp_ble_gattc_write_char(
                            client->gattcIf, client->connId, handle, len, const_cast<uint8_t *>(data),
                            ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE
                          );
      if (err != ESP_OK) {
#if defined(BLE_GATT_VALIDATION_HOOKS)
        client->validationHoldWriteCommand = false;
        client->validationHeldWriteCommandEventSeen = false;
#endif
      } else {
        ++client->commandCount;
        client->writeOwner = client;
      }
    }
    if (err != ESP_OK) {
      log_e("esp_ble_gattc_write no-rsp: %s", esp_err_to_name(err));
      return BTStatus::Fail;
    }
    return BTStatus::OK;
  }

  BLELockGuard operationLock(client->gattOpMtx);

  for (int retry = 0; retry < 2; retry++) {
    esp_err_t err;
    {
      BLELockGuard lock(client->mtx);
      if (!remoteTargetConnectedLocked(target, client)) {
        return BTStatus::InvalidState;
      }
      if (client->requestHandle != 0 || client->requestWaiter || client->commandCount != 0) {
        return BTStatus::Busy;
      }
      client->lastWriteStatus = ESP_GATT_OK;
      client->requestHandle = handle;
      client->requestWaiter = true;
      client->writeOwner = client;
      client->writeSync.take();
      err =
        isDescr ? esp_ble_gattc_write_char_descr(
                    client->gattcIf, client->connId, handle, len, const_cast<uint8_t *>(data),
                    ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE
                  )
                : esp_ble_gattc_write_char(
                    client->gattcIf, client->connId, handle, len, const_cast<uint8_t *>(data),
                    ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE
                  );
      if (err != ESP_OK) {
        client->requestHandle = 0;
        client->requestWaiter = false;
        client->writeOwner.reset();
        client->writeSync.give(BTStatus::Fail);
      }
    }

    if (err != ESP_OK) {
      log_e("esp_ble_gattc_write: %s", esp_err_to_name(err));
      return BTStatus::Fail;
    }

    uint32_t timeoutMs = 10000;
#if defined(BLE_GATT_VALIDATION_HOOKS)
    uint32_t injectedTimeout = bleRemoteTestWriteTimeoutMs.exchange(0, std::memory_order_relaxed);
    if (injectedTimeout > 0) {
      timeoutMs = injectedTimeout;
    }
#endif
    BTStatus st = client->writeSync.wait(timeoutMs);
#if defined(BLE_GATT_VALIDATION_HOOKS)
    if (st != BTStatus::Timeout && bleRemoteTestHoldWriteResponseRetirement.load(std::memory_order_relaxed)) {
      TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(5000);
      while (bleRemoteTestHoldWriteResponseRetirement.load(std::memory_order_relaxed)
             && static_cast<int32_t>(deadline - xTaskGetTickCount()) > 0) {
        vTaskDelay(1);
      }
    }
#endif
    esp_gatt_status_t lastStatus = ESP_GATT_OK;
    {
      BLELockGuard lock(client->mtx);
      if (st != BTStatus::Timeout) {
        client->requestHandle = 0;
        if (client->commandCount == 0) {
          client->writeOwner.reset();
        }
      }
      client->requestWaiter = false;
      lastStatus = client->lastWriteStatus;
#if defined(BLE_GATT_VALIDATION_HOOKS)
      if (bleRemoteTestWriteCommandOverlappedWaiter.load(std::memory_order_relaxed)) {
        bleRemoteTestWriteResponseStatusPreserved.store(lastStatus == ESP_GATT_OK, std::memory_order_relaxed);
      }
#endif
      if (st == BTStatus::OK && !remoteTargetConnectedLocked(target, client)) {
        st = BTStatus::InvalidState;
      }
    }
    if (st == BTStatus::OK) {
      return BTStatus::OK;
    }

    if (retry == 0 && isAuthGattStatus(lastStatus)) {
      if (!initiateSecurityAndWait(target, client)) {
        return BTStatus::AuthFailed;
      }
      continue;
    }
    return st;
  }
  return BTStatus::Fail;
}

// Blocking: synchronous wait on the client read object after issuing esp_ble_gattc_read_char.
bool BLERemoteCharacteristic::readInto(uint32_t timeoutMs) {
  if (!_impl) {
    return false;
  }
  auto service = _impl->serviceOwner.lock();
  RemoteGattTarget target{service, _impl, {}};
  auto client = remoteClient(target);
  if (!performRemoteRead(target, client, _impl->handle, false, _impl->value, timeoutMs)) {
    log_w("RemoteCharacteristic %s: read failed/timed out", _impl->uuid.toString().c_str());
    return false;
  }
  return true;
}

// --------------------------------------------------------------------------
// Write
// --------------------------------------------------------------------------

// Blocking: when withResponse is true, esp_ble_gattc_write_char is followed by a wait on the client write sync. Long data may still be subject to the controller/ATT MTU.
BTStatus BLERemoteCharacteristic::writeValue(const uint8_t *data, size_t len, bool withResponse) {
  auto service = _impl ? _impl->serviceOwner.lock() : nullptr;
  RemoteGattTarget target{service, _impl, {}};
  auto client = remoteClient(target);
  return performRemoteWrite(target, client, _impl ? _impl->handle : 0, false, data, len, withResponse);
}

// --------------------------------------------------------------------------
// Subscribe / Unsubscribe
// --------------------------------------------------------------------------

static BTStatus subscribeWithCccd(BLERemoteCharacteristic &chr, const std::shared_ptr<BLERemoteCharacteristic::Impl> &impl, uint16_t cccdVal, BLERemoteCharacteristic::NotifyCallback callback) {
  auto service = impl ? impl->serviceOwner.lock() : nullptr;
  RemoteGattTarget target{service, impl, {}};
  auto client = remoteClient(target);
  if (!client) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: subscribe cccd=0x%04x (connId=%u)", impl->uuid.toString().c_str(), cccdVal, client->connId.load());
  esp_err_t err;
  {
    BLELockGuard lock(client->mtx);
    if (!remoteTargetConnectedLocked(target, client)) {
      return BTStatus::InvalidState;
    }
    impl->onNotifyCb = std::move(callback);
    esp_bd_addr_t bda;
    client->peerAddress.toEspBdAddr(bda);
    err = esp_ble_gattc_register_for_notify(client->gattcIf, bda, impl->handle);
  }

  if (err != ESP_OK) {
    log_e("esp_ble_gattc_register_for_notify: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }

  BLERemoteDescriptor cccd = chr.getDescriptor(BLEUUID(BLE_DSC_UUID16_CCCD));
  if (cccd) {
    return cccd.writeValue(reinterpret_cast<const uint8_t *>(&cccdVal), sizeof(cccdVal), true);
  }

  log_w("RemoteCharacteristic %s: CCCD descriptor (0x2902) not found", impl->uuid.toString().c_str());
  return BTStatus::NotFound;
}

// Blocking: CCCD write path uses a confirmed GATTC write with wait; CCCD subscription semantics apply.
BTStatus BLERemoteCharacteristic::subscribe(BLESubscription mode, NotifyCallback callback) {
  if (mode == BLESubscription::None) {
    log_w("RemoteCharacteristic: subscribe called with BLESubscription::None; use unsubscribe()");
    return BTStatus::InvalidParam;
  }
  return subscribeWithCccd(*this, _impl, static_cast<uint16_t>(mode), std::move(callback));
}

// Blocking: writes 0x0000 to the CCCD descriptor with wait.
BTStatus BLERemoteCharacteristic::unsubscribe() {
  auto service = _impl ? _impl->serviceOwner.lock() : nullptr;
  RemoteGattTarget target{service, _impl, {}};
  auto client = remoteClient(target);
  if (!client) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: unsubscribe (connId=%u)", _impl->uuid.toString().c_str(), client->connId.load());
  {
    BLELockGuard lock(client->mtx);
    if (!remoteTargetConnectedLocked(target, client)) {
      return BTStatus::InvalidState;
    }
    _impl->onNotifyCb = nullptr;
    esp_bd_addr_t bda;
    client->peerAddress.toEspBdAddr(bda);
    esp_ble_gattc_unregister_for_notify(client->gattcIf, bda, _impl->handle);
  }

  // Write 0x0000 to CCCD
  uint16_t cccdVal = 0x0000;

  BLERemoteDescriptor cccd = getDescriptor(BLEUUID(BLE_DSC_UUID16_CCCD));
  if (cccd) {
    return cccd.writeValue(reinterpret_cast<const uint8_t *>(&cccdVal), sizeof(cccdVal), true);
  }

  log_w("RemoteCharacteristic %s: CCCD descriptor (0x2902) not found for unsubscribe", _impl->uuid.toString().c_str());
  return BTStatus::NotFound;
}

// --------------------------------------------------------------------------
// Descriptor discovery
// --------------------------------------------------------------------------

// Uses esp_ble_gattc_get_attr_count / esp_ble_gattc_get_all_descr; blocking attribute queries, not a live discovery procedure like NimBLE's.
BLERemoteDescriptor BLERemoteCharacteristic::getDescriptor(const BLEUUID &uuid) const {
  BLE_CHECK_IMPL(BLERemoteDescriptor());

  auto service = impl.serviceOwner.lock();
  RemoteGattTarget target{service, _impl, {}};
  auto client = remoteClient(target);
  BLELockGuard lock(client ? client->mtx : nullptr);
  if (!remoteTargetConnectedLocked(target, client)) {
    return BLERemoteDescriptor();
  }
  if (!impl.descriptorsDiscovered) {
    uint16_t count = 0;
    esp_gatt_status_t st = esp_ble_gattc_get_attr_count(client->gattcIf, client->connId, ESP_GATT_DB_DESCRIPTOR, 0, 0, impl.handle, &count);

    if (st == ESP_GATT_OK && count > 0) {
      std::vector<esp_gattc_descr_elem_t> descs(count);
      st = esp_ble_gattc_get_all_descr(client->gattcIf, client->connId, impl.handle, descs.data(), &count, 0);

      if (st == ESP_GATT_OK) {
        for (uint16_t i = 0; i < count; i++) {
          auto dImpl = std::make_shared<BLERemoteDescriptor::Impl>();
          dImpl->uuid = bluedroidUuidToPublic(descs[i].uuid);
          dImpl->handle = descs[i].handle;
          dImpl->chr = _impl.get();
          dImpl->chrOwner = _impl;
          dImpl->cacheMtx = impl.cacheMtx;
          impl.descriptors.push_back(dImpl);
        }
      }
    }
    if (st == ESP_GATT_OK) {
      impl.descriptorsDiscovered = true;
    }
  }

  for (auto &d : impl.descriptors) {
    if (d->uuid == uuid) {
      return BLERemoteDescriptor(d);
    }
  }
  return BLERemoteDescriptor();
}

std::vector<BLERemoteDescriptor> BLERemoteCharacteristic::getDescriptors() const {
  std::vector<BLERemoteDescriptor> result;
  if (!_impl) {
    return result;
  }

  auto service = _impl->serviceOwner.lock();
  RemoteGattTarget target{service, _impl, {}};
  auto client = remoteClient(target);
  if (!_impl->descriptorsDiscovered && client && client->connected) {
    (void)getDescriptor(BLEUUID(static_cast<uint16_t>(0)));  // triggers discovery
  }

  BLELockGuard lock(client ? client->mtx : nullptr);
  if (!remoteTargetConnectedLocked(target, client)) {
    return result;
  }
  for (auto &d : _impl->descriptors) {
    result.push_back(BLERemoteDescriptor(d));
  }
  return result;
}

#else /* !BLE_GATT_CLIENT_SUPPORTED -- stubs */

#include "esp32-hal-log.h"

// Stubs for BLE_GATT_CLIENT_SUPPORTED == 0: false/empty/NotSupported as appropriate; log on side-effecting paths.

bool BLERemoteCharacteristic::readInto(uint32_t) {
  return false;
}

BTStatus BLERemoteCharacteristic::writeValue(const uint8_t *, size_t, bool) {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BTStatus BLERemoteCharacteristic::subscribe(BLESubscription, NotifyCallback) {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BTStatus BLERemoteCharacteristic::unsubscribe() {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BLERemoteDescriptor BLERemoteCharacteristic::getDescriptor(const BLEUUID &) const {
  return BLERemoteDescriptor();
}

std::vector<BLERemoteDescriptor> BLERemoteCharacteristic::getDescriptors() const {
  return {};
}

#endif /* BLE_GATT_CLIENT_SUPPORTED */

// ==========================================================================
// BLERemoteDescriptor - remote descriptor read/write
// ==========================================================================

#if BLE_GATT_CLIENT_SUPPORTED

#include "BLE.h"
#include "client/BLEClient.bluedroid.h"
#include "remote/BLERemoteGatt.bluedroid.h"
#include "core/BLEImplHelpers.h"
#include "esp32-hal-log.h"

#include <esp_gattc_api.h>
#include <string.h>

// --------------------------------------------------------------------------
// Read
// --------------------------------------------------------------------------

// Blocking: esp_ble_gattc_read_char_descr and synchronous wait on the client read sync object.
bool BLERemoteDescriptor::readInto(uint32_t timeoutMs) {
  if (!_impl) {
    return false;
  }
  auto characteristic = _impl->chrOwner.lock();
  auto service = characteristic ? characteristic->serviceOwner.lock() : nullptr;
  RemoteGattTarget target{service, characteristic, _impl};
  auto client = remoteClient(target);
  if (!performRemoteRead(target, client, _impl->handle, true, _impl->value, timeoutMs)) {
    log_w("RemoteDescriptor: read failed/timed out (handle=0x%04x)", _impl->handle);
    return false;
  }
  return true;
}

// --------------------------------------------------------------------------
// Write
// --------------------------------------------------------------------------

// Blocking on confirmed writes: esp_ble_gattc_write_char_descr and wait; used for CCCD subscription writes from higher layers.
BTStatus BLERemoteDescriptor::writeValue(const uint8_t *data, size_t len, bool withResponse) {
  auto characteristic = _impl ? _impl->chrOwner.lock() : nullptr;
  auto service = characteristic ? characteristic->serviceOwner.lock() : nullptr;
  RemoteGattTarget target{service, characteristic, _impl};
  auto client = remoteClient(target);
  return performRemoteWrite(target, client, _impl ? _impl->handle : 0, true, data, len, withResponse);
}

#else /* !BLE_GATT_CLIENT_SUPPORTED -- stubs */

#include "esp32-hal-log.h"

// Stubs for BLE_GATT_CLIENT_SUPPORTED == 0: empty read, NotSupported write.

bool BLERemoteDescriptor::readInto(uint32_t) {
  return false;
}

BTStatus BLERemoteDescriptor::writeValue(const uint8_t *, size_t, bool) {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

#endif /* BLE_GATT_CLIENT_SUPPORTED */

#endif /* BLE_BLUEDROID */
