/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
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

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "core/BLEBackend.h"
#include "core/BLEImplHelpers.h"
#include "core/BLEMutex.h"
#include "gatt/BLEService.h"
#include "esp32-hal-log.h"

/**
 * @file
 * @brief Stack-agnostic @ref BLECharacteristic helpers and shared notify/indicate dispatch.
 */

BLECharacteristic::BLECharacteristic() : _impl(nullptr) {}

BLECharacteristic::operator bool() const {
  return _impl != nullptr;
}

void BLECharacteristic::setValue(const String &value) {
  setValue(reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
}

void BLECharacteristic::setValue(uint16_t v) {
  setValue(reinterpret_cast<const uint8_t *>(&v), sizeof(v));
}

void BLECharacteristic::setValue(uint32_t v) {
  setValue(reinterpret_cast<const uint8_t *>(&v), sizeof(v));
}

void BLECharacteristic::setValue(float v) {
  setValue(reinterpret_cast<const uint8_t *>(&v), sizeof(v));
}

void BLECharacteristic::setValue(double v) {
  setValue(reinterpret_cast<const uint8_t *>(&v), sizeof(v));
}

BLEProperty BLECharacteristic::getProperties() const {
  BLE_CHECK_IMPL(BLEProperty{});
  return impl.properties;
}

BLEPermission BLECharacteristic::getPermissions() const {
  BLE_CHECK_IMPL(BLEPermission{});
  return impl.permissions;
}

void BLECharacteristic::setPermissions(BLEPermission permissions) {
  // Changes take effect at registration time; mutating permissions after the
  // service is started requires re-registration.
  BLE_CHECK_IMPL();
  impl.permissions = permissions;
}

BLEDescriptor BLECharacteristic::getDescriptor(const BLEUUID &uuid) const {
  BLE_CHECK_IMPL(BLEDescriptor());
  for (auto &d : impl.descriptors) {
    if (d->uuid == uuid) {
      return BLEDescriptor(d);
    }
  }
  return BLEDescriptor();
}

std::vector<BLEDescriptor> BLECharacteristic::getDescriptors() const {
  std::vector<BLEDescriptor> result;
  BLE_CHECK_IMPL(result);
  result.reserve(impl.descriptors.size());
  for (auto &d : impl.descriptors) {
    result.push_back(BLEDescriptor(d));
  }
  return result;
}

void BLECharacteristic::removeDescriptor(const BLEDescriptor &desc) {
#if BLE_GATT_SERVER_SUPPORTED
  BLE_CHECK_IMPL();
  if (!desc._impl) {
    return;
  }
  std::shared_ptr<BLEDescriptor::Impl> removed;
  {
    BLELockGuard lock(impl.mtx);
    if (impl.service && impl.service->started) {
      log_w("Characteristic %s: removeDescriptor is only allowed before the service is started", impl.uuid.toString().c_str());
      return;
    }
    auto &descs = impl.descriptors;
    for (auto it = descs.begin(); it != descs.end(); ++it) {
      if (*it == desc._impl) {
        removed = *it;
        descs.erase(it);
        break;
      }
    }
  }
  bleDetachLocalDescriptor(removed);
#else
  (void)desc;
#endif
}

BLEUUID BLECharacteristic::getUUID() const {
  BLE_CHECK_IMPL(BLEUUID());
  return impl.uuid;
}

uint16_t BLECharacteristic::getHandle() const {
  BLE_CHECK_IMPL(0);
  return impl.handle;
}

BLEService BLECharacteristic::getService() const {
  // The returned handle uses a non-owning shared_ptr (service lifetime is owned
  // by the server), so it must not outlive the server.
  BLE_CHECK_IMPL(BLEService());
  return impl.service ? BLEService(impl.service->shared_from_this()) : BLEService();
}

String BLECharacteristic::toString() const {
  BLE_CHECK_IMPL("BLECharacteristic(null)");
  return "BLECharacteristic(uuid=" + impl.uuid.toString() + ")";
}

void BLECharacteristic::setValue(int value) {
  setValue(static_cast<uint32_t>(value));
}

void BLECharacteristic::onRead(ReadHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onReadCb = std::move(handler);
}

void BLECharacteristic::onWrite(WriteHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onWriteCb = std::move(handler);
}

void BLECharacteristic::onNotify(NotifyHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onNotifyCb = std::move(handler);
  impl.dispatchNotifyHook = BLECharacteristicImplCommon::invokeDispatchNotify;
}

void BLECharacteristic::onSubscribe(SubscribeHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onSubscribeCb = std::move(handler);
}

void BLECharacteristic::onStatus(StatusHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onStatusCb = std::move(handler);
  impl.dispatchStatusHook = BLECharacteristicImplCommon::invokeDispatchStatus;
}

void BLECharacteristicImplCommon::invokeDispatchNotify(BLECharacteristicImplCommon *self) {
  BLECharacteristic::NotifyHandler cb;
  {
    BLELockGuard lock(self->mtx);
    if (!self->onNotifyCb) {
      return;
    }
    cb = self->onNotifyCb;
  }
  BLECharacteristic chr = makeHandle(self->shared_from_this());
  cb(chr);
}

void BLECharacteristicImplCommon::invokeDispatchStatus(BLECharacteristicImplCommon *self, BLECharacteristic::NotifyStatus status, uint32_t code) {
  BLECharacteristic::StatusHandler cb;
  {
    BLELockGuard lock(self->mtx);
    if (!self->onStatusCb) {
      return;
    }
    cb = self->onStatusCb;
  }
  BLECharacteristic chr = makeHandle(self->shared_from_this());
  cb(chr, status, code);
}

#if BLE_GATT_SERVER_SUPPORTED

BLEServer::Impl *BLECharacteristicImplCommon::activeServer() {
  BLEService::Impl *svc = nullptr;
  BLEServer::Impl *server = nullptr;
  {
    BLELockGuard lock(mtx);
    if (handle == 0 || !service || !service->server) {
      return nullptr;
    }
    svc = service;
    server = svc->server;
  }
  BLELockGuard lock(server->mtx);
  if (!server->active || !server->started || !svc->started || svc->removing || svc->server != server) {
    return nullptr;
  }
  return server;
}

static BTStatus sendNotifyAdapter(BLECharacteristic::Impl &chr, BLEServer::Impl *, uint16_t connHandle, const uint8_t *data, size_t length) {
  return bleCharacteristicSendNotify(chr, connHandle, data, length);
}

static BTStatus sendIndicateAdapter(BLECharacteristic::Impl &chr, BLEServer::Impl *server, uint16_t connHandle, const uint8_t *data, size_t length) {
  return server->indicate(connHandle, chr.shared_from_this(), std::vector<uint8_t>(data, data + length));
}

BTStatus BLECharacteristicImplCommon::sendToSubscribers(const uint8_t *data, size_t length, bool indicate, SendFn sendFn) {
  BLEServer::Impl *server = activeServer();
  if (!server) {
    log_w("Characteristic %s: %s called while not registered on an active server", uuid.toString().c_str(), indicate ? "indicate" : "notify");
    return BTStatus::InvalidState;
  }
  BLEProperty requiredProp = indicate ? BLEProperty::Indicate : BLEProperty::Notify;
  if (!(properties & requiredProp)) {
    log_w("Characteristic %s: %s called but property not set", uuid.toString().c_str(), indicate ? "indicate" : "notify");
    return BTStatus::InvalidState;
  }

  uint16_t cccdBit = indicate ? BLE_CCCD_INDICATE : BLE_CCCD_NOTIFY;
  uint16_t targets[16];
  size_t count = 0;
  bool anyConnection = false;
  {
    BLELockGuard lock(server->mtx);
    anyConnection = !server->connections.empty();
    for (const auto &[connHandle, subVal] : subscribers) {
      if ((subVal & cccdBit) && count < (sizeof(targets) / sizeof(targets[0]))) {
        targets[count++] = connHandle;
      }
    }
  }
  if (count == 0) {
    dispatchStatus(anyConnection ? BLECharacteristic::ERROR_NO_SUBSCRIBER : BLECharacteristic::ERROR_NO_CLIENT, 0);
    return BTStatus::OK;
  }

  dispatchNotify();
  BLELockGuard valueLock((!data || length == 0) ? mtx : nullptr);
  const uint8_t *payloadPtr = (!data || length == 0) ? value.data() : data;
  size_t payloadLen = (!data || length == 0) ? value.size() : length;

  auto &self = static_cast<BLECharacteristic::Impl &>(*this);
  BTStatus result = BTStatus::OK;
  for (size_t i = 0; i < count; ++i) {
    BTStatus s = sendFn(self, server, targets[i], payloadPtr, payloadLen);
    if (s == BTStatus::Busy) {
      result = BTStatus::Busy;
    } else if (!s && result) {
      result = s;
    }
  }
  return indicate ? result : BTStatus::OK;
}

BTStatus BLECharacteristicImplCommon::sendToConnection(uint16_t connHandle, const uint8_t *data, size_t length, bool indicate, SendFn sendFn) {
  BLEServer::Impl *server = activeServer();
  if (!server) {
    log_w("Characteristic %s: %s(conn) called while not registered on an active server", uuid.toString().c_str(), indicate ? "indicate" : "notify");
    return BTStatus::InvalidState;
  }
  BLEProperty requiredProp = indicate ? BLEProperty::Indicate : BLEProperty::Notify;
  if (!(properties & requiredProp)) {
    log_w("Characteristic %s: %s(conn) called but property not set", uuid.toString().c_str(), indicate ? "indicate" : "notify");
    return BTStatus::InvalidState;
  }

  uint16_t cccdBit = indicate ? BLE_CCCD_INDICATE : BLE_CCCD_NOTIFY;
  bool connected = false;
  uint16_t subVal = 0;
  {
    BLELockGuard lock(server->mtx);
    connected = server->connFind(connHandle) != nullptr;
    for (const auto &sub : subscribers) {
      if (sub.first == connHandle) {
        subVal = sub.second;
        break;
      }
    }
  }
  if (!connected) {
    log_w("Characteristic %s: %s(conn=%u) but not connected", uuid.toString().c_str(), indicate ? "indicate" : "notify", connHandle);
    dispatchStatus(BLECharacteristic::ERROR_NO_CLIENT, 0);
    return BTStatus::NotConnected;
  }
  if (!(subVal & cccdBit)) {
    log_w("Characteristic %s: %s(conn=%u) but the client has not enabled it", uuid.toString().c_str(), indicate ? "indicate" : "notify", connHandle);
    dispatchStatus(indicate ? BLECharacteristic::ERROR_INDICATE_DISABLED : BLECharacteristic::ERROR_NOTIFY_DISABLED, 0);
    return BTStatus::InvalidState;
  }

  dispatchNotify();
  BLELockGuard valueLock((!data || length == 0) ? mtx : nullptr);
  const uint8_t *payloadPtr = (!data || length == 0) ? value.data() : data;
  size_t payloadLen = (!data || length == 0) ? value.size() : length;

  auto &self = static_cast<BLECharacteristic::Impl &>(*this);
  return sendFn(self, server, connHandle, payloadPtr, payloadLen);
}

BTStatus BLECharacteristic::notify(const uint8_t *data, size_t length) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.sendToSubscribers(data, length, /*indicate=*/false, sendNotifyAdapter);
}

