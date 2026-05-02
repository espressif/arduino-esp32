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

/**
 * @file NimBLERemoteCharacteristic.cpp
 * @brief NimBLE GATT client: remote characteristic read, write, notify/indicate, and descriptor discovery.
 */

#include "impl/BLEGuards.h"
#if BLE_NIMBLE

#include "BLE.h"
#include "NimBLERemoteTypes.h"
#include "NimBLEUUID.h"
#include "esp32-hal-log.h"
#include "impl/BLEImplHelpers.h"

#if BLE_GATT_CLIENT_SUPPORTED

#include <host/ble_gap.h>
#include "impl/BLEMutex.h"
#include <utility>
#include <vector>

struct NotifyEntry {
  uint16_t connHandle;
  uint16_t attrHandle;
  std::shared_ptr<BLERemoteCharacteristic::Impl> chr;
};

static SemaphoreHandle_t sNotifyMtx = xSemaphoreCreateRecursiveMutex();
static std::vector<NotifyEntry> sNotifyRegistry;

/**
 * @brief Tests whether a NimBLE ATT return code indicates missing authentication or encryption.
 * @param rc Stack return or ATT error code.
 * @return True if the error is insufficient authentication, encryption, or authorization.
 */
static bool isAuthError(int rc) {
  return rc == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHEN) || rc == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_ENC)
         || rc == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHOR);
}

/**
 * @brief Starts pairing/encryption and blocks until security completes or times out.
 * @param connHandle GAP connection handle.
 * @return True on successful authentication wait; false if security cannot be started or times out.
 */
static bool initiateSecurityAndWait(uint16_t connHandle) {
  int rc = ble_gap_security_initiate(connHandle);
  if (rc != 0) {
    return false;
  }
  BLESecurity sec = BLE.getSecurity();
  if (!sec) {
    return false;
  }
  return sec.waitForAuthenticationComplete(10000);
}

// ==========================================================================
// BLERemoteCharacteristic implementation
// ==========================================================================

/**
 * @brief Returns the GATT value handle for this remote characteristic.
 * @return Value handle, or 0 if there is no implementation.
 */
uint16_t BLERemoteCharacteristic::getHandle() const {
  return _impl ? _impl->valHandle : 0;
}

/**
 * @brief Whether the discovered characteristic property includes READ.
 * @return True if READ is set; false otherwise.
 */
bool BLERemoteCharacteristic::canRead() const {
  return _impl && (_impl->properties & BLE_GATT_CHR_PROP_READ);
}

/**
 * @brief Whether the discovered characteristic property includes WRITE (with response).
 * @return True if WRITE is set; false otherwise.
 */
bool BLERemoteCharacteristic::canWrite() const {
  return _impl && (_impl->properties & BLE_GATT_CHR_PROP_WRITE);
}

/**
 * @brief Whether the discovered characteristic property includes write-without-response.
 * @return True if WRITE_NO_RSP is set; false otherwise.
 */
bool BLERemoteCharacteristic::canWriteNoResponse() const {
  return _impl && (_impl->properties & BLE_GATT_CHR_PROP_WRITE_NO_RSP);
}

/**
 * @brief Whether the discovered characteristic property includes NOTIFY.
 * @return True if NOTIFY is set; false otherwise.
 */
bool BLERemoteCharacteristic::canNotify() const {
  return _impl && (_impl->properties & BLE_GATT_CHR_PROP_NOTIFY);
}

/**
 * @brief Whether the discovered characteristic property includes INDICATE.
 * @return True if INDICATE is set; false otherwise.
 */
bool BLERemoteCharacteristic::canIndicate() const {
  return _impl && (_impl->properties & BLE_GATT_CHR_PROP_INDICATE);
}

/**
 * @brief Whether the discovered characteristic property includes BROADCAST.
 * @return True if BROADCAST is set; false otherwise.
 */
bool BLERemoteCharacteristic::canBroadcast() const {
  return _impl && (_impl->properties & BLE_GATT_CHR_PROP_BROADCAST);
}

