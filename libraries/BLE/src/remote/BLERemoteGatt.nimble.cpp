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
 * @file BLERemoteGatt.nimble.cpp
 * @brief NimBLE GATT client: remote service, characteristic, and descriptor implementations.
 */

#include "core/BLEGuards.h"
#if BLE_NIMBLE

#include "BLE.h"
#include "remote/BLERemoteGatt.nimble.h"
#include "types/BLEUUID.nimble.h"
#include "esp32-hal-log.h"
#include "core/BLEImplHelpers.h"
#include "core/BLEMutex.h"
#if BLE_GATT_CLIENT_SUPPORTED

namespace {

bool remoteServiceConnected(const BLERemoteService::Impl *service, uint16_t connHandle) {
  auto *client = service ? service->client : nullptr;
  if (!client) {
    return false;
  }
  BLELockGuard lock(client->mtx);
  return client->connected && client->connectionGeneration.load() == service->clientGeneration && client->connHandle == connHandle
         && nimbleIsGattConnected(connHandle);
}

bool remoteConnected(const std::shared_ptr<BLERemoteService::Impl> &service, uint16_t connHandle) {
  return remoteServiceConnected(service.get(), connHandle);
}

bool remoteConnected(const std::shared_ptr<BLERemoteCharacteristic::Impl> &chr, uint16_t connHandle) {
  return chr && remoteServiceConnected(chr->service, connHandle);
}

bool remoteConnected(const std::shared_ptr<BLERemoteDescriptor::Impl> &desc, uint16_t connHandle) {
  auto *chr = desc ? desc->chr : nullptr;
  return chr && remoteServiceConnected(chr->service, connHandle);
}

}  // namespace

// ==========================================================================
// BLERemoteService - remote service discovery and characteristic lookup
// ==========================================================================

// getClient() is stack-agnostic and lives in the common src/BLERemoteService.cpp.

