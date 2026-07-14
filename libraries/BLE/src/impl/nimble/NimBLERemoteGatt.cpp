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
 * @file NimBLERemoteGatt.cpp
 * @brief NimBLE GATT client: remote service, characteristic, and descriptor implementations.
 */

#include "impl/common/BLEGuards.h"
#if BLE_NIMBLE

#include "BLE.h"
#include "NimBLERemoteGatt.h"
#include "NimBLEUUID.h"
#include "esp32-hal-log.h"
#include "impl/common/BLEImplHelpers.h"
#if BLE_GATT_CLIENT_SUPPORTED

// ==========================================================================
// BLERemoteService - remote service discovery and characteristic lookup
// ==========================================================================

// getClient() is stack-agnostic and lives in the common src/BLERemoteService.cpp.

// Blocking: runs a synchronous ble_gattc_disc_all_chrs discovery on first lookup.
BLERemoteCharacteristic BLERemoteService::getCharacteristic(const BLEUUID &uuid) {
  BLE_CHECK_IMPL(BLERemoteCharacteristic());

  if (!impl.characteristicsDiscovered) {
    if (!nimbleIsGattConnected(impl.connHandle)) {
      return BLERemoteCharacteristic();
    }

    impl.chrDiscoverSync.take();
    int rc = ble_gattc_disc_all_chrs(impl.connHandle, impl.startHandle, impl.endHandle, BLERemoteService::Impl::chrDiscoveryCb, _impl.get());
    if (rc != 0) {
      impl.chrDiscoverSync.give(BTStatus::Fail);
      return BLERemoteCharacteristic();
    }
    BTStatus status = impl.chrDiscoverSync.wait(10000);
    if (status == BTStatus::OK) {
      impl.characteristicsDiscovered = true;
      for (auto &c : impl.characteristics) {
        c->connHandle = impl.connHandle;
        c->service = _impl.get();
      }
    }
  }

  for (auto &c : impl.characteristics) {
    if (c->uuid == uuid) {
      return BLERemoteCharacteristic(c);
    }
  }
  return BLERemoteCharacteristic();
}

// Blocking: may run the same discovery as getCharacteristic() when the cache is empty.
std::vector<BLERemoteCharacteristic> BLERemoteService::getCharacteristics() const {
  std::vector<BLERemoteCharacteristic> result;
  BLE_CHECK_IMPL(result);

  // Lazy discovery: if the caller never triggered a targeted getCharacteristic(uuid),
  // NimBLE's per-service characteristic cache is still empty. Trigger discovery
  // transparently so the aggregate getter behaves symmetrically with the targeted one.
  if (!impl.characteristicsDiscovered && nimbleIsGattConnected(impl.connHandle)) {
    auto *self = const_cast<BLERemoteService *>(this);
    (void)self->getCharacteristic(BLEUUID(static_cast<uint16_t>(0)));
  }

  for (auto &c : impl.characteristics) {
    result.push_back(BLERemoteCharacteristic(c));
  }
  return result;
}

/**
 * @brief NimBLE callback for GATT characteristic discovery; fills the per-service list and signals completion.
 * @param connHandle Connection handle (unused for validation here).
 * @param error GATT error status; final completion uses @c BLE_HS_EDONE.
 * @param chr Discovered characteristic field, or nullptr when the iteration completes.
 * @param arg Opaque @c BLERemoteService::Impl pointer.
 * @return Zero to continue; stack convention for NimBLE GATT client callbacks.
 */
int BLERemoteService::Impl::chrDiscoveryCb(uint16_t connHandle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg) {
  auto *impl = static_cast<BLERemoteService::Impl *>(arg);
  if (!impl) {
    return 0;
  }

  if (error->status == 0 && chr != nullptr) {
    auto cImpl = std::make_shared<BLERemoteCharacteristic::Impl>();
    cImpl->uuid = nimbleUuidToPublic(chr->uuid);
    cImpl->defHandle = chr->def_handle;
    cImpl->handle = chr->val_handle;
    cImpl->properties = chr->properties;
    impl->characteristics.push_back(cImpl);
    return 0;
  }

  if (error->status == BLE_HS_EDONE) {
    impl->chrDiscoverSync.give(BTStatus::OK);
  } else {
    impl->chrDiscoverSync.give(BTStatus::Fail);
  }
  return 0;
}

#else /* !BLE_GATT_CLIENT_SUPPORTED -- stubs */

// Stubs for BLE_GATT_CLIENT_SUPPORTED == 0: log, return empty client/characteristic or lists.