/**
 * @brief Reads the characteristic value, using a long read when the attribute is "long" and falling back to a short read.
 * @param timeoutMs Milliseconds to wait for each GATT read sequence.
 * @return The value as a string, or empty on error, timeout, or disconnect.
 * @note Blocking: @c ble_gattc_read_long / @c ble_gattc_read with synchronous wait. Long read may be retried after security upgrade.
 */
String BLERemoteCharacteristic::readValue(uint32_t timeoutMs) {
  if (!_impl || !isGattConnected(_impl->connHandle)) {
    return "";
  }

  log_d("RemoteCharacteristic %s: read (conn=%u)", _impl->uuid.toString().c_str(), _impl->connHandle);
  for (int retry = 0; retry < 2; retry++) {
    _impl->lastValue.clear();
    _impl->lastReadRC = 0;
    _impl->readSync.take();

    int rc = ble_gattc_read_long(_impl->connHandle, _impl->valHandle, 0, Impl::readCb, _impl.get());
    if (rc != 0) {
      log_e("RemoteCharacteristic %s: ble_gattc_read_long rc=%d", _impl->uuid.toString().c_str(), rc);
      _impl->readSync.give(BTStatus::Fail);
      return "";
    }

    BTStatus status = _impl->readSync.wait(timeoutMs);

    if (status != BTStatus::OK && _impl->lastReadRC == BLE_HS_ATT_ERR(BLE_ATT_ERR_ATTR_NOT_LONG)) {
      _impl->lastValue.clear();
      _impl->lastReadRC = 0;
      _impl->readSync.take();
      rc = ble_gattc_read(_impl->connHandle, _impl->valHandle, Impl::readCb, _impl.get());
      if (rc != 0) {
        log_e("RemoteCharacteristic %s: ble_gattc_read fallback rc=%d", _impl->uuid.toString().c_str(), rc);
        _impl->readSync.give(BTStatus::Fail);
        return "";
      }
      status = _impl->readSync.wait(timeoutMs);
    }

    if (status == BTStatus::OK) {
      return String(reinterpret_cast<const char *>(_impl->lastValue.data()), _impl->lastValue.size());
    }

    if (retry == 0 && isAuthError(_impl->lastReadRC)) {
      log_d("RemoteCharacteristic %s: auth error on read, initiating security", _impl->uuid.toString().c_str());
      if (!initiateSecurityAndWait(_impl->connHandle)) {
        log_w("RemoteCharacteristic %s: security initiation failed", _impl->uuid.toString().c_str());
        return "";
      }
      continue;
    }
    break;
  }
  log_w("RemoteCharacteristic %s: read failed (rc=%d)", _impl->uuid.toString().c_str(), _impl->lastReadRC);
  return "";
}

/**
 * @brief Returns a pointer to the last value buffer filled by @ref readValue.
 * @param len If non-null, receives the length in bytes (or 0 when no data).
 * @return Pointer to raw bytes, or nullptr when there is no cached data.
 */
const uint8_t *BLERemoteCharacteristic::readRawData(size_t *len) {
  if (!_impl || _impl->lastValue.empty()) {
    if (len) {
      *len = 0;
    }
    return nullptr;
  }
  if (len) {
    *len = _impl->lastValue.size();
  }
  return _impl->lastValue.data();
}

/**
 * @brief Writes a value, using no-response when allowed, or a long/prepared write when the payload exceeds the ATT MTU.
 * @param data Bytes to write.
 * @param len Number of bytes.
 * @param withResponse If true, uses confirmed write or long write; if false, may use write-without-response for small payloads.
 * @return Result of the write operation.
 * @note Blocking: @c ble_gattc_write_flat, @c ble_gattc_write_long (prepared write/execute), or @c ble_gattc_write_no_rsp_flat; waits on the write sync for confirmed paths.
 */
