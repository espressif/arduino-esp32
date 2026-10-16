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
BLERemoteCharacteristic BLERemoteService::getCharacteristic(const BLEUUID &uuid) {
  BLE_CHECK_IMPL(BLERemoteCharacteristic());

  // Discover characteristics if not yet done
  auto *client = impl.client;
  if (!impl.characteristicsDiscovered && client && client->connected) {
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
    impl.characteristicsDiscovered = true;
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

  // Trigger discovery through non-const cast (discovery is a lazy-init pattern)
  auto *client = _impl->client;
  if (!_impl->characteristicsDiscovered && client && client->connected) {
    auto *self = const_cast<BLERemoteService *>(this);
    self->getCharacteristic(BLEUUID(static_cast<uint16_t>(0)));  // triggers discovery
  }

  for (auto &c : _impl->characteristics) {
    result.push_back(BLERemoteCharacteristic(c));
  }
  return result;
}

#else /* !BLE_GATT_CLIENT_SUPPORTED -- stubs */

#include "esp32-hal-log.h"

// Stubs for BLE_GATT_CLIENT_SUPPORTED == 0: empty characteristic handles and lists.

BLERemoteCharacteristic BLERemoteService::getCharacteristic(const BLEUUID &) {
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
    return false;
  }

  BTStatus st = secImpl->authSync.wait(10000);
  return st == BTStatus::OK;
#else
  return false;
#endif
}

// Blocking: synchronous wait on the client read object after issuing esp_ble_gattc_read_char.
bool BLERemoteCharacteristic::readInto(uint32_t timeoutMs) {
  auto client = remoteClient(_impl);
  if (!client) {
    return false;
  }
  if (!client->connected) {
    return false;
  }

  log_d("RemoteCharacteristic %s: read (connId=%u)", _impl->uuid.toString().c_str(), client->connId.load());

  for (int retry = 0; retry < 2; retry++) {
    client->readBuf.clear();
    client->lastReadStatus = ESP_GATT_OK;
    client->readSync.take();

    esp_err_t err = esp_ble_gattc_read_char(client->gattcIf, client->connId, _impl->handle, ESP_GATT_AUTH_REQ_NONE);

    if (err != ESP_OK) {
      log_e("esp_ble_gattc_read_char: %s", esp_err_to_name(err));
      client->readSync.give(BTStatus::Fail);
      return false;
    }

    BTStatus st = client->readSync.wait(timeoutMs);

    if (st == BTStatus::OK) {
      _impl->value = client->readBuf;
      log_d("RemoteCharacteristic %s: read %u byte(s)", _impl->uuid.toString().c_str(), (unsigned)client->readBuf.size());
      return true;
    }

    if (retry == 0 && isAuthGattStatus(client->lastReadStatus)) {
      log_d("RemoteCharacteristic %s: auth error on read, initiating security", _impl->uuid.toString().c_str());
      if (!initiateSecurityAndWait(client)) {
        log_w("RemoteCharacteristic %s: security initiation failed", _impl->uuid.toString().c_str());
        return false;
      }
      continue;
    }
    break;
  }
  log_w("RemoteCharacteristic %s: read failed/timed out", _impl->uuid.toString().c_str());
  return false;
}

// --------------------------------------------------------------------------
// Write
// --------------------------------------------------------------------------

// Blocking: when withResponse is true, esp_ble_gattc_write_char is followed by a wait on the client write sync. Long data may still be subject to the controller/ATT MTU.
BTStatus BLERemoteCharacteristic::writeValue(const uint8_t *data, size_t len, bool withResponse) {
  auto client = remoteClient(_impl);
  if (!client) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: write len=%u withResponse=%d (connId=%u)", _impl->uuid.toString().c_str(), len, withResponse, client->connId.load());
  esp_gatt_write_type_t writeType = withResponse ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP;

  for (int retry = 0; retry < 2; retry++) {
    if (withResponse) {
      client->lastWriteStatus = ESP_GATT_OK;
      client->writeSync.take();
    }

    esp_err_t err =
      esp_ble_gattc_write_char(client->gattcIf, client->connId, _impl->handle, len, const_cast<uint8_t *>(data), writeType, ESP_GATT_AUTH_REQ_NONE);

    if (err != ESP_OK) {
      log_e("esp_ble_gattc_write_char: %s", esp_err_to_name(err));
      if (withResponse) {
        client->writeSync.give(BTStatus::Fail);
      }
      return BTStatus::Fail;
    }

    if (!withResponse) {
      return BTStatus::OK;
    }

    BTStatus st = client->writeSync.wait(10000);
    if (st == BTStatus::OK) {
      return BTStatus::OK;
    }

    if (retry == 0 && isAuthGattStatus(client->lastWriteStatus)) {
      log_d("RemoteCharacteristic %s: auth error on write, initiating security", _impl->uuid.toString().c_str());
      if (!initiateSecurityAndWait(client)) {
        return BTStatus::AuthFailed;
      }
      continue;
    }
    return st;
  }
  return BTStatus::Fail;
}