BLEClient BLERemoteService::getClient() const {
  log_w("GATT client not supported");
  return BLEClient();
}

BLERemoteCharacteristic BLERemoteService::getCharacteristic(const BLEUUID &) {
  log_w("GATT client not supported");
  return BLERemoteCharacteristic();
}

std::vector<BLERemoteCharacteristic> BLERemoteService::getCharacteristics() const {
  log_w("GATT client not supported");
  return {};
}

#endif /* BLE_GATT_CLIENT_SUPPORTED */

// ==========================================================================
// BLERemoteCharacteristic - remote characteristic read/write/notify/discovery
// ==========================================================================

#if BLE_GATT_CLIENT_SUPPORTED

#include <host/ble_gap.h>
#include "impl/common/BLEMutex.h"
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

// getHandle / can*() / readRawData are stack-agnostic and live in the common src/BLERemoteCharacteristic.cpp.

// Blocking: ble_gattc_read_long / ble_gattc_read with synchronous wait; long read may be retried after a security upgrade.
bool BLERemoteCharacteristic::readInto(uint32_t timeoutMs) {
  if (!_impl || !nimbleIsGattConnected(_impl->connHandle)) {
    return false;
  }

  log_d("RemoteCharacteristic %s: read (conn=%u)", _impl->uuid.toString().c_str(), _impl->connHandle);
  for (int retry = 0; retry < 2; retry++) {
    _impl->value.clear();
    _impl->lastAttError = 0;
    _impl->readSync.take();

    int rc = ble_gattc_read_long(_impl->connHandle, _impl->handle, 0, BLERemoteCharacteristic::Impl::readCb, _impl.get());
    if (rc != 0) {
      log_e("RemoteCharacteristic %s: ble_gattc_read_long rc=%d", _impl->uuid.toString().c_str(), rc);
      _impl->readSync.give(BTStatus::Fail);
      return false;
    }

    BTStatus status = _impl->readSync.wait(timeoutMs);

    if (status != BTStatus::OK && _impl->lastAttError == BLE_HS_ATT_ERR(BLE_ATT_ERR_ATTR_NOT_LONG)) {
      _impl->value.clear();
      _impl->lastAttError = 0;
      _impl->readSync.take();
      rc = ble_gattc_read(_impl->connHandle, _impl->handle, BLERemoteCharacteristic::Impl::readCb, _impl.get());
      if (rc != 0) {
        log_e("RemoteCharacteristic %s: ble_gattc_read fallback rc=%d", _impl->uuid.toString().c_str(), rc);
        _impl->readSync.give(BTStatus::Fail);
        return false;
      }
      status = _impl->readSync.wait(timeoutMs);
    }

    if (status == BTStatus::OK) {
      return true;
    }

    if (retry == 0 && isAuthError(_impl->lastAttError)) {
      log_d("RemoteCharacteristic %s: auth error on read, initiating security", _impl->uuid.toString().c_str());
      if (!initiateSecurityAndWait(_impl->connHandle)) {
        log_w("RemoteCharacteristic %s: security initiation failed", _impl->uuid.toString().c_str());
        return false;
      }
      continue;
    }
    break;
  }
  log_w("RemoteCharacteristic %s: read failed (rc=%d)", _impl->uuid.toString().c_str(), _impl->lastAttError);
  return false;
}

