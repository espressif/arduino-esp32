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
#include "core/BLEValidationHooks.h"
#if BLE_GATT_CLIENT_SUPPORTED

namespace {

struct RemoteGattTarget {
  std::shared_ptr<BLERemoteService::Impl> service;
  std::shared_ptr<BLERemoteCharacteristic::Impl> characteristic;
  std::shared_ptr<BLERemoteDescriptor::Impl> descriptor;
};

bool remoteTargetConnectedLocked(const RemoteGattTarget &target, const std::shared_ptr<BLEClient::Impl> &client, uint16_t connHandle) {
  if (!target.service || !client || target.service->clientOwner.lock() != client || target.service->client != client.get() || !client->connected
      || client->connectionGeneration.load() != target.service->clientGeneration || client->connHandle != connHandle || !nimbleIsGattConnected(connHandle)) {
    return false;
  }
  if (target.characteristic && (target.characteristic->serviceOwner.lock() != target.service || target.characteristic->service != target.service.get())) {
    return false;
  }
  return !target.descriptor || (target.descriptor->chrOwner.lock() == target.characteristic && target.descriptor->chr == target.characteristic.get());
}

bool remoteConnected(const RemoteGattTarget &target, uint16_t connHandle) {
  auto client = target.service ? target.service->clientOwner.lock() : nullptr;
  BLELockGuard lock(client ? client->mtx : nullptr);
  return remoteTargetConnectedLocked(target, client, connHandle);
}

bool remoteConnected(const std::shared_ptr<BLERemoteService::Impl> &service, uint16_t connHandle) {
  return remoteConnected({service, {}, {}}, connHandle);
}

bool remoteConnected(const std::shared_ptr<BLERemoteCharacteristic::Impl> &chr, uint16_t connHandle) {
  if (!chr) {
    return false;
  }
  auto service = chr->serviceOwner.lock();
  return remoteConnected({service, chr, {}}, connHandle);
}

bool remoteConnected(const std::shared_ptr<BLERemoteDescriptor::Impl> &desc, uint16_t connHandle) {
  if (!desc) {
    return false;
  }
  auto chr = desc->chrOwner.lock();
  auto service = chr ? chr->serviceOwner.lock() : nullptr;
  return remoteConnected({service, chr, desc}, connHandle);
}

BTStatus waitRemoteOp(std::atomic<uint32_t> &opGeneration, BLESync &opSync, uint32_t generation, uint32_t timeoutMs) {
  BTStatus status = opSync.wait(timeoutMs);
  if (status == BTStatus::Timeout) {
    uint32_t expected = generation;
    if (!opGeneration.compare_exchange_strong(expected, generation + 1)) {
      status = opSync.wait(portMAX_DELAY);
    }
  }
  return status;
}

}  // namespace

// ==========================================================================
// BLERemoteService - remote service discovery and characteristic lookup
// ==========================================================================

// getClient() is stack-agnostic and lives in the common src/BLERemoteService.cpp.

struct RemoteChrDiscContext {
  std::shared_ptr<BLERemoteService::Impl> owner;
  uint32_t generation;
  std::vector<std::shared_ptr<BLERemoteCharacteristic::Impl>> characteristics;
};

BLERemoteService::Impl::~Impl() {
  if (opMtx) {
    vSemaphoreDelete(opMtx);
    opMtx = nullptr;
  }
}