BTStatus BLECharacteristic::notify(uint16_t connHandle, const uint8_t *data, size_t length) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.sendToConnection(connHandle, data, length, /*indicate=*/false, sendNotifyAdapter);
}

BTStatus BLECharacteristic::indicate(const uint8_t *data, size_t length) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.sendToSubscribers(data, length, /*indicate=*/true, sendIndicateAdapter);
}

BTStatus BLECharacteristic::indicate(uint16_t connHandle, const uint8_t *data, size_t length) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  return impl.sendToConnection(connHandle, data, length, /*indicate=*/true, sendIndicateAdapter);
}

void BLECharacteristic::setDescription(const String &desc) {
  if (!_impl) {
    return;
  }
  auto existing = getDescriptor(BLEUUID(BLE_DSC_UUID16_USER_DESC));
  if (existing) {
    existing.setValue(desc);
  } else {
    auto d = createDescriptor(BLEUUID(BLE_DSC_UUID16_USER_DESC), BLEPermission::ReadOpen, desc.length() + 1);
    d.setValue(desc);
  }
}

#else /* !BLE_GATT_SERVER_SUPPORTED */

BTStatus BLECharacteristic::notify(const uint8_t *, size_t) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLECharacteristic::notify(uint16_t, const uint8_t *, size_t) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLECharacteristic::indicate(const uint8_t *, size_t) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLECharacteristic::indicate(uint16_t, const uint8_t *, size_t) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BLEDescriptor BLECharacteristic::createDescriptor(const BLEUUID &, BLEPermission, size_t) {
  return BLEDescriptor();
}