// Blocking: runs a synchronous ble_gattc_disc_all_chrs discovery on first lookup.
BLERemoteCharacteristic BLERemoteService::getCharacteristic(const BLEUUID &uuid) {
  BLE_CHECK_IMPL(BLERemoteCharacteristic());

  if (!impl.characteristicsDiscovered) {
    if (!remoteConnected(_impl, impl.connHandle)) {
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
  if (!impl.characteristicsDiscovered && remoteConnected(_impl, impl.connHandle)) {
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

// Stubs for BLE_GATT_CLIENT_SUPPORTED == 0: log, return empty characteristic or lists.

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
#include <new>
#include <utility>
#include <vector>

struct NotifyEntry {
  uint16_t connHandle;
  uint16_t attrHandle;
  BLERemoteCharacteristic::Impl *chr;
};

static SemaphoreHandle_t sNotifyMtx = xSemaphoreCreateRecursiveMutex();
static std::vector<NotifyEntry> sNotifyRegistry;

void (*BLERemoteCharacteristic::Impl::clearNotifyRoutesHook)(uint16_t connHandle) = nullptr;
void (*BLERemoteCharacteristic::Impl::handleNotifyRxHook)(uint16_t connHandle, uint16_t attrHandle, struct os_mbuf *om, bool isNotify) = nullptr;

BLERemoteCharacteristic::Impl::~Impl() {
  if (clearNotifyRoutesHook) {
    unregisterForNotify(connHandle, handle);
  }
}

/**
 * @brief Tests whether a NimBLE ATT return code indicates missing authentication or encryption.
 * @param rc Stack return or ATT error code.
 * @return True if pairing/encryption can clear the error.
 * @note Insufficient authorization is an application deny; retrying SMP will not help.
 */
static bool isAuthError(int rc) {
  return rc == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHEN) || rc == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_ENC);
}

/**
 * @brief Tells whether a final @c ble_gattc_read_long status completes the read successfully.
 * @param status Final callback status.
 * @param received Bytes received so far.
 * @return True on @c BLE_HS_EDONE, or on Attribute Not Long once data was received.
 * @note NimBLE sends a Read Blob whenever a response fills ATT_MTU - 1 octets. A server may reject
 *       it with Attribute Not Long when the value is exactly that size (Core Vol 3 Part F
 *       §3.4.4.5); the first response already held the whole value, as Bluedroid also assumes.
 */
static bool nimbleLongReadDone(int status, const std::vector<uint8_t> &received) {
  return status == BLE_HS_EDONE || (status == BLE_HS_ATT_ERR(BLE_ATT_ERR_ATTR_NOT_LONG) && !received.empty());
}

struct RemoteReadContext {
  std::shared_ptr<void> owner;
  std::atomic<uint32_t> *readGeneration;
  BLESync *readSync;
  std::vector<uint8_t> *targetValue;
  int *lastAttError;
  uint32_t generation;
  std::vector<uint8_t> value;
};

static int handleRemoteReadCb(uint16_t, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  auto *context = static_cast<RemoteReadContext *>(arg);
  if (!context) {
    return 0;
  }

  if (error->status == 0 && attr != nullptr) {
    uint16_t len = OS_MBUF_PKTLEN(attr->om);
    size_t offset = context->value.size();
    context->value.resize(offset + len);
    os_mbuf_copydata(attr->om, 0, len, context->value.data() + offset);
    return 0;
  }

  bool success = nimbleLongReadDone(error->status, context->value);
  uint32_t expected = context->generation;
  if (context->readGeneration->compare_exchange_strong(expected, expected + 1)) {
    if (context->lastAttError) {
      *context->lastAttError = error->status;
    }
    if (success) {
      *context->targetValue = std::move(context->value);
    }
    context->readSync->give(success ? BTStatus::OK : BTStatus::Fail);
  }
  delete context;
  return 0;
}

static BTStatus performRemoteLongRead(
  const std::shared_ptr<void> &owner, uint16_t connHandle, uint16_t attrHandle, std::atomic<uint32_t> &readGeneration, BLESync &readSync,
  std::vector<uint8_t> &targetValue, int *lastAttError, ble_gatt_attr_fn *cb, uint32_t timeoutMs
) {
  targetValue.clear();
  if (lastAttError) {
    *lastAttError = 0;
  }
  uint32_t generation = readGeneration.fetch_add(1) + 1;
  readSync.take();
  auto *context = new (std::nothrow) RemoteReadContext{owner, &readGeneration, &readSync, &targetValue, lastAttError, generation, {}};
  if (!context) {
    readSync.give(BTStatus::NoMemory);
    return BTStatus::NoMemory;
  }

  int rc = ble_gattc_read_long(connHandle, attrHandle, 0, cb, context);
  if (rc != 0) {
    delete context;
    readSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }

  BTStatus status = readSync.wait(timeoutMs);
  if (status == BTStatus::Timeout) {
    uint32_t expected = generation;
    if (!readGeneration.compare_exchange_strong(expected, generation + 1)) {
      // readCb already claimed this generation and is completing right now.
      status = readSync.wait(portMAX_DELAY);
    }
  }
  return status;
}

/**
 * @brief Starts pairing/encryption and blocks until security completes or times out.
 * @param connHandle GAP connection handle.
 * @return True on successful authentication wait; false if security cannot be started or times out.
 */
static bool initiateSecurityAndWait(const std::shared_ptr<BLERemoteCharacteristic::Impl> &chrImpl) {
  auto *svc = chrImpl ? chrImpl->service : nullptr;
  auto *client = svc ? svc->client : nullptr;
  if (!client) {
    return false;
  }
  client->authSync.take();
  int rc = ble_gap_security_initiate(chrImpl->connHandle);
  if (rc != 0) {
    client->authSync.give(BTStatus::Fail);
    return false;
  }
  return client->authSync.wait(10000) == BTStatus::OK;
}

// ==========================================================================
// BLERemoteCharacteristic implementation
// ==========================================================================

// getHandle / can*() / readRawData are stack-agnostic and live in the common src/BLERemoteCharacteristic.cpp.

// Blocking: ble_gattc_read_long (GATT Read Long Characteristic Values) with synchronous wait; may be retried after a security upgrade.
bool BLERemoteCharacteristic::readInto(uint32_t timeoutMs) {
  if (!_impl || !remoteConnected(_impl, _impl->connHandle)) {
    return false;
  }

  log_d("RemoteCharacteristic %s: read (conn=%u)", _impl->uuid.toString().c_str(), _impl->connHandle);
  for (int retry = 0; retry < 2; retry++) {
    BTStatus status = performRemoteLongRead(
      _impl, _impl->connHandle, _impl->handle, _impl->readGeneration, _impl->readSync, _impl->value, &_impl->lastAttError,
      BLERemoteCharacteristic::Impl::readCb, timeoutMs
    );
    if (status == BTStatus::OK) {
      return true;
    }
    if (status == BTStatus::NoMemory) {
      return false;
    }

    if (retry == 0 && isAuthError(_impl->lastAttError)) {
      log_d("RemoteCharacteristic %s: auth error on read, initiating security", _impl->uuid.toString().c_str());
      if (!initiateSecurityAndWait(_impl)) {
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
static BTStatus performRemoteWrite(
  const std::shared_ptr<BLERemoteCharacteristic::Impl> &chrImpl, uint16_t connHandle, uint16_t attrHandle, const uint8_t *data, size_t len, bool withResponse,
  BLESync &writeSync, int *lastAttError, ble_gatt_attr_fn *cb, void *cbArg
) {
  uint16_t mtu = ble_att_mtu(connHandle);
  uint16_t maxSingle = (mtu > 3) ? (mtu - 3) : 0;

  if (!withResponse && len <= maxSingle) {
    int rc = ble_gattc_write_no_rsp_flat(connHandle, attrHandle, data, len);
    if (rc != 0) {
      log_e("Remote write no-rsp failed rc=%d", rc);
    }
    return (rc == 0) ? BTStatus::OK : BTStatus::Fail;
  }

  bool isLong = (len > maxSingle);

  for (int retry = 0; retry < (lastAttError ? 2 : 1); retry++) {
    if (lastAttError) {
      *lastAttError = 0;
    }
    writeSync.take();

    int rc;
    if (isLong) {
      struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
      if (!om) {
        writeSync.give(BTStatus::NoMemory);
        return BTStatus::NoMemory;
      }
      rc = ble_gattc_write_long(connHandle, attrHandle, 0, om, cb, cbArg);
    } else {
      rc = ble_gattc_write_flat(connHandle, attrHandle, data, len, cb, cbArg);
    }

    if (rc != 0) {
      log_e("Remote write rc=%d", rc);
      writeSync.give(BTStatus::Fail);
      return BTStatus::Fail;
    }

    BTStatus status = writeSync.wait(10000);
    if (status == BTStatus::OK) {
      return BTStatus::OK;
    }

    if (retry == 0 && lastAttError && isAuthError(*lastAttError)) {
      log_d("Remote write: auth error, initiating security");
      if (!initiateSecurityAndWait(chrImpl)) {
        log_w("Remote write: security initiation failed on write retry");
        return BTStatus::AuthFailed;
      }
      continue;
    }
    log_w("Remote write failed (status=%d, handle=0x%04x)", static_cast<int>(status), attrHandle);
    return status;
  }
  return BTStatus::Fail;
}

BTStatus BLERemoteCharacteristic::writeValue(const uint8_t *data, size_t len, bool withResponse) {
  if (!_impl || !remoteConnected(_impl, _impl->connHandle)) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: write len=%u withResponse=%d (conn=%u)", _impl->uuid.toString().c_str(), len, withResponse, _impl->connHandle);
  return performRemoteWrite(
    _impl, _impl->connHandle, _impl->handle, data, len, withResponse, _impl->writeSync, &_impl->lastAttError, BLERemoteCharacteristic::Impl::writeCb,
    _impl.get()
  );
}

// Blocking: discovers the CCCD descriptor and writes it with a synchronous wait; registers the notify path before the write.
BTStatus BLERemoteCharacteristic::subscribe(bool notifications, NotifyCallback callback) {
  if (!_impl || !remoteConnected(_impl, _impl->connHandle)) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: subscribe %s (conn=%u)", _impl->uuid.toString().c_str(), notifications ? "notify" : "indicate", _impl->connHandle);
  BLERemoteDescriptor cccd = getDescriptor(BLEUUID(BLE_DSC_UUID16_CCCD));
  if (!cccd) {
    log_w("RemoteCharacteristic %s: CCCD descriptor (0x2902) not found", _impl->uuid.toString().c_str());
    return BTStatus::NotFound;
  }

  {
    // onNotifyCb is read by the host task in the RX path; guard it with the same
    // mutex used for the notify registry.
    BLELockGuard lock(sNotifyMtx);
    _impl->onNotifyCb = std::move(callback);
  }

  BLERemoteCharacteristic::Impl::registerForNotify(_impl->connHandle, _impl->handle, _impl.get());

  uint16_t cccdVal = notifications ? BLE_CCCD_NOTIFY : BLE_CCCD_INDICATE;
  BTStatus status = cccd.writeValue(reinterpret_cast<const uint8_t *>(&cccdVal), sizeof(cccdVal), true);
  if (status != BTStatus::OK) {
    log_w("RemoteCharacteristic %s: subscribe failed/timed out", _impl->uuid.toString().c_str());
    BLERemoteCharacteristic::Impl::unregisterForNotify(_impl->connHandle, _impl->handle);
  } else {
    log_i("RemoteCharacteristic %s: subscribed (%s)", _impl->uuid.toString().c_str(), notifications ? "notify" : "indicate");
  }
  return status;
}

// Blocking: writes 0x0000 to the CCCD descriptor with wait.
BTStatus BLERemoteCharacteristic::unsubscribe() {
  if (!_impl || !remoteConnected(_impl, _impl->connHandle)) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: unsubscribe (conn=%u)", _impl->uuid.toString().c_str(), _impl->connHandle);
  BLERemoteCharacteristic::Impl::unregisterForNotify(_impl->connHandle, _impl->handle);
  {
    BLELockGuard lock(sNotifyMtx);
    _impl->onNotifyCb = nullptr;
  }

  BLERemoteDescriptor cccd = getDescriptor(BLEUUID(BLE_DSC_UUID16_CCCD));
  if (!cccd) {
    log_w("RemoteCharacteristic %s: CCCD descriptor (0x2902) not found for unsubscribe", _impl->uuid.toString().c_str());
    return BTStatus::NotFound;
  }

  uint16_t cccdVal = 0x0000;
  BTStatus status = cccd.writeValue(reinterpret_cast<const uint8_t *>(&cccdVal), sizeof(cccdVal), true);
  if (status != BTStatus::OK) {
    log_w("RemoteCharacteristic %s: unsubscribe timed out", _impl->uuid.toString().c_str());
  }
  return status;
}

// Blocking: ble_gattc_disc_all_dscs within the characteristic's handle range (Core Vol 3 Part G §4.7.1).
BLERemoteDescriptor BLERemoteCharacteristic::getDescriptor(const BLEUUID &uuid) {
  BLE_CHECK_IMPL(BLERemoteDescriptor());

  if (!impl.descriptorsDiscovered) {
    if (!remoteConnected(_impl, impl.connHandle)) {
      return BLERemoteDescriptor();
    }

    auto *svc = impl.service;
    uint16_t endHandle = svc ? svc->endHandle : 0xFFFF;
    if (svc) {
      for (const auto &c : svc->characteristics) {
        if (c->defHandle > impl.handle && static_cast<uint16_t>(c->defHandle - 1) < endHandle) {
          endHandle = static_cast<uint16_t>(c->defHandle - 1);
        }
      }
    }
    if (endHandle <= impl.handle) {
      impl.descriptorsDiscovered = true;
      return BLERemoteDescriptor();
    }

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
  if (!impl.descriptorsDiscovered && remoteConnected(_impl, impl.connHandle)) {
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
 * @param arg Owning per-operation context; released on the terminal callback.
 * @return Zero to continue; NimBLE callback convention.
 * @note Multiple invocations with status 0 append to @c value; the read completes on @c BLE_HS_EDONE,
 *       or on Attribute Not Long after the first response (see @c nimbleLongReadDone).
 */
int BLERemoteCharacteristic::Impl::readCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  return handleRemoteReadCb(connHandle, error, attr, arg);
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
 * @brief Inserts or updates a mapping from (connection, attribute handle) to a characteristic for notify routing.
 * @param connHandle GAP connection handle.
 * @param attrHandle Value handle receiving notifications.
 * @param impl Shared implementation object.
 */
void BLERemoteCharacteristic::Impl::registerForNotify(uint16_t connHandle, uint16_t attrHandle, BLERemoteCharacteristic::Impl *impl) {
  BLELockGuard lock(sNotifyMtx);
  clearNotifyRoutesHook = clearNotifyRoutes;
  handleNotifyRxHook = handleNotifyRx;
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

void BLERemoteCharacteristic::Impl::clearNotifyRoutes(uint16_t connHandle) {
  BLELockGuard lock(sNotifyMtx);
  for (auto it = sNotifyRegistry.begin(); it != sNotifyRegistry.end();) {
    if (it->connHandle == connHandle) {
      it = sNotifyRegistry.erase(it);
    } else {
      ++it;
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
        if (entry.chr) {
          implPtr = entry.chr->shared_from_this();
          cb = entry.chr->onNotifyCb;
        }
        break;
      }
    }
  }
  if (!implPtr || !remoteConnected(implPtr, connHandle)) {
    return;
  }

  uint16_t len = OS_MBUF_PKTLEN(om);
  const uint8_t *dataPtr = nullptr;
  {
    BLELockGuard lock(sNotifyMtx);
    implPtr->value.resize(len);
    if (len > 0) {
      os_mbuf_copydata(om, 0, len, implPtr->value.data());
    }
    dataPtr = implPtr->value.data();
  }

  if (cb) {
    BLERemoteCharacteristic chr = BLERemoteCharacteristicImplCommon::makeHandle(implPtr);
    cb(chr, dataPtr, len, isNotify);
  }
}

#else /* !BLE_GATT_CLIENT_SUPPORTED -- stubs */

// Stubs for BLE_GATT_CLIENT_SUPPORTED == 0: false/empty/NotSupported; log on side-effecting paths.

bool BLERemoteCharacteristic::readInto(uint32_t) {
  log_w("GATT client not supported");
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
  log_w("GATT client not supported");
  return BLERemoteDescriptor();
}

std::vector<BLERemoteDescriptor> BLERemoteCharacteristic::getDescriptors() const {
  log_w("GATT client not supported");
  return {};
}

BLERemoteCharacteristic::Impl::~Impl() = default;
void (*BLERemoteCharacteristic::Impl::clearNotifyRoutesHook)(uint16_t) = nullptr;
void (*BLERemoteCharacteristic::Impl::handleNotifyRxHook)(uint16_t, uint16_t, struct os_mbuf *, bool) = nullptr;

#endif /* BLE_GATT_CLIENT_SUPPORTED */

// ==========================================================================
// BLERemoteDescriptor - remote descriptor read/write
// ==========================================================================

#if BLE_GATT_CLIENT_SUPPORTED

// ==========================================================================
// BLERemoteDescriptor implementation
// ==========================================================================

// Blocking: ble_gattc_read_long (GATT Read Long Characteristic Descriptors) plus synchronous wait;
// safe only when the connection is in a valid GATT state.
bool BLERemoteDescriptor::readInto(uint32_t timeoutMs) {
  if (!_impl || !remoteConnected(_impl, _impl->connHandle)) {
    return false;
  }

  BTStatus status = performRemoteLongRead(
    _impl, _impl->connHandle, _impl->handle, _impl->readGeneration, _impl->readSync, _impl->value, nullptr, BLERemoteDescriptor::Impl::readCb, timeoutMs
  );
  if (status != BTStatus::OK) {
    log_w("RemoteDescriptor: read failed/timed out (handle=0x%04x)", _impl->handle);
    return false;
  }
  return true;
}

// Blocking: confirmed path uses ble_gattc_write_flat or ble_gattc_write_long and waits; CCCD-style descriptor writes follow the same contract as characteristic writes.
BTStatus BLERemoteDescriptor::writeValue(const uint8_t *data, size_t len, bool withResponse) {
  if (!_impl || !remoteConnected(_impl, _impl->connHandle)) {
    log_w("RemoteDescriptor: writeValue called but not connected");
    return BTStatus::InvalidState;
  }
  return performRemoteWrite(
    nullptr, _impl->connHandle, _impl->handle, data, len, withResponse, _impl->writeSync, nullptr, BLERemoteDescriptor::Impl::writeCb, _impl.get()
  );
}

/**
 * @brief NimBLE callback for a remote descriptor read: appends each fragment and signals completion.
 * @param connHandle Connection handle.
 * @param error GATT error from the stack.
 * @param attr Read attribute payload, or nullptr on completion/error paths.
 * @param arg Owning per-operation context; released on the terminal callback.
 * @return Zero (NimBLE GATT client callback convention).
 * @note Same completion rules as @c BLERemoteCharacteristic::Impl::readCb.
 */
int BLERemoteDescriptor::Impl::readCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  return handleRemoteReadCb(connHandle, error, attr, arg);
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
  impl->writeSync.give((error->status == 0 || error->status == BLE_HS_EDONE) ? BTStatus::OK : BTStatus::Fail);
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