BTStatus BLERemoteCharacteristic::writeValue(const uint8_t *data, size_t len, bool withResponse) {
  if (!_impl || !isGattConnected(_impl->connHandle)) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: write len=%u withResponse=%d (conn=%u)", _impl->uuid.toString().c_str(), len, withResponse, _impl->connHandle);
  uint16_t mtu = ble_att_mtu(_impl->connHandle);
  uint16_t maxSingle = (mtu > 3) ? (mtu - 3) : 0;

  if (!withResponse && len <= maxSingle) {
    int rc = ble_gattc_write_no_rsp_flat(_impl->connHandle, _impl->valHandle, data, len);
    if (rc != 0) {
      log_e("RemoteCharacteristic %s: write no-rsp failed rc=%d", _impl->uuid.toString().c_str(), rc);
    }
    return (rc == 0) ? BTStatus::OK : BTStatus::Fail;
  }

  bool isLong = (len > maxSingle);

  for (int retry = 0; retry < 2; retry++) {
    _impl->lastWriteRC = 0;
    _impl->writeSync.take();

    int rc;
    if (isLong) {
      struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
      if (!om) {
        _impl->writeSync.give(BTStatus::NoMemory);
        return BTStatus::NoMemory;
      }
      rc = ble_gattc_write_long(_impl->connHandle, _impl->valHandle, 0, om, Impl::writeCb, _impl.get());
    } else {
      rc = ble_gattc_write_flat(_impl->connHandle, _impl->valHandle, data, len, Impl::writeCb, _impl.get());
    }

    if (rc != 0) {
      log_e("RemoteCharacteristic %s: write rc=%d", _impl->uuid.toString().c_str(), rc);
      _impl->writeSync.give(BTStatus::Fail);
      return BTStatus::Fail;
    }

    BTStatus status = _impl->writeSync.wait(10000);
    if (status == BTStatus::OK) {
      return BTStatus::OK;
    }

    if (retry == 0 && isAuthError(_impl->lastWriteRC)) {
      log_d("RemoteCharacteristic %s: auth error on write, initiating security", _impl->uuid.toString().c_str());
      if (!initiateSecurityAndWait(_impl->connHandle)) {
        log_w("RemoteCharacteristic %s: security initiation failed on write retry", _impl->uuid.toString().c_str());
        return BTStatus::AuthFailed;
      }
      continue;
    }
    log_w("RemoteCharacteristic %s: write failed (status=%d)", _impl->uuid.toString().c_str(), static_cast<int>(status));
    return status;
  }
  return BTStatus::Fail;
}

/**
 * @brief Enables notify or indicate on the characteristic and writes the CCCD.
 * @param notifications True for notify (0x0001), false for indicate (0x0002) CCCD value.
 * @param callback Invoked for incoming notification/indication payloads.
 * @return Status of the CCCD write and wait.
 * @note Blocking: CCCD is written with @c ble_gattc_write_flat and a synchronous wait. Registers the notify path before the write.
 */
BTStatus BLERemoteCharacteristic::subscribe(bool notifications, NotifyCallback callback) {
  if (!_impl || !isGattConnected(_impl->connHandle)) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: subscribe %s (conn=%u)", _impl->uuid.toString().c_str(), notifications ? "notify" : "indicate", _impl->connHandle);
  _impl->notifyCb = callback;

  Impl::registerForNotify(_impl->connHandle, _impl->valHandle, _impl);

  uint16_t cccdVal = notifications ? 0x0001 : 0x0002;
  uint16_t cccdHandle = _impl->valHandle + 1;

  _impl->writeSync.take();
  int rc = ble_gattc_write_flat(_impl->connHandle, cccdHandle, &cccdVal, sizeof(cccdVal), Impl::writeCb, _impl.get());
  if (rc != 0) {
    log_e("RemoteCharacteristic %s: subscribe CCCD write failed rc=%d", _impl->uuid.toString().c_str(), rc);
    Impl::unregisterForNotify(_impl->connHandle, _impl->valHandle);
    _impl->writeSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }

  BTStatus status = _impl->writeSync.wait(5000);
  if (status != BTStatus::OK) {
    log_w("RemoteCharacteristic %s: subscribe failed/timed out", _impl->uuid.toString().c_str());
    Impl::unregisterForNotify(_impl->connHandle, _impl->valHandle);
  } else {
    log_i("RemoteCharacteristic %s: subscribed (%s)", _impl->uuid.toString().c_str(), notifications ? "notify" : "indicate");
  }
  return status;
}