// Blocking: runs a synchronous ble_gattc_disc_all_chrs discovery on first lookup.
BLERemoteCharacteristic BLERemoteService::getCharacteristic(const BLEUUID &uuid) const {
  BLE_CHECK_IMPL(BLERemoteCharacteristic());
  BLELockGuard opLock(impl.opMtx);
  auto client = impl.clientOwner.lock();

  if (!impl.characteristicsDiscovered) {
    uint32_t generation = impl.chrDiscoverGeneration.fetch_add(1) + 1;
    impl.chrDiscoverSync.take();
    auto *context = new (std::nothrow) RemoteChrDiscContext{_impl, generation, {}};
    if (!context) {
      impl.chrDiscoverSync.give(BTStatus::NoMemory);
      return BLERemoteCharacteristic();
    }
    int rc;
    {
      // Keep validation and native submission in one client-generation
      // critical section. Otherwise disconnect/reconnect can recycle the
      // native handle between the check and this call.
      BLELockGuard lock(client ? client->mtx : nullptr);
      if (!remoteTargetConnectedLocked({_impl, {}, {}}, client, impl.connHandle)) {
        delete context;
        impl.chrDiscoverSync.give(BTStatus::InvalidState);
        return BLERemoteCharacteristic();
      }
      rc = ble_gattc_disc_all_chrs(impl.connHandle, impl.startHandle, impl.endHandle, BLERemoteService::Impl::chrDiscoveryCb, context);
    }
    if (rc != 0) {
      delete context;
      impl.chrDiscoverSync.give(BTStatus::Fail);
      return BLERemoteCharacteristic();
    }
    BTStatus status = waitRemoteOp(impl.chrDiscoverGeneration, impl.chrDiscoverSync, generation, 10000);
    if (status != BTStatus::OK) {
      return BLERemoteCharacteristic();
    }
  }

  BLELockGuard lock(client ? client->mtx : nullptr);
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
    (void)getCharacteristic(BLEUUID(static_cast<uint16_t>(0)));
  }

  auto client = impl.clientOwner.lock();
  BLELockGuard lock(client ? client->mtx : nullptr);
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
 * @param arg Owning per-operation discovery context; released on the terminal callback.
 * @return Zero to continue; stack convention for NimBLE GATT client callbacks.
 */
int BLERemoteService::Impl::chrDiscoveryCb(uint16_t connHandle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg) {
  auto *context = static_cast<RemoteChrDiscContext *>(arg);
  if (!context) {
    return 0;
  }

  if (error->status == 0 && chr != nullptr) {
    if (context->owner->chrDiscoverGeneration.load() != context->generation) {
      delete context;
      return BLE_HS_EDONE;
    }
    auto cImpl = std::make_shared<BLERemoteCharacteristic::Impl>();
    cImpl->uuid = nimbleUuidToPublic(chr->uuid);
    cImpl->defHandle = chr->def_handle;
    cImpl->handle = chr->val_handle;
    cImpl->properties = chr->properties;
    context->characteristics.push_back(cImpl);
    return 0;
  }

  auto *impl = context->owner.get();
  uint32_t expected = context->generation;
  if (impl->chrDiscoverGeneration.compare_exchange_strong(expected, expected + 1)) {
    BTStatus status = BTStatus::Fail;
    if (error->status == BLE_HS_EDONE) {
      auto client = impl->clientOwner.lock();
      BLELockGuard lock(client ? client->mtx : nullptr);
      if (remoteTargetConnectedLocked({context->owner, {}, {}}, client, connHandle)) {
        for (auto &c : context->characteristics) {
          c->connHandle = impl->connHandle;
          c->service = impl;
          c->serviceOwner = context->owner;
          c->cacheMtx = impl->cacheMtx;
        }
        impl->characteristics = std::move(context->characteristics);
        impl->characteristicsDiscovered = true;
        status = BTStatus::OK;
      } else {
        status = BTStatus::InvalidState;
      }
    }
    impl->chrDiscoverSync.give(status);
  }
  delete context;
  return 0;
}

#else /* !BLE_GATT_CLIENT_SUPPORTED -- stubs */

// Stubs for BLE_GATT_CLIENT_SUPPORTED == 0: log, return empty characteristic or lists.

BLERemoteService::Impl::~Impl() {
  if (opMtx) {
    vSemaphoreDelete(opMtx);
    opMtx = nullptr;
  }
}

BLERemoteCharacteristic BLERemoteService::getCharacteristic(const BLEUUID &) const {
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
  // Weak: the registry must never keep a characteristic alive or dereference
  // a destroyed one. Entries are pruned when the weak_ptr expires.
  std::weak_ptr<BLERemoteCharacteristic::Impl> chr;
};

static SemaphoreHandle_t sNotifyMtx = xSemaphoreCreateRecursiveMutex();
static std::vector<NotifyEntry> sNotifyRegistry;

BLERemoteCharacteristic::Impl::~Impl() {
  // Unconditional: unregisterForNotify() is identity-checked, so this cannot
  // erase a newer registration for the same (conn, handle).
  unregisterForNotify(connHandle, handle, this);
  if (opMtx) {
    vSemaphoreDelete(opMtx);
    opMtx = nullptr;
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
  SemaphoreHandle_t valueMtx;
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
    if (context->readGeneration->load() != context->generation) {
      delete context;
      return BLE_HS_EDONE;
    }
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
      BLELockGuard lock(context->valueMtx);
      *context->targetValue = std::move(context->value);
    }
    context->readSync->give(success ? BTStatus::OK : BTStatus::Fail);
  }
  delete context;
  return 0;
}

