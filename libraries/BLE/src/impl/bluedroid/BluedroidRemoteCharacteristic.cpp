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
 * @file BluedroidRemoteCharacteristic.cpp
 * @brief Bluedroid GATT client: remote characteristic read, write, notify/indicate, and descriptor cache.
 */

#include "impl/BLEGuards.h"
#if BLE_BLUEDROID

#if BLE_GATT_CLIENT_SUPPORTED

#include "BLE.h"
#include "BluedroidClient.h"
#if BLE_SMP_SUPPORTED
#include "BluedroidSecurity.h"
#endif
#include "BluedroidRemoteTypes.h"
#include "BluedroidUUID.h"
#include "impl/BLEImplHelpers.h"
#include "impl/BLEMutex.h"
#include "esp32-hal-log.h"

#include <esp_gattc_api.h>
#include <esp_gap_ble_api.h>
#include <esp_gatt_defs.h>
#include <string.h>

// --------------------------------------------------------------------------
// BLERemoteCharacteristic -- Bluedroid backend
// --------------------------------------------------------------------------

/**
 * @brief Returns the GATT attribute handle for this remote characteristic.
 * @return Characteristic handle, or 0 if there is no implementation.
 */
uint16_t BLERemoteCharacteristic::getHandle() const {
  return _impl ? _impl->handle : 0;
}

/**
 * @brief Whether the Bluedroid characteristic properties include read.
 * @return True if READ is set; false otherwise.
 */
bool BLERemoteCharacteristic::canRead() const {
  return _impl && (_impl->properties & ESP_GATT_CHAR_PROP_BIT_READ);
}

/**
 * @brief Whether the characteristic properties include write (with response).
 * @return True if WRITE is set; false otherwise.
 */
bool BLERemoteCharacteristic::canWrite() const {
  return _impl && (_impl->properties & ESP_GATT_CHAR_PROP_BIT_WRITE);
}

/**
 * @brief Whether the characteristic allows write-without-response.
 * @return True if WRITE_NR is set; false otherwise.
 */
bool BLERemoteCharacteristic::canWriteNoResponse() const {
  return _impl && (_impl->properties & ESP_GATT_CHAR_PROP_BIT_WRITE_NR);
}

/**
 * @brief Whether the characteristic supports notifications.
 * @return True if NOTIFY is set; false otherwise.
 */
bool BLERemoteCharacteristic::canNotify() const {
  return _impl && (_impl->properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY);
}

/**
 * @brief Whether the characteristic supports indications.
 * @return True if INDICATE is set; false otherwise.
 */
bool BLERemoteCharacteristic::canIndicate() const {
  return _impl && (_impl->properties & ESP_GATT_CHAR_PROP_BIT_INDICATE);
}

/**
 * @brief Whether the characteristic is marked broadcast-capable in the GATT database.
 * @return True if BROADCAST is set; false otherwise.
 */
bool BLERemoteCharacteristic::canBroadcast() const {
  return _impl && (_impl->properties & ESP_GATT_CHAR_PROP_BIT_BROADCAST);
}

// --------------------------------------------------------------------------
// Read
// --------------------------------------------------------------------------

/**
 * @brief Heuristic for "must encrypt/authenticate" style GATT status codes.
 * @param s Status from a GATTC operation.
 * @return True if the status matches insufficient auth/authorization/encryption.
 */