/**
 * @brief Disables notify/indicate by writing 0 to the CCCD and unregisters the callback.
 * @return Status of the CCCD clear operation.
 * @note Blocking: @c ble_gattc_write_flat to the CCCD handle (value handle + 1) with wait.
 */
BTStatus BLERemoteCharacteristic::unsubscribe() {
  if (!_impl || !isGattConnected(_impl->connHandle)) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: unsubscribe (conn=%u)", _impl->uuid.toString().c_str(), _impl->connHandle);
  Impl::unregisterForNotify(_impl->connHandle, _impl->valHandle);
  _impl->notifyCb = nullptr;

  uint16_t cccdVal = 0x0000;
  uint16_t cccdHandle = _impl->valHandle + 1;

  _impl->writeSync.take();
  int rc = ble_gattc_write_flat(_impl->connHandle, cccdHandle, &cccdVal, sizeof(cccdVal), Impl::writeCb, _impl.get());
  if (rc != 0) {
    log_e("RemoteCharacteristic %s: unsubscribe CCCD write failed rc=%d", _impl->uuid.toString().c_str(), rc);
    _impl->writeSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  BTStatus status = _impl->writeSync.wait(5000);
  if (status != BTStatus::OK) {
    log_w("RemoteCharacteristic %s: unsubscribe timed out", _impl->uuid.toString().c_str());
  }
  return status;
}

/**
 * @brief Finds a remote descriptor by UUID, running descriptor discovery once if needed.
 * @param uuid Target descriptor UUID.
 * @return A descriptor handle, or empty if not found or discovery failed.
 * @note Blocking: @c ble_gattc_disc_all_dscs between value and end handle with sync wait; assumes CCCD at @c valHandle+1 for subscribe path elsewhere.
 */
BLERemoteDescriptor BLERemoteCharacteristic::getDescriptor(const BLEUUID &uuid) {
  BLE_CHECK_IMPL(BLERemoteDescriptor());

  if (!impl.descsDiscovered) {
    if (!isGattConnected(impl.connHandle)) {
      return BLERemoteDescriptor();
    }

    auto svc = impl.service;
    uint16_t endHandle = svc ? svc->endHandle : 0xFFFF;

    impl.dscDiscoverSync.take();
    int rc = ble_gattc_disc_all_dscs(impl.connHandle, impl.valHandle, endHandle, Impl::dscDiscoveryCb, _impl.get());
    if (rc != 0) {
      impl.dscDiscoverSync.give(BTStatus::Fail);
      return BLERemoteDescriptor();
    }
    BTStatus status = impl.dscDiscoverSync.wait(10000);
    if (status == BTStatus::OK) {
      impl.descsDiscovered = true;
      for (auto &d : impl.descriptors) {
        d->connHandle = impl.connHandle;
        d->chr = _impl.get();
      }
    }
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
 * @brief Returns all cached descriptors, triggering discovery on first use when connected.
 * @return Vector of remote descriptors; may be empty.
 * @note May call @ref getDescriptor with a zero UUID to force the same one-time @c ble_gattc_disc_all_dscs when the cache is empty.
 */
std::vector<BLERemoteDescriptor> BLERemoteCharacteristic::getDescriptors() const {
  std::vector<BLERemoteDescriptor> result;
  BLE_CHECK_IMPL(result);

  // Lazy discovery: if the caller never triggered a targeted getDescriptor(uuid),
  // the per-characteristic descriptor cache is still empty. Trigger discovery
  // transparently so the aggregate getter behaves symmetrically with the targeted one.
  if (!impl.descsDiscovered && isGattConnected(impl.connHandle)) {
    auto *self = const_cast<BLERemoteCharacteristic *>(this);
    (void)self->getDescriptor(BLEUUID(static_cast<uint16_t>(0)));
  }

  for (auto &d : impl.descriptors) {
    result.push_back(BLERemoteDescriptor(d));
  }
  return result;
}

/**
 * @brief GATT read callback: appends data for long reads; signals completion on @c BLE_HS_EDONE or error.
 * @param connHandle Connection handle.
 * @param error GATT error status.
 * @param attr Attribute data for a fragment, or nullptr when finishing.
 * @param arg Opaque @c BLERemoteCharacteristic::Impl pointer.
 * @return Zero to continue; NimBLE callback convention.
 * @note Long read: multiple invocations with status 0 append to @c lastValue; final give uses @c BLE_HS_EDONE.
 */
int BLERemoteCharacteristic::Impl::readCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  auto *impl = static_cast<BLERemoteCharacteristic::Impl *>(arg);
  if (!impl) {
    return 0;
  }

  impl->lastReadRC = error->status;

  if (error->status == 0 && attr != nullptr) {
    uint16_t len = OS_MBUF_PKTLEN(attr->om);
    size_t offset = impl->lastValue.size();
    impl->lastValue.resize(offset + len);
    os_mbuf_copydata(attr->om, 0, len, impl->lastValue.data() + offset);
    return 0;
  }

  impl->readSync.give((error->status == BLE_HS_EDONE) ? BTStatus::OK : BTStatus::Fail);
  return 0;
}

/**
 * @brief GATT write callback for short and long (prepared) writes; signals the write sync.
 * @param connHandle Connection handle.
 * @param error GATT error; @c BLE_HS_EDONE on long-write execute complete.
 * @param attr Optional attribute; may be null.
 * @param arg Opaque @c BLERemoteCharacteristic::Impl pointer.
 * @return Zero; NimBLE callback convention.
 * @note Long write: fires for each prepared segment and the execute; first successful give releases the waiter.
 */
int BLERemoteCharacteristic::Impl::writeCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  auto *impl = static_cast<BLERemoteCharacteristic::Impl *>(arg);
  if (!impl) {
    return 0;
  }
  impl->lastWriteRC = error->status;

  // Signal on every callback. For long writes (ble_gattc_write_long), the
  // callback fires for each prepared write and for the execute write, all
  // with status 0 on success.  The first callback wakes the waiter;
  // subsequent ones are harmless no-ops (the BLESync waiter is already
  // cleared after the first give/wait pair completes).
  if (error->status == 0 || error->status == BLE_HS_EDONE) {
    impl->writeSync.give(BTStatus::OK);
  } else {
    impl->writeSync.give(BTStatus::Fail);
  }
  return 0;
}

/**
 * @brief GATT callback for @c ble_gattc_disc_all_dscs: collects descriptors and signals on completion.
 * @param connHandle Connection handle.
 * @param error GATT error; @c BLE_HS_EDONE at end of discovery.
 * @param chrHandle Defining characteristic handle.
 * @param dsc Descriptor info, or nullptr on completion.
 * @param arg Opaque @c BLERemoteCharacteristic::Impl pointer.
 * @return Zero; NimBLE callback convention.
 */
int BLERemoteCharacteristic::Impl::dscDiscoveryCb(
  uint16_t connHandle, const struct ble_gatt_error *error, uint16_t chrHandle, const struct ble_gatt_dsc *dsc, void *arg
) {
  auto *impl = static_cast<BLERemoteCharacteristic::Impl *>(arg);
  if (!impl) {
    return 0;
  }

  if (error->status == 0 && dsc != nullptr) {
    auto dImpl = std::make_shared<BLERemoteDescriptor::Impl>();
    dImpl->uuid = nimbleToUuid(dsc->uuid);
    dImpl->handle = dsc->handle;
    impl->descriptors.push_back(dImpl);
    return 0;
  }

  impl->dscDiscoverSync.give((error->status == BLE_HS_EDONE) ? BTStatus::OK : BTStatus::Fail);
  return 0;
}

/**
 * @brief Static GATT client notification callback: copies data and dispatches the user @c notifyCb.
 * @param connHandle Connection handle.
 * @param error GATT error.
 * @param attr Incoming notification payload.
 * @param arg Opaque @c BLERemoteCharacteristic::Impl pointer.
 * @return Zero; NimBLE callback convention.
 */
int BLERemoteCharacteristic::Impl::notifyCb_static(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  auto *impl = static_cast<BLERemoteCharacteristic::Impl *>(arg);
  if (!impl || !attr) {
    return 0;
  }

  uint16_t len = OS_MBUF_PKTLEN(attr->om);
  std::vector<uint8_t> data(len);
  os_mbuf_copydata(attr->om, 0, len, data.data());

  impl->lastValue = data;

  if (impl->notifyCb) {
    BLERemoteCharacteristic chr{impl->shared_from_this()};
    impl->notifyCb(chr, data.data(), data.size(), false);
  }
  return 0;
}

/**
 * @brief Inserts or updates a mapping from (connection, attribute handle) to a characteristic for notify routing.
 * @param connHandle GAP connection handle.
 * @param attrHandle Value handle receiving notifications.
 * @param impl Shared implementation object.
 */
void BLERemoteCharacteristic::Impl::registerForNotify(uint16_t connHandle, uint16_t attrHandle, const std::shared_ptr<BLERemoteCharacteristic::Impl> &impl) {
  BLELockGuard lock(sNotifyMtx);
  for (auto &entry : sNotifyRegistry) {
    if (entry.connHandle == connHandle && entry.attrHandle == attrHandle) {
      entry.chr = impl;
      return;
    }
  }
  sNotifyRegistry.push_back({connHandle, attrHandle, impl});
}

/**
 * @brief Removes the notify route for the given connection and value handle.
 * @param connHandle GAP connection handle.
 * @param attrHandle Value handle to unregister.
 */
void BLERemoteCharacteristic::Impl::unregisterForNotify(uint16_t connHandle, uint16_t attrHandle) {
  BLELockGuard lock(sNotifyMtx);
  for (auto it = sNotifyRegistry.begin(); it != sNotifyRegistry.end(); ++it) {
    if (it->connHandle == connHandle && it->attrHandle == attrHandle) {
      sNotifyRegistry.erase(it);
      return;
    }
  }
}

/**
 * @brief Dispatches a received notify/indicate payload from the GAP/GATT path to the registered user callback.
 * @param connHandle GAP connection handle.
 * @param attrHandle Value handle the data belongs to.
 * @param om Incoming mbuf.
 * @param isNotify True for notification, false for indication-ack-style delivery through this path when applicable.
 */
void BLERemoteCharacteristic::Impl::handleNotifyRx(uint16_t connHandle, uint16_t attrHandle, struct os_mbuf *om, bool isNotify) {
  std::shared_ptr<BLERemoteCharacteristic::Impl> implPtr;
  {
    BLELockGuard lock(sNotifyMtx);
    for (auto &entry : sNotifyRegistry) {
      if (entry.connHandle == connHandle && entry.attrHandle == attrHandle) {
        implPtr = entry.chr;
        break;
      }
    }
  }
  if (!implPtr) {
    return;
  }

  uint16_t len = OS_MBUF_PKTLEN(om);
  std::vector<uint8_t> data(len);
  os_mbuf_copydata(om, 0, len, data.data());

  implPtr->lastValue = data;

  if (implPtr->notifyCb) {
    BLERemoteCharacteristic chr{implPtr};
    implPtr->notifyCb(chr, data.data(), data.size(), isNotify);
  }
}

#else /* !BLE_GATT_CLIENT_SUPPORTED -- stubs */

// Stubs for BLE_GATT_CLIENT_SUPPORTED == 0: false/empty/NotSupported/ nullptr; log on side-effecting
// paths. can*() stays silent to avoid log spam in polling.

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
  log_w("GATT client not supported");
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
  log_w("GATT client not supported");
  return BLERemoteDescriptor();
}

std::vector<BLERemoteDescriptor> BLERemoteCharacteristic::getDescriptors() const {
  log_w("GATT client not supported");
  return {};
}

#endif /* BLE_GATT_CLIENT_SUPPORTED */

#endif /* BLE_NIMBLE */