// --------------------------------------------------------------------------
// Subscribe / Unsubscribe
// --------------------------------------------------------------------------

// Blocking: CCCD write path uses a confirmed GATTC write with wait; CCCD subscription semantics apply.
BTStatus BLERemoteCharacteristic::subscribe(bool notifications, NotifyCallback callback) {
  auto client = remoteClient(_impl);
  if (!client) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: subscribe %s (connId=%u)", _impl->uuid.toString().c_str(), notifications ? "notify" : "indicate", client->connId.load());
  {
    BLELockGuard lock(client->mtx);
    _impl->onNotifyCb = std::move(callback);
  }

  // Register for notifications/indications with the Bluedroid stack
  esp_bd_addr_t bda;
  client->peerAddress.toEspBdAddr(bda);
  esp_err_t err = esp_ble_gattc_register_for_notify(client->gattcIf, bda, _impl->handle);
  if (err != ESP_OK) {
    log_e("esp_ble_gattc_register_for_notify: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }

  // Write CCCD descriptor to enable notifications/indications
  uint16_t cccdVal = notifications ? BLE_CCCD_NOTIFY : BLE_CCCD_INDICATE;

  BLERemoteDescriptor cccd = getDescriptor(BLEUUID(BLE_DSC_UUID16_CCCD));
  if (cccd) {
    return cccd.writeValue(reinterpret_cast<const uint8_t *>(&cccdVal), sizeof(cccdVal), true);
  }

  log_w("RemoteCharacteristic %s: CCCD descriptor (0x2902) not found", _impl->uuid.toString().c_str());
  return BTStatus::NotFound;
}

// Blocking: confirmed descriptor write with wait in the handle+1 fallback path.
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
BLERemoteDescriptor BLERemoteCharacteristic::getDescriptor(const BLEUUID &uuid) {
  BLE_CHECK_IMPL(BLERemoteDescriptor());

  auto client = remoteClient(_impl);
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
    impl.descriptorsDiscovered = true;
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

  // Trigger discovery through non-const cast (lazy init)
  auto client = remoteClient(_impl);
  if (!_impl->descriptorsDiscovered && client && client->connected) {
    auto *self = const_cast<BLERemoteCharacteristic *>(this);
    self->getDescriptor(BLEUUID(static_cast<uint16_t>(0)));  // triggers discovery
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

BTStatus BLERemoteCharacteristic::subscribe(bool, NotifyCallback) {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BTStatus BLERemoteCharacteristic::unsubscribe() {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BLERemoteDescriptor BLERemoteCharacteristic::getDescriptor(const BLEUUID &) {
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
  auto client = remoteClient(_impl);
  if (!client) {
    return false;
  }
  if (!client->connected) {
    return false;
  }

  client->readBuf.clear();
  client->readSync.take();

  esp_err_t err = esp_ble_gattc_read_char_descr(client->gattcIf, client->connId, _impl->handle, ESP_GATT_AUTH_REQ_NONE);

  if (err != ESP_OK) {
    log_e("esp_ble_gattc_read_char_descr: %s", esp_err_to_name(err));
    client->readSync.give(BTStatus::Fail);
    return false;
  }

  BTStatus st = client->readSync.wait(timeoutMs);
  if (st != BTStatus::OK) {
    log_w("RemoteDescriptor: read failed/timed out (handle=0x%04x)", _impl->handle);
    return false;
  }

  _impl->value = client->readBuf;
  return true;
}

// --------------------------------------------------------------------------
// Write
// --------------------------------------------------------------------------

// Blocking on confirmed writes: esp_ble_gattc_write_char_descr and wait; used for CCCD subscription writes from higher layers.
BTStatus BLERemoteDescriptor::writeValue(const uint8_t *data, size_t len, bool withResponse) {
  auto client = remoteClient(_impl);
  if (!client) {
    return BTStatus::InvalidState;
  }

  esp_gatt_write_type_t writeType = withResponse ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP;

  if (withResponse) {
    client->writeSync.take();
  }

  esp_err_t err =
    esp_ble_gattc_write_char_descr(client->gattcIf, client->connId, _impl->handle, len, const_cast<uint8_t *>(data), writeType, ESP_GATT_AUTH_REQ_NONE);

  if (err != ESP_OK) {
    log_e("esp_ble_gattc_write_char_descr: %s", esp_err_to_name(err));
    if (withResponse) {
      client->writeSync.give(BTStatus::Fail);
    }
    return BTStatus::Fail;
  }

  if (withResponse) {
    return client->writeSync.wait(10000);
  }
  return BTStatus::OK;
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
