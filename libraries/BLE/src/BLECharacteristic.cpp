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

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include "impl/BLEBackend.h"
#include "impl/common/BLEImplHelpers.h"
#include "impl/common/BLEMutex.h"
#include "BLEService.h"
#include "esp32-hal-log.h"

/**
 * @file
 * @brief Stack-agnostic @ref BLECharacteristic helpers; ATT server events,
 *        notify/indicate, and registration live in the selected backend.
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
  return _impl ? _impl->properties : BLEProperty{};
}

BLEPermission BLECharacteristic::getPermissions() const {
  return _impl ? _impl->permissions : BLEPermission{};
}

void BLECharacteristic::setPermissions(BLEPermission permissions) {
  // Changes take effect at registration time; mutating permissions after the
  // service is started requires re-registration.
  BLE_CHECK_IMPL();
  impl.permissions = permissions;
}

BLEDescriptor BLECharacteristic::getDescriptor(const BLEUUID &uuid) {
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
  if (!_impl || !desc._impl) {
    return;
  }
  auto &descs = _impl->descriptors;
  for (auto it = descs.begin(); it != descs.end(); ++it) {
    if (*it == desc._impl) {
      descs.erase(it);
      break;
    }
  }
}

BLEUUID BLECharacteristic::getUUID() const {
  return _impl ? _impl->uuid : BLEUUID();
}

uint16_t BLECharacteristic::getHandle() const {
  return _impl ? _impl->handle : 0;
}

BLEService BLECharacteristic::getService() const {
  // The returned handle uses a non-owning shared_ptr (service lifetime is owned
  // by the server), so it must not outlive the server.
  return _impl && _impl->service ? BLEService(_impl->service->shared_from_this()) : BLEService();
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
  impl.onReadCb = handler;
}

void BLECharacteristic::onWrite(WriteHandler handler) {
  BLE_CHECK_IMPL();
  impl.onWriteCb = handler;
}

void BLECharacteristic::onNotify(NotifyHandler handler) {
  BLE_CHECK_IMPL();
  impl.onNotifyCb = handler;
}

void BLECharacteristic::onSubscribe(SubscribeHandler handler) {
  BLE_CHECK_IMPL();
  impl.onSubscribeCb = handler;
}

void BLECharacteristic::onStatus(StatusHandler handler) {
  BLE_CHECK_IMPL();
  impl.onStatusCb = handler;
}

void BLECharacteristic::resetCallbacks() {
  BLE_CHECK_IMPL();
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