static bool isAuthGattStatus(esp_gatt_status_t s) {
  return s == ESP_GATT_INSUF_AUTHENTICATION || s == ESP_GATT_INSUF_AUTHORIZATION || s == ESP_GATT_INSUF_ENCRYPTION;
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
  memcpy(bda, client->peerAddress.data(), 6);
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

/**
 * @brief Reads the characteristic value via @c esp_ble_gattc_read_char.
 * @param timeoutMs Wait time for the read to complete in the GATTC event path.
 * @return The read payload as a string, or empty on error, auth failure, or timeout.
 * @note Blocking: synchronous wait on the client read object after issuing @c esp_ble_gattc_read_char.
 */
String BLERemoteCharacteristic::readValue(uint32_t timeoutMs) {
  if (!_impl || !_impl->service || !_impl->service->client) {
    return "";
  }
  auto *client = _impl->service->client;
  if (!client->connected) {
    return "";
  }

  log_d("RemoteCharacteristic %s: read (connId=%u)", _impl->uuid.toString().c_str(), client->connId);

  for (int retry = 0; retry < 2; retry++) {
    client->readBuf.clear();
    client->lastReadStatus = ESP_GATT_OK;
    client->readSync.take();

    esp_err_t err = esp_ble_gattc_read_char(client->gattcIf, client->connId, _impl->handle, ESP_GATT_AUTH_REQ_NONE);

    if (err != ESP_OK) {
      log_e("esp_ble_gattc_read_char: %s", esp_err_to_name(err));
      client->readSync.give(BTStatus::Fail);
      return "";
    }

    BTStatus st = client->readSync.wait(timeoutMs);

    if (st == BTStatus::OK) {
      _impl->value = client->readBuf;
      log_d("RemoteCharacteristic %s: read %u byte(s)", _impl->uuid.toString().c_str(), (unsigned)client->readBuf.size());
      return String(reinterpret_cast<const char *>(client->readBuf.data()), client->readBuf.size());
    }

    if (retry == 0 && isAuthGattStatus(client->lastReadStatus)) {
      log_d("RemoteCharacteristic %s: auth error on read, initiating security", _impl->uuid.toString().c_str());
      if (!initiateSecurityAndWait(client)) {
        log_w("RemoteCharacteristic %s: security initiation failed", _impl->uuid.toString().c_str());
        return "";
      }
      continue;
    }
    break;
  }
  log_w("RemoteCharacteristic %s: read failed/timed out", _impl->uuid.toString().c_str());
  return "";
}

/**
 * @brief Returns a view into the last value cached from @ref readValue.
 * @param len If non-null, receives the byte count (0 when empty).
 * @return Pointer to cached data, or nullptr.
 */
const uint8_t *BLERemoteCharacteristic::readRawData(size_t *len) {
  if (!_impl || _impl->value.empty()) {
    if (len) {
      *len = 0;
    }
    return nullptr;
  }
  if (len) {
    *len = _impl->value.size();
  }
  return _impl->value.data();
}

// --------------------------------------------------------------------------
// Write
// --------------------------------------------------------------------------

/**
 * @brief Writes the characteristic, optionally waiting for the GATT response event.
 * @param data Data buffer.
 * @param len Number of bytes.
 * @param withResponse If true, waits for write confirmation; if false, uses a no-response write type when allowed.
 * @return Final status; may reflect timeout or link loss.
 * @note Blocking: when @a withResponse is true, @c esp_ble_gattc_write_char is followed by a wait on the client write sync. Long data may still be subject to the controller/ATT MTU.
 */
BTStatus BLERemoteCharacteristic::writeValue(const uint8_t *data, size_t len, bool withResponse) {
  if (!_impl || !_impl->service || !_impl->service->client) {
    return BTStatus::InvalidState;
  }
  auto *client = _impl->service->client;
  if (!client->connected) {
    return BTStatus::NotConnected;
  }

  log_d("RemoteCharacteristic %s: write len=%u withResponse=%d (connId=%u)", _impl->uuid.toString().c_str(), len, withResponse, client->connId);
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

/**
 * @brief Registers for notify with Bluedroid and enables notify/indicate via the CCCD.
 * @param notifications True for notify (0x0001 in CCCD), false for indicate (0x0002).
 * @param callback Application callback for incoming packets.
 * @return Result of the CCCD write (via @c BLERemoteDescriptor or handle+1) or @c register_for_notify.
 * @note Blocking: CCCD write path uses a confirmed GATTC write with wait; CCCD subscription semantics apply.
 */
BTStatus BLERemoteCharacteristic::subscribe(bool notifications, NotifyCallback callback) {
  if (!_impl || !_impl->service || !_impl->service->client) {
    return BTStatus::InvalidState;
  }
  auto *client = _impl->service->client;
  if (!client->connected) {
    return BTStatus::NotConnected;
  }

  log_d("RemoteCharacteristic %s: subscribe %s (connId=%u)", _impl->uuid.toString().c_str(), notifications ? "notify" : "indicate", client->connId);
  {
    BLELockGuard lock(client->mtx);
    _impl->notifyCb = callback;
  }

  // Register for notifications/indications with the Bluedroid stack
  esp_bd_addr_t bda;
  memcpy(bda, client->peerAddress.data(), 6);
  esp_err_t err = esp_ble_gattc_register_for_notify(client->gattcIf, bda, _impl->handle);
  if (err != ESP_OK) {
    log_e("esp_ble_gattc_register_for_notify: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }

  // Write CCCD descriptor to enable notifications/indications
  uint16_t cccdVal = notifications ? 0x0001 : 0x0002;

  // Find CCCD descriptor (0x2902) - discover descriptors if needed
  BLERemoteDescriptor cccd = getDescriptor(BLEUUID(static_cast<uint16_t>(0x2902)));
  if (cccd) {
    return cccd.writeValue(reinterpret_cast<const uint8_t *>(&cccdVal), sizeof(cccdVal), true);
  }

  log_w("RemoteCharacteristic %s: CCCD descriptor (0x2902) not found", _impl->uuid.toString().c_str());
  return BTStatus::NotFound;
}

/**
 * @brief Unregisters notify and writes 0x0000 to the CCCD (direct or via descriptor object).
 * @return Status of the CCCD clear operation when using the fallback GATTC write; otherwise the descriptor @ref BLERemoteDescriptor::writeValue result.
 * @note Blocking: confirmed descriptor write with wait in the handle+1 fallback path.
 */
BTStatus BLERemoteCharacteristic::unsubscribe() {
  if (!_impl || !_impl->service || !_impl->service->client) {
    return BTStatus::InvalidState;
  }
  auto *client = _impl->service->client;
  if (!client->connected) {
    return BTStatus::NotConnected;
  }

  log_d("RemoteCharacteristic %s: unsubscribe (connId=%u)", _impl->uuid.toString().c_str(), client->connId);
  {
    BLELockGuard lock(client->mtx);
    _impl->notifyCb = nullptr;
  }

  // Unregister for notifications
  esp_bd_addr_t bda;
  memcpy(bda, client->peerAddress.data(), 6);
  esp_ble_gattc_unregister_for_notify(client->gattcIf, bda, _impl->handle);

  // Write 0x0000 to CCCD
  uint16_t cccdVal = 0x0000;

  BLERemoteDescriptor cccd = getDescriptor(BLEUUID(static_cast<uint16_t>(0x2902)));
  if (cccd) {
    return cccd.writeValue(reinterpret_cast<const uint8_t *>(&cccdVal), sizeof(cccdVal), true);
  }

  log_w("RemoteCharacteristic %s: CCCD descriptor (0x2902) not found for unsubscribe", _impl->uuid.toString().c_str());
  return BTStatus::NotFound;
}

// --------------------------------------------------------------------------
// Descriptor discovery
// --------------------------------------------------------------------------

/**
 * @brief Resolves a descriptor by UUID, filling the list from the GATT database on first use.
 * @param uuid Requested 128-bit-comparable descriptor UUID.
 * @return A descriptor object, or empty if not found.
 * @note Uses @c esp_ble_gattc_get_attr_count / @c esp_ble_gattc_get_all_descr; blocking attribute queries, not a live discovery procedure like NimBLE's.
 */
BLERemoteDescriptor BLERemoteCharacteristic::getDescriptor(const BLEUUID &uuid) {
  BLE_CHECK_IMPL(BLERemoteDescriptor());

  if (!impl.descriptorsRetrieved && impl.service && impl.service->client && impl.service->client->connected) {
    auto *client = impl.service->client;
    uint16_t count = 0;
    esp_gatt_status_t st = esp_ble_gattc_get_attr_count(client->gattcIf, client->connId, ESP_GATT_DB_DESCRIPTOR, 0, 0, impl.handle, &count);

    if (st == ESP_GATT_OK && count > 0) {
      std::vector<esp_gattc_descr_elem_t> descs(count);
      st = esp_ble_gattc_get_all_descr(client->gattcIf, client->connId, impl.handle, descs.data(), &count, 0);

      if (st == ESP_GATT_OK) {
        for (uint16_t i = 0; i < count; i++) {
          auto dImpl = std::make_shared<BLERemoteDescriptor::Impl>();
          dImpl->uuid = espToUuid(descs[i].uuid);
          dImpl->handle = descs[i].handle;
          dImpl->chr = _impl.get();
          impl.descriptors.push_back(dImpl);
        }
      }
    }
    impl.descriptorsRetrieved = true;
  }

  BLEUUID target = uuid.to128();
  for (auto &d : impl.descriptors) {
    if (d->uuid.to128() == target) {
      return BLERemoteDescriptor(d);
    }
  }
  return BLERemoteDescriptor();
}

/**
 * @brief Returns all descriptors, triggering lazy one-time query via @ref getDescriptor.
 * @return All descriptors for this characteristic; may be empty.
 */
std::vector<BLERemoteDescriptor> BLERemoteCharacteristic::getDescriptors() const {
  std::vector<BLERemoteDescriptor> result;
  if (!_impl) {
    return result;
  }

  // Trigger discovery through non-const cast (lazy init)
  if (!_impl->descriptorsRetrieved && _impl->service && _impl->service->client && _impl->service->client->connected) {
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

// Stubs for BLE_GATT_CLIENT_SUPPORTED == 0: false/empty/NotSupported/nullptr as appropriate; log on side-effecting paths.

uint16_t BLERemoteCharacteristic::getHandle() const {
  return 0;
}

bool BLERemoteCharacteristic::canRead() const {
  return false;
}

bool BLERemoteCharacteristic::canWrite() const {
  return false;
}

bool BLERemoteCharacteristic::canWriteNoResponse() const {
  return false;
}

bool BLERemoteCharacteristic::canNotify() const {
  return false;
}

bool BLERemoteCharacteristic::canIndicate() const {
  return false;
}

bool BLERemoteCharacteristic::canBroadcast() const {
  return false;
}

String BLERemoteCharacteristic::readValue(uint32_t) {
  return "";
}

const uint8_t *BLERemoteCharacteristic::readRawData(size_t *len) {
  if (len) {
    *len = 0;
  }
  return nullptr;
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

#endif /* BLE_BLUEDROID */