void BLECharacteristic::setDescription(const String &) {}

#endif /* BLE_GATT_SERVER_SUPPORTED */

void BLECharacteristic::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onReadCb = nullptr;
  impl.onWriteCb = nullptr;
  impl.onNotifyCb = nullptr;
  impl.onSubscribeCb = nullptr;
  impl.onStatusCb = nullptr;
}

// subscribers is protected by the owning server's mtx (the BLE event task
// mutates it on subscribe/unsubscribe/disconnect). Returns that mutex, or
// nullptr if the characteristic is not yet attached to a server (BLELockGuard
// treats nullptr as a no-op).
static SemaphoreHandle_t subscribersMtx(const BLECharacteristic::Impl &impl) {
  return (impl.service && impl.service->server) ? impl.service->server->mtx : nullptr;
}

size_t BLECharacteristic::getSubscribedCount() const {
  BLE_CHECK_IMPL(0);
  BLELockGuard lock(subscribersMtx(impl));
  return impl.subscribers.size();
}

bool BLECharacteristic::isSubscribed(uint16_t connHandle) const {
  BLE_CHECK_IMPL(false);
  BLELockGuard lock(subscribersMtx(impl));
  for (const auto &sub : impl.subscribers) {
    if (sub.first == connHandle) {
      return sub.second > 0;
    }
  }
  return false;
}