/**
 * @brief Starts pairing/encryption and blocks until security completes or times out.
 * @param svc Owning remote service implementation.
 * @param connHandle GAP connection handle.
 * @return True on successful authentication wait; false if security cannot be started or times out.
 */
static bool initiateSecurityAndWait(const RemoteGattTarget &target, uint16_t connHandle) {
  auto client = target.service ? target.service->clientOwner.lock() : nullptr;
  if (!client) {
    return false;
  }
  int rc;
  {
    BLELockGuard lock(client->mtx);
    if (!remoteTargetConnectedLocked(target, client, connHandle)) {
      return false;
    }
    client->authSync.take();
    rc = ble_gap_security_initiate(connHandle);
  }
  if (rc != 0) {
    client->authSync.give(BTStatus::Fail);
    return false;
  }
  return client->authSync.wait(10000) == BTStatus::OK;
}

static bool performRemoteLongRead(
  const std::shared_ptr<void> &owner, const RemoteGattTarget &target, uint16_t connHandle, uint16_t attrHandle, std::atomic<uint32_t> &readGeneration,
  BLESync &readSync, std::vector<uint8_t> &targetValue, int *lastAttError, ble_gatt_attr_fn *cb, uint32_t timeoutMs, SemaphoreHandle_t opMtx
) {
  // Serialize per-Impl ops for the whole blocking procedure (including the
  // auth retry): a concurrent op on the same Impl would otherwise steal the
  // generation/sync handshake and waitRemoteOp() could wait indefinitely for
  // a callback belonging to the other op. RAII releases on every path.
  BLELockGuard opLock(opMtx);
  auto client = target.service ? target.service->clientOwner.lock() : nullptr;
  if (!client) {
    return false;
  }
  SemaphoreHandle_t valueMtx = client ? client->mtx : nullptr;
  for (int retry = 0; retry < 2; retry++) {
    int rc;
    uint32_t generation;
    {
      BLELockGuard lock(valueMtx);
      if (!remoteTargetConnectedLocked(target, client, connHandle)) {
        return false;
      }
      targetValue.clear();
      if (lastAttError) {
        *lastAttError = 0;
      }
      generation = readGeneration.fetch_add(1) + 1;
      readSync.take();
      auto *context = new (std::nothrow) RemoteReadContext{owner, &readGeneration, &readSync, &targetValue, valueMtx, lastAttError, generation, {}};
      if (!context) {
        readSync.give(BTStatus::NoMemory);
        return false;
      }
      rc = ble_gattc_read_long(connHandle, attrHandle, 0, cb, context);
      if (rc != 0) {
        delete context;
        readSync.give(BTStatus::Fail);
      }
    }
    if (rc != 0) {
      return false;
    }

    BTStatus status = waitRemoteOp(readGeneration, readSync, generation, timeoutMs);
    if (status == BTStatus::OK) {
      return true;
    }
    if (retry == 0 && lastAttError && isAuthError(*lastAttError)) {
      if (!initiateSecurityAndWait(target, connHandle)) {
        return false;
      }
      continue;
    }
    break;
  }
  return false;
}

struct RemoteWriteContext {
  std::shared_ptr<void> owner;
  std::atomic<uint32_t> *writeGeneration;
  BLESync *writeSync;
  int *lastAttError;
  uint32_t generation;
};

static int handleRemoteWriteCb(uint16_t, const struct ble_gatt_error *error, struct ble_gatt_attr *, void *arg) {
  auto *context = static_cast<RemoteWriteContext *>(arg);
  if (!context) {
    return 0;
  }

  uint32_t expected = context->generation;
  if (context->writeGeneration->compare_exchange_strong(expected, expected + 1)) {
    if (context->lastAttError) {
      *context->lastAttError = error->status;
    }
    context->writeSync->give((error->status == 0 || error->status == BLE_HS_EDONE) ? BTStatus::OK : BTStatus::Fail);
  }
  delete context;
  return 0;
}