// Blocking: ble_gattc_write_flat, ble_gattc_write_long (prepared write/execute), or ble_gattc_write_no_rsp_flat; confirmed paths wait on the write sync.
BTStatus BLERemoteCharacteristic::writeValue(const uint8_t *data, size_t len, bool withResponse) {
  if (!_impl || !nimbleIsGattConnected(_impl->connHandle)) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: write len=%u withResponse=%d (conn=%u)", _impl->uuid.toString().c_str(), len, withResponse, _impl->connHandle);
  uint16_t mtu = ble_att_mtu(_impl->connHandle);
  uint16_t maxSingle = (mtu > 3) ? (mtu - 3) : 0;

  if (!withResponse && len <= maxSingle) {
    int rc = ble_gattc_write_no_rsp_flat(_impl->connHandle, _impl->handle, data, len);
    if (rc != 0) {
      log_e("RemoteCharacteristic %s: write no-rsp failed rc=%d", _impl->uuid.toString().c_str(), rc);
    }
    return (rc == 0) ? BTStatus::OK : BTStatus::Fail;
  }

  bool isLong = (len > maxSingle);

  for (int retry = 0; retry < 2; retry++) {
    _impl->lastAttError = 0;
    _impl->writeSync.take();

    int rc;
    if (isLong) {
      struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
      if (!om) {
        _impl->writeSync.give(BTStatus::NoMemory);
        return BTStatus::NoMemory;
      }
      rc = ble_gattc_write_long(_impl->connHandle, _impl->handle, 0, om, BLERemoteCharacteristic::Impl::writeCb, _impl.get());
    } else {
      rc = ble_gattc_write_flat(_impl->connHandle, _impl->handle, data, len, BLERemoteCharacteristic::Impl::writeCb, _impl.get());
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

    if (retry == 0 && isAuthError(_impl->lastAttError)) {
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

// Blocking: CCCD written with ble_gattc_write_flat and a synchronous wait; registers the notify path before the write.
BTStatus BLERemoteCharacteristic::subscribe(bool notifications, NotifyCallback callback) {
  if (!_impl || !nimbleIsGattConnected(_impl->connHandle)) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: subscribe %s (conn=%u)", _impl->uuid.toString().c_str(), notifications ? "notify" : "indicate", _impl->connHandle);
  {
    // onNotifyCb is read by the host task in the RX path; guard it with the same
    // mutex used for the notify registry.
    BLELockGuard lock(sNotifyMtx);
    _impl->onNotifyCb = callback;
  }

  BLERemoteCharacteristic::Impl::registerForNotify(_impl->connHandle, _impl->handle, _impl);

  uint16_t cccdVal = notifications ? BLE_CCCD_NOTIFY : BLE_CCCD_INDICATE;
  uint16_t cccdHandle = _impl->handle + 1;

  _impl->writeSync.take();
  int rc = ble_gattc_write_flat(_impl->connHandle, cccdHandle, &cccdVal, sizeof(cccdVal), BLERemoteCharacteristic::Impl::writeCb, _impl.get());
  if (rc != 0) {
    log_e("RemoteCharacteristic %s: subscribe CCCD write failed rc=%d", _impl->uuid.toString().c_str(), rc);
    BLERemoteCharacteristic::Impl::unregisterForNotify(_impl->connHandle, _impl->handle);
    _impl->writeSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }

  BTStatus status = _impl->writeSync.wait(5000);
  if (status != BTStatus::OK) {
    log_w("RemoteCharacteristic %s: subscribe failed/timed out", _impl->uuid.toString().c_str());
    BLERemoteCharacteristic::Impl::unregisterForNotify(_impl->connHandle, _impl->handle);
  } else {
    log_i("RemoteCharacteristic %s: subscribed (%s)", _impl->uuid.toString().c_str(), notifications ? "notify" : "indicate");
  }
  return status;
}

// Blocking: ble_gattc_write_flat to the CCCD handle (value handle + 1) with wait.
BTStatus BLERemoteCharacteristic::unsubscribe() {
  if (!_impl || !nimbleIsGattConnected(_impl->connHandle)) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: unsubscribe (conn=%u)", _impl->uuid.toString().c_str(), _impl->connHandle);
  BLERemoteCharacteristic::Impl::unregisterForNotify(_impl->connHandle, _impl->handle);
  {
    BLELockGuard lock(sNotifyMtx);
    _impl->onNotifyCb = nullptr;
  }

  uint16_t cccdVal = 0x0000;
  uint16_t cccdHandle = _impl->handle + 1;

  _impl->writeSync.take();
  int rc = ble_gattc_write_flat(_impl->connHandle, cccdHandle, &cccdVal, sizeof(cccdVal), BLERemoteCharacteristic::Impl::writeCb, _impl.get());
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

// Blocking: ble_gattc_disc_all_dscs between value and end handle with sync wait; assumes CCCD at handle+1 for the subscribe path elsewhere.
BLERemoteDescriptor BLERemoteCharacteristic::getDescriptor(const BLEUUID &uuid) {
  BLE_CHECK_IMPL(BLERemoteDescriptor());

  if (!impl.descriptorsDiscovered) {
    if (!nimbleIsGattConnected(impl.connHandle)) {
      return BLERemoteDescriptor();
    }

    auto svc = impl.service;
    uint16_t endHandle = svc ? svc->endHandle : 0xFFFF;

    impl.dscDiscoverSync.take();
    int rc = ble_gattc_disc_all_dscs(impl.connHandle, impl.handle, endHandle, BLERemoteCharacteristic::Impl::dscDiscoveryCb, _impl.get());
    if (rc != 0) {
      impl.dscDiscoverSync.give(BTStatus::Fail);
      return BLERemoteDescriptor();
    }
    BTStatus status = impl.dscDiscoverSync.wait(10000);
    if (status == BTStatus::OK) {
      impl.descriptorsDiscovered = true;
      for (auto &d : impl.descriptors) {
        d->connHandle = impl.connHandle;
        d->chr = _impl.get();
      }
    }
  }

  for (auto &d : impl.descriptors) {
    if (d->uuid == uuid) {
      return BLERemoteDescriptor(d);
    }
  }
  return BLERemoteDescriptor();
}

// May call getDescriptor with a zero UUID to force the same one-time ble_gattc_disc_all_dscs when the cache is empty.
std::vector<BLERemoteDescriptor> BLERemoteCharacteristic::getDescriptors() const {
  std::vector<BLERemoteDescriptor> result;
  BLE_CHECK_IMPL(result);

  // Lazy discovery: if the caller never triggered a targeted getDescriptor(uuid),
  // the per-characteristic descriptor cache is still empty. Trigger discovery
  // transparently so the aggregate getter behaves symmetrically with the targeted one.
  if (!impl.descriptorsDiscovered && nimbleIsGattConnected(impl.connHandle)) {
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
 * @note Long read: multiple invocations with status 0 append to @c value; final give uses @c BLE_HS_EDONE.
 */
int BLERemoteCharacteristic::Impl::readCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  auto *impl = static_cast<BLERemoteCharacteristic::Impl *>(arg);
  if (!impl) {
    return 0;
  }

  impl->lastAttError = error->status;

  if (error->status == 0 && attr != nullptr) {
    uint16_t len = OS_MBUF_PKTLEN(attr->om);
    size_t offset = impl->value.size();
    impl->value.resize(offset + len);
    os_mbuf_copydata(attr->om, 0, len, impl->value.data() + offset);
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
  impl->lastAttError = error->status;

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
    dImpl->uuid = nimbleUuidToPublic(dsc->uuid);
    dImpl->handle = dsc->handle;
    impl->descriptors.push_back(dImpl);
    return 0;
  }

  impl->dscDiscoverSync.give((error->status == BLE_HS_EDONE) ? BTStatus::OK : BTStatus::Fail);
  return 0;
}

/**
 * @brief Static GATT client notification callback: copies data and dispatches the user @c onNotifyCb.
 * @param connHandle Connection handle.
 * @param error GATT error.
 * @param attr Incoming notification payload.
 * @param arg Opaque @c BLERemoteCharacteristic::Impl pointer.
 * @return Zero; NimBLE callback convention.
 */
int BLERemoteCharacteristic::Impl::onNotifyCb_static(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  auto *impl = static_cast<BLERemoteCharacteristic::Impl *>(arg);
  if (!impl || !attr) {
    return 0;
  }

  uint16_t len = OS_MBUF_PKTLEN(attr->om);
  impl->value.resize(len);
  os_mbuf_copydata(attr->om, 0, len, impl->value.data());

  BLERemoteCharacteristic::NotifyCallback cb;
  {
    BLELockGuard lock(sNotifyMtx);
    cb = impl->onNotifyCb;
  }
  if (cb) {
    BLERemoteCharacteristic chr = BLERemoteCharacteristicImplCommon::makeHandle(impl->shared_from_this());
    cb(chr, impl->value.data(), impl->value.size(), false);
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
  BLERemoteCharacteristic::NotifyCallback cb;
  {
    BLELockGuard lock(sNotifyMtx);
    for (auto &entry : sNotifyRegistry) {
      if (entry.connHandle == connHandle && entry.attrHandle == attrHandle) {
        implPtr = entry.chr;
        cb = implPtr->onNotifyCb;
        break;
      }
    }
  }
  if (!implPtr) {
    return;
  }

  uint16_t len = OS_MBUF_PKTLEN(om);
  implPtr->value.resize(len);
  os_mbuf_copydata(om, 0, len, implPtr->value.data());

  if (cb) {
    BLERemoteCharacteristic chr = BLERemoteCharacteristicImplCommon::makeHandle(implPtr);
    cb(chr, implPtr->value.data(), implPtr->value.size(), isNotify);
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

bool BLERemoteCharacteristic::readInto(uint32_t) {
  log_w("GATT client not supported");
  return false;
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

// ==========================================================================
// BLERemoteDescriptor - remote descriptor read/write
// ==========================================================================

#if BLE_GATT_CLIENT_SUPPORTED

// ==========================================================================
// BLERemoteDescriptor implementation
// ==========================================================================

// Blocking: ble_gattc_read plus synchronous wait; safe only when the connection is in a valid GATT state.
bool BLERemoteDescriptor::readInto(uint32_t timeoutMs) {
  if (!_impl || !nimbleIsGattConnected(_impl->connHandle)) {
    return false;
  }

  _impl->value.clear();
  _impl->readSync.take();

  int rc = ble_gattc_read(_impl->connHandle, _impl->handle, BLERemoteDescriptor::Impl::readCb, _impl.get());
  if (rc != 0) {
    log_e("RemoteDescriptor: ble_gattc_read rc=%d", rc);
    _impl->readSync.give(BTStatus::Fail);
    return false;
  }

  BTStatus status = _impl->readSync.wait(timeoutMs);
  if (status != BTStatus::OK) {
    log_w("RemoteDescriptor: read failed/timed out (handle=0x%04x)", _impl->handle);
    return false;
  }
  return true;
}

// Blocking: confirmed path uses ble_gattc_write_flat and waits; CCCD-style descriptor writes follow the same contract as characteristic writes.
BTStatus BLERemoteDescriptor::writeValue(const uint8_t *data, size_t len, bool withResponse) {
  if (!_impl || !nimbleIsGattConnected(_impl->connHandle)) {
    log_w("RemoteDescriptor: writeValue called but not connected");
    return BTStatus::InvalidState;
  }

  if (!withResponse) {
    int rc = ble_gattc_write_no_rsp_flat(_impl->connHandle, _impl->handle, data, len);
    if (rc != 0) {
      log_e("RemoteDescriptor: write no-rsp failed rc=%d", rc);
      return BTStatus::Fail;
    }
    return BTStatus::OK;
  }

  _impl->writeSync.take();
  int rc = ble_gattc_write_flat(_impl->connHandle, _impl->handle, data, len, BLERemoteDescriptor::Impl::writeCb, _impl.get());
  if (rc != 0) {
    log_e("RemoteDescriptor: ble_gattc_write_flat rc=%d", rc);
    _impl->writeSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  BTStatus status = _impl->writeSync.wait(5000);
  if (status != BTStatus::OK) {
    log_w("RemoteDescriptor: write timed out (handle=0x%04x)", _impl->handle);
  }
  return status;
}

/**
 * @brief NimBLE callback completing a GATT read for a remote descriptor.
 * @param connHandle Connection handle.
 * @param error GATT error from the stack.
 * @param attr Read attribute payload, or nullptr on error paths.
 * @param arg Opaque @c BLERemoteDescriptor::Impl pointer.
 * @return Zero (NimBLE GATT client callback convention).
 */
int BLERemoteDescriptor::Impl::readCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  auto *impl = static_cast<BLERemoteDescriptor::Impl *>(arg);
  if (!impl) {
    return 0;
  }

  if (error->status == 0 && attr != nullptr) {
    uint16_t len = OS_MBUF_PKTLEN(attr->om);
    impl->value.resize(len);
    os_mbuf_copydata(attr->om, 0, len, impl->value.data());
    impl->readSync.give(BTStatus::OK);
  } else {
    impl->readSync.give((error->status == BLE_HS_EDONE) ? BTStatus::OK : BTStatus::Fail);
  }
  return 0;
}

/**
 * @brief NimBLE callback completing a GATT write (including CCCD) for a remote descriptor.
 * @param connHandle Connection handle.
 * @param error GATT error from the stack.
 * @param attr Optional attribute; may be null on some paths.
 * @param arg Opaque @c BLERemoteDescriptor::Impl pointer.
 * @return Zero (NimBLE GATT client callback convention).
 */
int BLERemoteDescriptor::Impl::writeCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  auto *impl = static_cast<BLERemoteDescriptor::Impl *>(arg);
  if (!impl) {
    return 0;
  }
  impl->writeSync.give((error->status == 0) ? BTStatus::OK : BTStatus::Fail);
  return 0;
}

#else /* !BLE_GATT_CLIENT_SUPPORTED -- stubs */

// Stubs for BLE_GATT_CLIENT_SUPPORTED == 0: empty read, NotSupported write.

bool BLERemoteDescriptor::readInto(uint32_t) {
  log_w("GATT client not supported");
  return false;
}

BTStatus BLERemoteDescriptor::writeValue(const uint8_t *, size_t, bool) {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

#endif /* BLE_GATT_CLIENT_SUPPORTED */

#endif /* BLE_NIMBLE */
