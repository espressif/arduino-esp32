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

BLEClient::Impl *remoteClient(const BLERemoteService::Impl *service) {
  if (!service) {
    return nullptr;
  }
  auto *client = service->client;
  return client && client->connected && client->connectionGeneration.load() == service->clientGeneration ? client : nullptr;
}

BLEClient::Impl *remoteClient(const std::shared_ptr<BLERemoteCharacteristic::Impl> &chr) {
  return chr ? remoteClient(chr->service) : nullptr;
}

BLEClient::Impl *remoteClient(const std::shared_ptr<BLERemoteDescriptor::Impl> &desc) {
  return (desc && desc->chr) ? remoteClient(desc->chr->service) : nullptr;
}

}  // namespace

// getClient() is stack-agnostic and lives in the common src/BLERemoteService.cpp.

// Blocking: may call esp_ble_gattc_get_attr_count / esp_ble_gattc_get_all_char (synchronous attribute queries) while connected.
BLERemoteCharacteristic BLERemoteService::getCharacteristic(const BLEUUID &uuid) const {
  BLE_CHECK_IMPL(BLERemoteCharacteristic());

  // Discover characteristics if not yet done
  auto *client = remoteClient(&impl);
  BLELockGuard lock(client ? client->mtx : nullptr);
  if (!impl.characteristicsDiscovered && client) {
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

  auto *client = remoteClient(_impl.get());
  if (!_impl->characteristicsDiscovered && client) {
    (void)getCharacteristic(BLEUUID(static_cast<uint16_t>(0)));  // triggers discovery
  }

  BLELockGuard lock(client ? client->mtx : nullptr);
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
static bool initiateSecurityAndWait(BLEClient::Impl *client) {
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

  // Prepare the sync BEFORE triggering encryption so the AUTH_CMPL_EVT
  // give() can't race ahead of our wait().
  secImpl->authSync.take();

  esp_bd_addr_t bda;
  client->peerAddress.toEspBdAddr(bda);
  esp_err_t err = esp_ble_set_encryption(bda, ESP_BLE_SEC_ENCRYPT_MITM);
  if (err != ESP_OK) {
    secImpl->authSync.give(BTStatus::Fail);
    return false;
  }

  BTStatus st = secImpl->authSync.wait(10000);
  return st == BTStatus::OK;
#else
  return false;
#endif
}

static bool performRemoteRead(BLEClient::Impl *client, uint16_t handle, bool isDescr, std::vector<uint8_t> &value, uint32_t timeoutMs) {
  if (!client || !client->connected) {
    return false;
  }

  for (int retry = 0; retry < 2; retry++) {
    {
      BLELockGuard lock(client->mtx);
      client->readBuf.clear();
      client->lastReadStatus = ESP_GATT_OK;
      client->pendingReadHandle = handle;
    }
    client->readSync.take();

    esp_err_t err = isDescr ? esp_ble_gattc_read_char_descr(client->gattcIf, client->connId, handle, ESP_GATT_AUTH_REQ_NONE)
                            : esp_ble_gattc_read_char(client->gattcIf, client->connId, handle, ESP_GATT_AUTH_REQ_NONE);

    if (err != ESP_OK) {
      {
        BLELockGuard lock(client->mtx);
        client->pendingReadHandle = 0;
      }
      log_e("esp_ble_gattc_read: %s", esp_err_to_name(err));
      client->readSync.give(BTStatus::Fail);
      return false;
    }

    BTStatus st = client->readSync.wait(timeoutMs);
    esp_gatt_status_t lastStatus = ESP_GATT_OK;
    {
      BLELockGuard lock(client->mtx);
      client->pendingReadHandle = 0;
      lastStatus = client->lastReadStatus;
      if (st == BTStatus::OK) {
        value = std::move(client->readBuf);
      }
    }
    if (st == BTStatus::OK) {
      return true;
    }

    if (retry == 0 && isAuthGattStatus(lastStatus)) {
      if (!initiateSecurityAndWait(client)) {
        return false;
      }
      continue;
    }
    break;
  }
  return false;
}

static BTStatus performRemoteWrite(BLEClient::Impl *client, uint16_t handle, bool isDescr, const uint8_t *data, size_t len, bool withResponse) {
  if (!client) {
    return BTStatus::InvalidState;
  }

  if (!withResponse) {
    // A no-response write is a single ATT Write Command and cannot exceed
    // MTU-3; fail explicitly instead of issuing an oversized single write
    // (parity with the NimBLE backend, which must not silently upgrade to a
    // long write either).
    uint16_t mtu = client->mtu.load();
    size_t maxSingle = (mtu > 3) ? static_cast<size_t>(mtu - 3) : 0;
    if (len > maxSingle) {
      log_e("esp_ble_gattc_write no-rsp len=%u exceeds MTU-3 (%u)", (unsigned)len, (unsigned)maxSingle);
      return BTStatus::InvalidParam;
    }
  }

  esp_gatt_write_type_t writeType = withResponse ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP;

  for (int retry = 0; retry < 2; retry++) {
    if (withResponse) {
      {
        BLELockGuard lock(client->mtx);
        client->lastWriteStatus = ESP_GATT_OK;
        client->pendingWriteHandle = handle;
      }
      client->writeSync.take();
    }

    esp_err_t err =
      isDescr ? esp_ble_gattc_write_char_descr(client->gattcIf, client->connId, handle, len, const_cast<uint8_t *>(data), writeType, ESP_GATT_AUTH_REQ_NONE)
              : esp_ble_gattc_write_char(client->gattcIf, client->connId, handle, len, const_cast<uint8_t *>(data), writeType, ESP_GATT_AUTH_REQ_NONE);

    if (err != ESP_OK) {
      log_e("esp_ble_gattc_write: %s", esp_err_to_name(err));
      if (withResponse) {
        {
          BLELockGuard lock(client->mtx);
          client->pendingWriteHandle = 0;
        }
        client->writeSync.give(BTStatus::Fail);
      }
      return BTStatus::Fail;
    }

    if (!withResponse) {
      return BTStatus::OK;
    }

    BTStatus st = client->writeSync.wait(10000);
    esp_gatt_status_t lastStatus = ESP_GATT_OK;
    {
      BLELockGuard lock(client->mtx);
      client->pendingWriteHandle = 0;
      lastStatus = client->lastWriteStatus;
    }
    if (st == BTStatus::OK) {
      return BTStatus::OK;
    }

    if (retry == 0 && isAuthGattStatus(lastStatus)) {
      if (!initiateSecurityAndWait(client)) {
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
  auto client = remoteClient(_impl);
  if (!performRemoteRead(client, _impl->handle, false, _impl->value, timeoutMs)) {
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
  auto client = remoteClient(_impl);
  return performRemoteWrite(client, _impl ? _impl->handle : 0, false, data, len, withResponse);
}

// --------------------------------------------------------------------------
// Subscribe / Unsubscribe
// --------------------------------------------------------------------------

static BTStatus subscribeWithCccd(BLERemoteCharacteristic &chr, const std::shared_ptr<BLERemoteCharacteristic::Impl> &impl, uint16_t cccdVal, BLERemoteCharacteristic::NotifyCallback callback) {
  auto client = remoteClient(impl);
  if (!client) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: subscribe cccd=0x%04x (connId=%u)", impl->uuid.toString().c_str(), cccdVal, client->connId.load());
  {
    BLELockGuard lock(client->mtx);
    impl->onNotifyCb = std::move(callback);
  }

  // Register for notifications/indications with the Bluedroid stack
  esp_bd_addr_t bda;
  client->peerAddress.toEspBdAddr(bda);
  esp_err_t err = esp_ble_gattc_register_for_notify(client->gattcIf, bda, impl->handle);
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
  auto client = remoteClient(_impl);
  if (!client) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: unsubscribe (connId=%u)", _impl->uuid.toString().c_str(), client->connId.load());
  {
    BLELockGuard lock(client->mtx);
    _impl->onNotifyCb = nullptr;
  }

  // Unregister for notifications
  esp_bd_addr_t bda;
  client->peerAddress.toEspBdAddr(bda);
  esp_ble_gattc_unregister_for_notify(client->gattcIf, bda, _impl->handle);

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

  auto client = remoteClient(_impl);
  BLELockGuard lock(client ? client->mtx : nullptr);
  if (!impl.descriptorsDiscovered && client && client->connected) {
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

  auto client = remoteClient(_impl);
  if (!_impl->descriptorsDiscovered && client && client->connected) {
    (void)getDescriptor(BLEUUID(static_cast<uint16_t>(0)));  // triggers discovery
  }

  BLELockGuard lock(client ? client->mtx : nullptr);
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
  auto client = remoteClient(_impl);
  if (!performRemoteRead(client, _impl->handle, true, _impl->value, timeoutMs)) {
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
  auto client = remoteClient(_impl);
  return performRemoteWrite(client, _impl ? _impl->handle : 0, true, data, len, withResponse);
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