std::vector<uint16_t> BLECharacteristic::getSubscribedConnections() const {
  std::vector<uint16_t> result;
  BLE_CHECK_IMPL(result);
  BLELockGuard lock(subscribersMtx(impl));
  for (const auto &sub : impl.subscribers) {
    if (sub.second > 0) {
      result.push_back(sub.first);
    }
  }
  return result;
}

void BLECharacteristic::setValue(const uint8_t *data, size_t length) {
  BLE_CHECK_IMPL();
  // Keep verbose logging of large payloads off hot paths - high-frequency
  // updates can make this call costly.
  log_d("Characteristic %s: setValue len=%u", impl.uuid.toString().c_str(), length);
  BLELockGuard lock(impl.mtx);
  impl.value.assign(data, data + length);
}

const uint8_t *BLECharacteristic::getValue(size_t *length) const {
  if (!_impl) {
    if (length) {
      *length = 0;
    }
    return nullptr;
  }
  BLELockGuard lock(_impl->mtx);
  if (length) {
    *length = _impl->value.size();
  }
  return _impl->value.empty() ? nullptr : _impl->value.data();
}

String BLECharacteristic::getStringValue() const {
  BLE_CHECK_IMPL("");
  BLELockGuard lock(impl.mtx);
  return String(reinterpret_cast<const char *>(impl.value.data()), impl.value.size());
}

#endif /* BLE_ENABLED */