struct RemoteDscDiscContext {
  std::shared_ptr<BLERemoteCharacteristic::Impl> owner;
  uint32_t generation;
  std::vector<std::shared_ptr<BLERemoteDescriptor::Impl>> descriptors;
};

// ==========================================================================
// BLERemoteCharacteristic implementation
// ==========================================================================

// getHandle / can*() / readRawData are stack-agnostic and live in the common src/BLERemoteCharacteristic.cpp.

// Blocking: ble_gattc_read_long (GATT Read Long Characteristic Values) with synchronous wait; may be retried after a security upgrade.
bool BLERemoteCharacteristic::readInto(uint32_t timeoutMs) {
  auto service = _impl ? _impl->serviceOwner.lock() : nullptr;
  if (!_impl || !service || !remoteConnected(_impl, _impl->connHandle)) {
    return false;
  }

  if (!performRemoteLongRead(
        _impl, {service, _impl, {}}, _impl->connHandle, _impl->handle, _impl->readGeneration, _impl->readSync, _impl->value, &_impl->lastAttError,
        BLERemoteCharacteristic::Impl::readCb, timeoutMs, _impl->opMtx
      )) {
    log_w("RemoteCharacteristic %s: read failed (rc=%d)", _impl->uuid.toString().c_str(), _impl->lastAttError);
    return false;
  }
  return true;
}

// Blocking: ble_gattc_write_flat, ble_gattc_write_long (prepared write/execute), or ble_gattc_write_no_rsp_flat; confirmed paths wait on the write sync.
static BTStatus performRemoteWrite(
  const std::shared_ptr<void> &owner, const RemoteGattTarget &target, uint16_t connHandle, uint16_t attrHandle, const uint8_t *data, size_t len,
  bool withResponse, std::atomic<uint32_t> &writeGeneration, BLESync &writeSync, int *lastAttError, ble_gatt_attr_fn *cb, SemaphoreHandle_t opMtx
) {
  BLELockGuard opLock(opMtx);
  auto client = target.service ? target.service->clientOwner.lock() : nullptr;
  if (!client) {
    return BTStatus::InvalidState;
  }
  uint16_t mtu;
  {
    BLELockGuard lock(client->mtx);
    if (!remoteTargetConnectedLocked(target, client, connHandle)) {
      return BTStatus::InvalidState;
    }
    mtu = ble_att_mtu(connHandle);
  }
  uint16_t maxSingle = (mtu > 3) ? (mtu - 3) : 0;

  if (!withResponse && len > maxSingle) {
    // A no-response write is a single ATT Write Command (Core Vol 3 Part F
    // 3.4.5.1) and cannot exceed MTU-3. Fail explicitly instead of silently
    // upgrading to a (responded, queued) long write the caller did not ask
    // for -- matching the Bluedroid backend.
    log_e("Remote write no-rsp len=%u exceeds MTU-3 (%u)", (unsigned)len, (unsigned)maxSingle);
    return BTStatus::InvalidParam;
  }

  if (!withResponse && len <= maxSingle) {
    int rc;
    {
      BLELockGuard lock(client->mtx);
      if (!remoteTargetConnectedLocked(target, client, connHandle)) {
        return BTStatus::InvalidState;
      }
      rc = ble_gattc_write_no_rsp_flat(connHandle, attrHandle, data, len);
    }
    if (rc != 0) {
      log_e("Remote write no-rsp failed rc=%d", rc);
    }
    return (rc == 0) ? BTStatus::OK : BTStatus::Fail;
  }

  bool isLong = (len > maxSingle);

  for (int retry = 0; retry < (lastAttError ? 2 : 1); retry++) {
    int rc;
    uint32_t generation;
    {
      BLELockGuard lock(client->mtx);
      if (!remoteTargetConnectedLocked(target, client, connHandle)) {
        return BTStatus::InvalidState;
      }
      if (lastAttError) {
        *lastAttError = 0;
      }
      generation = writeGeneration.fetch_add(1) + 1;
      writeSync.take();
      auto *context = new (std::nothrow) RemoteWriteContext{owner, &writeGeneration, &writeSync, lastAttError, generation};
      if (!context) {
        writeSync.give(BTStatus::NoMemory);
        return BTStatus::NoMemory;
      }

      if (isLong) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
        if (!om) {
          delete context;
          writeSync.give(BTStatus::NoMemory);
          return BTStatus::NoMemory;
        }
        rc = ble_gattc_write_long(connHandle, attrHandle, 0, om, cb, context);
      } else {
        rc = ble_gattc_write_flat(connHandle, attrHandle, data, len, cb, context);
      }
      if (rc != 0) {
        delete context;
        writeSync.give(BTStatus::Fail);
      }
    }
    if (rc != 0) {
      log_e("Remote write rc=%d", rc);
      return BTStatus::Fail;
    }

    BTStatus status = waitRemoteOp(writeGeneration, writeSync, generation, 10000);
    if (status == BTStatus::OK) {
      return BTStatus::OK;
    }

    if (retry == 0 && lastAttError && isAuthError(*lastAttError)) {
      log_d("Remote write: auth error, initiating security");
      if (!initiateSecurityAndWait(target, connHandle)) {
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
  auto service = _impl ? _impl->serviceOwner.lock() : nullptr;
  if (!_impl || !service || !remoteConnected(_impl, _impl->connHandle)) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: write len=%u withResponse=%d (conn=%u)", _impl->uuid.toString().c_str(), len, withResponse, _impl->connHandle);
  return performRemoteWrite(
    _impl, {service, _impl, {}}, _impl->connHandle, _impl->handle, data, len, withResponse, _impl->writeGeneration, _impl->writeSync, &_impl->lastAttError,
    BLERemoteCharacteristic::Impl::writeCb, _impl->opMtx
  );
}

static BTStatus subscribeWithCccd(
  BLERemoteCharacteristic &chr, const std::shared_ptr<BLERemoteCharacteristic::Impl> &impl, uint16_t cccdVal, BLERemoteCharacteristic::NotifyCallback callback
) {
  if (!impl || !remoteConnected(impl, impl->connHandle)) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: subscribe cccd=0x%04x (conn=%u)", impl->uuid.toString().c_str(), cccdVal, impl->connHandle);
  BLERemoteDescriptor cccd = chr.getDescriptor(BLEUUID(BLE_DSC_UUID16_CCCD));
  if (!cccd) {
    log_w("RemoteCharacteristic %s: CCCD descriptor (0x2902) not found", impl->uuid.toString().c_str());
    return BTStatus::NotFound;
  }

  {
    BLELockGuard lock(impl->cacheMtx ? impl->cacheMtx->get() : nullptr);
    impl->onNotifyCb = std::move(callback);
  }

  BLERemoteCharacteristic::Impl::registerForNotify(impl->connHandle, impl->handle, impl);

#if BLE_VALIDATION_HOOKS
  if (BLEHooks.remote.injectCccdWriteFail.exchange(false, std::memory_order_acq_rel)) {
    log_w("RemoteCharacteristic %s: injected CCCD write fail", impl->uuid.toString().c_str());
    BLERemoteCharacteristic::Impl::unregisterForNotify(impl->connHandle, impl->handle, impl.get());
    BLELockGuard lock(impl->cacheMtx ? impl->cacheMtx->get() : nullptr);
    impl->onNotifyCb = nullptr;
    return BTStatus::Fail;
  }
#endif
  BTStatus status = cccd.writeValue(reinterpret_cast<const uint8_t *>(&cccdVal), sizeof(cccdVal), true);
  if (status != BTStatus::OK) {
    log_w("RemoteCharacteristic %s: subscribe failed/timed out", impl->uuid.toString().c_str());
    BLERemoteCharacteristic::Impl::unregisterForNotify(impl->connHandle, impl->handle, impl.get());
    BLELockGuard lock(impl->cacheMtx ? impl->cacheMtx->get() : nullptr);
    impl->onNotifyCb = nullptr;
  } else {
    log_i("RemoteCharacteristic %s: subscribed (cccd=0x%04x)", impl->uuid.toString().c_str(), cccdVal);
  }
  return status;
}

// Blocking: discovers the CCCD descriptor and writes it with a synchronous wait; registers the notify path before the write.
BTStatus BLERemoteCharacteristic::subscribe(BLESubscription mode, NotifyCallback callback) {
  if (mode == BLESubscription::None) {
    log_w("RemoteCharacteristic: subscribe called with BLESubscription::None; use unsubscribe()");
    return BTStatus::InvalidParam;
  }
  return subscribeWithCccd(*this, _impl, static_cast<uint16_t>(mode), std::move(callback));
}

// Blocking: writes 0x0000 to the CCCD descriptor with wait.
BTStatus BLERemoteCharacteristic::unsubscribe() {
  if (!_impl || !remoteConnected(_impl, _impl->connHandle)) {
    return BTStatus::InvalidState;
  }

  log_d("RemoteCharacteristic %s: unsubscribe (conn=%u)", _impl->uuid.toString().c_str(), _impl->connHandle);
  BLERemoteCharacteristic::Impl::unregisterForNotify(_impl->connHandle, _impl->handle, _impl.get());
  {
    BLELockGuard lock(_impl->cacheMtx ? _impl->cacheMtx->get() : nullptr);
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
BLERemoteDescriptor BLERemoteCharacteristic::getDescriptor(const BLEUUID &uuid) const {
  BLE_CHECK_IMPL(BLERemoteDescriptor());
  // Serialize descriptor discovery with other ops on this characteristic
  // (recursive: subscribe -> getDescriptor on the same task must re-enter).
  BLELockGuard opLock(impl.opMtx);

  auto svc = impl.serviceOwner.lock();
  auto client = svc ? svc->clientOwner.lock() : nullptr;

  if (!impl.descriptorsDiscovered) {
    if (!remoteConnected(_impl, impl.connHandle)) {
      return BLERemoteDescriptor();
    }

    uint16_t endHandle = svc ? svc->endHandle : 0xFFFF;
    if (svc) {
      BLELockGuard lock(client ? client->mtx : nullptr);
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

    uint32_t generation = impl.dscDiscoverGeneration.fetch_add(1) + 1;
    impl.dscDiscoverSync.take();
    auto *context = new (std::nothrow) RemoteDscDiscContext{_impl, generation, {}};
    if (!context) {
      impl.dscDiscoverSync.give(BTStatus::NoMemory);
      return BLERemoteDescriptor();
    }
    int rc;
    {
      BLELockGuard lock(client ? client->mtx : nullptr);
      if (!remoteTargetConnectedLocked({svc, _impl, {}}, client, impl.connHandle)) {
        delete context;
        impl.dscDiscoverSync.give(BTStatus::InvalidState);
        return BLERemoteDescriptor();
      }
      rc = ble_gattc_disc_all_dscs(impl.connHandle, impl.handle, endHandle, BLERemoteCharacteristic::Impl::dscDiscoveryCb, context);
    }
    if (rc != 0) {
      delete context;
      impl.dscDiscoverSync.give(BTStatus::Fail);
      return BLERemoteDescriptor();
    }
    BTStatus status = waitRemoteOp(impl.dscDiscoverGeneration, impl.dscDiscoverSync, generation, 10000);
    if (status != BTStatus::OK) {
      return BLERemoteDescriptor();
    }
  }

  BLELockGuard lock(client ? client->mtx : nullptr);
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
    (void)getDescriptor(BLEUUID(static_cast<uint16_t>(0)));
  }

  auto svc = impl.serviceOwner.lock();
  auto client = svc ? svc->clientOwner.lock() : nullptr;
  BLELockGuard lock(client ? client->mtx : nullptr);
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
 * @param error GATT error; @c 0 or @c BLE_HS_EDONE on completion.
 * @param attr Optional attribute; may be null.
 * @param arg Owning per-operation write context; released on callback.
 * @return Zero; NimBLE callback convention.
 */
int BLERemoteCharacteristic::Impl::writeCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  return handleRemoteWriteCb(connHandle, error, attr, arg);
}

/**
 * @brief GATT callback for @c ble_gattc_disc_all_dscs: collects descriptors and signals on completion.
 * @param connHandle Connection handle.
 * @param error GATT error; @c BLE_HS_EDONE at end of discovery.
 * @param chrHandle Defining characteristic handle.
 * @param dsc Descriptor info, or nullptr on completion.
 * @param arg Owning per-operation discovery context; released on the terminal callback.
 * @return Zero; NimBLE callback convention.
 */
int BLERemoteCharacteristic::Impl::dscDiscoveryCb(
  uint16_t connHandle, const struct ble_gatt_error *error, uint16_t chrHandle, const struct ble_gatt_dsc *dsc, void *arg
) {
  auto *context = static_cast<RemoteDscDiscContext *>(arg);
  if (!context) {
    return 0;
  }

  if (error->status == 0 && dsc != nullptr) {
    if (context->owner->dscDiscoverGeneration.load() != context->generation) {
      delete context;
      return BLE_HS_EDONE;
    }
    auto dImpl = std::make_shared<BLERemoteDescriptor::Impl>();
    dImpl->uuid = nimbleUuidToPublic(dsc->uuid);
    dImpl->handle = dsc->handle;
    context->descriptors.push_back(dImpl);
    return 0;
  }

  auto *impl = context->owner.get();
  uint32_t expected = context->generation;
  if (impl->dscDiscoverGeneration.compare_exchange_strong(expected, expected + 1)) {
    BTStatus status = BTStatus::Fail;
    if (error->status == BLE_HS_EDONE) {
      auto service = impl->serviceOwner.lock();
      auto client = service ? service->clientOwner.lock() : nullptr;
      BLELockGuard lock(client ? client->mtx : nullptr);
      if (remoteTargetConnectedLocked({service, context->owner, {}}, client, connHandle)) {
        for (auto &d : context->descriptors) {
          d->connHandle = impl->connHandle;
          d->chr = impl;
          d->chrOwner = context->owner;
          d->cacheMtx = impl->cacheMtx;
        }
        impl->descriptors = std::move(context->descriptors);
        impl->descriptorsDiscovered = true;
        status = BTStatus::OK;
      } else {
        status = BTStatus::InvalidState;
      }
    }
    impl->dscDiscoverSync.give(status);
  }
  delete context;
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
 * @param impl Only erase when the registered characteristic is this one (or
 *             has already expired), so destroying/replacing one Impl cannot
 *             erase a newer registration for the same route.
 */
void BLERemoteCharacteristic::Impl::unregisterForNotify(uint16_t connHandle, uint16_t attrHandle, const BLERemoteCharacteristic::Impl *impl) {
  BLELockGuard lock(sNotifyMtx);
  for (auto it = sNotifyRegistry.begin(); it != sNotifyRegistry.end(); ++it) {
    if (it->connHandle == connHandle && it->attrHandle == attrHandle) {
      auto registered = it->chr.lock();
      if (!registered || registered.get() == impl) {
        sNotifyRegistry.erase(it);
      }
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
    for (auto it = sNotifyRegistry.begin(); it != sNotifyRegistry.end();) {
      auto registered = it->chr.lock();
      if (!registered) {
        // Prune routes whose characteristic has been destroyed.
        it = sNotifyRegistry.erase(it);
        continue;
      }
      if (it->connHandle == connHandle && it->attrHandle == attrHandle) {
        implPtr = registered;
        break;
      }
      ++it;
    }
  }
  if (!implPtr || !remoteConnected(implPtr, connHandle)) {
    return;
  }

  uint16_t len = OS_MBUF_PKTLEN(om);
  const uint8_t *dataPtr = nullptr;
  std::vector<uint8_t> flatBuf;
  if (len > 0) {
    if (SLIST_NEXT(om, om_next) == nullptr) {
      dataPtr = om->om_data;
    } else {
      flatBuf.resize(len);
      os_mbuf_copydata(om, 0, len, flatBuf.data());
      dataPtr = flatBuf.data();
    }
  }
  auto svc = implPtr->serviceOwner.lock();
  auto client = svc ? svc->clientOwner.lock() : nullptr;
  const bool serviceChanged = !isNotify && len == 4 && svc && svc->uuid == BLEUUID::from16(0x1801) && implPtr->uuid == BLEUUID::from16(0x2A05);
  {
    BLELockGuard lock(client ? client->mtx : nullptr);
    if (!svc || !client || implPtr->service != svc.get() || svc->client != client.get()) {
      return;
    }
    cb = implPtr->onNotifyCb;
    implPtr->value.assign(dataPtr, dataPtr + len);
    if (serviceChanged) {
      // Service Changed invalidates the indicated database range. NimBLE gives
      // us the real payload, but remote handles do not retain per-attribute
      // range metadata, so conservatively detach the full discovered tree just
      // like Bluedroid. The callback below retains this characteristic handle.
      client->clearDiscoveredServices();
    }
  }
  if (serviceChanged) {
    clearNotifyRoutes(connHandle);
  }

  if (cb) {
    BLERemoteCharacteristic chr = BLERemoteCharacteristicImplCommon::makeHandle(implPtr);
    cb(chr, dataPtr, len, isNotify ? BLESubscription::Notify : BLESubscription::Indicate);
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

BTStatus BLERemoteCharacteristic::subscribe(BLESubscription, NotifyCallback) {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BTStatus BLERemoteCharacteristic::unsubscribe() {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

BLERemoteDescriptor BLERemoteCharacteristic::getDescriptor(const BLEUUID &) const {
  log_w("GATT client not supported");
  return BLERemoteDescriptor();
}

std::vector<BLERemoteDescriptor> BLERemoteCharacteristic::getDescriptors() const {
  log_w("GATT client not supported");
  return {};
}

BLERemoteCharacteristic::Impl::~Impl() {
  unregisterForNotify(connHandle, handle, this);
  if (opMtx) {
    vSemaphoreDelete(opMtx);
    opMtx = nullptr;
  }
}
void BLERemoteCharacteristic::Impl::registerForNotify(uint16_t, uint16_t, const std::shared_ptr<BLERemoteCharacteristic::Impl> &) {}
void BLERemoteCharacteristic::Impl::unregisterForNotify(uint16_t, uint16_t, const BLERemoteCharacteristic::Impl *) {}
void BLERemoteCharacteristic::Impl::clearNotifyRoutes(uint16_t) {}
void BLERemoteCharacteristic::Impl::handleNotifyRx(uint16_t, uint16_t, struct os_mbuf *, bool) {}

#endif /* BLE_GATT_CLIENT_SUPPORTED */

// ==========================================================================
// BLERemoteDescriptor - remote descriptor read/write
// ==========================================================================

#if BLE_GATT_CLIENT_SUPPORTED

// ==========================================================================
// BLERemoteDescriptor implementation
// ==========================================================================
BLERemoteDescriptor::Impl::~Impl() {
  if (opMtx) {
    vSemaphoreDelete(opMtx);
    opMtx = nullptr;
  }
}

// Blocking: ble_gattc_read_long (GATT Read Long Characteristic Descriptors) plus synchronous wait;
// safe only when the connection is in a valid GATT state.
bool BLERemoteDescriptor::readInto(uint32_t timeoutMs) {
  auto chr = _impl ? _impl->chrOwner.lock() : nullptr;
  auto svc = chr ? chr->serviceOwner.lock() : nullptr;
  if (!_impl || !chr || !svc || !remoteConnected(_impl, _impl->connHandle)) {
    return false;
  }

  if (!performRemoteLongRead(
        _impl, {svc, chr, _impl}, _impl->connHandle, _impl->handle, _impl->readGeneration, _impl->readSync, _impl->value, &_impl->lastAttError,
        BLERemoteDescriptor::Impl::readCb, timeoutMs, _impl->opMtx
      )) {
    log_w("RemoteDescriptor: read failed/timed out (handle=0x%04x)", _impl->handle);
    return false;
  }
  return true;
}

// Blocking: confirmed path uses ble_gattc_write_flat or ble_gattc_write_long and waits; CCCD-style descriptor writes follow the same contract as characteristic writes.
BTStatus BLERemoteDescriptor::writeValue(const uint8_t *data, size_t len, bool withResponse) {
  auto chr = _impl ? _impl->chrOwner.lock() : nullptr;
  auto svc = chr ? chr->serviceOwner.lock() : nullptr;
  if (!_impl || !chr || !svc || !remoteConnected(_impl, _impl->connHandle)) {
    log_w("RemoteDescriptor: writeValue called but not connected");
    return BTStatus::InvalidState;
  }
  return performRemoteWrite(
    _impl, {svc, chr, _impl}, _impl->connHandle, _impl->handle, data, len, withResponse, _impl->writeGeneration, _impl->writeSync, &_impl->lastAttError,
    BLERemoteDescriptor::Impl::writeCb, _impl->opMtx
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
 * @param arg Owning per-operation write context; released on callback.
 * @return Zero (NimBLE GATT client callback convention).
 */
int BLERemoteDescriptor::Impl::writeCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  return handleRemoteWriteCb(connHandle, error, attr, arg);
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

BLERemoteDescriptor::Impl::~Impl() {
  if (opMtx) {
    vSemaphoreDelete(opMtx);
    opMtx = nullptr;
  }
}

#endif /* BLE_GATT_CLIENT_SUPPORTED */

#endif /* BLE_NIMBLE */
