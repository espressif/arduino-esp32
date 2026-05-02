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

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include "impl/BLECharacteristicBackend.h"
#include "impl/BLEImplHelpers.h"
#include "impl/BLEMutex.h"
#include "impl/BLEServiceBackend.h"
#include "BLEService.h"
#include "esp32-hal-log.h"

/**
 * @file
 * @brief Stack-agnostic @ref BLECharacteristic helpers; ATT server events,
 *        notify/indicate, and registration live in the selected backend.
 */

// --------------------------------------------------------------------------
// BLECharacteristic common API (stack-agnostic)
// --------------------------------------------------------------------------

/**
 * @brief Construct a default (invalid) characteristic handle.
 * @note The handle evaluates to false until backed by a live Impl.
 */
BLECharacteristic::BLECharacteristic() : _impl(nullptr) {}

/**
 * @brief Check whether this handle refers to a valid characteristic.
 * @return true if the handle is backed by a live Impl, false otherwise.
 */
BLECharacteristic::operator bool() const {
  return _impl != nullptr;
}

/**
 * @brief Set the characteristic value from an Arduino String.
 * @param value String whose raw bytes are copied into the value buffer.
 * @note Delegates to setValue(const uint8_t*, size_t). No null terminator is stored.
 */
void BLECharacteristic::setValue(const String &value) {
  setValue(reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
}

/**
 * @brief Set the characteristic value from a 16-bit unsigned integer.
 * @param v Value stored in host byte order (no endian conversion).
 */
void BLECharacteristic::setValue(uint16_t v) {
  setValue(reinterpret_cast<const uint8_t *>(&v), sizeof(v));
}

/**
 * @brief Set the characteristic value from a 32-bit unsigned integer.
 * @param v Value stored in host byte order (no endian conversion).
 */
void BLECharacteristic::setValue(uint32_t v) {
  setValue(reinterpret_cast<const uint8_t *>(&v), sizeof(v));
}

/**
 * @brief Set the characteristic value from a 32-bit float.
 * @param v Value stored in host byte order (no endian conversion).
 */
void BLECharacteristic::setValue(float v) {
  setValue(reinterpret_cast<const uint8_t *>(&v), sizeof(v));
}

/**
 * @brief Set the characteristic value from a 64-bit double.
 * @param v Value stored in host byte order (no endian conversion).
 */
void BLECharacteristic::setValue(double v) {
  setValue(reinterpret_cast<const uint8_t *>(&v), sizeof(v));
}

/**
 * @brief Get the GATT properties of this characteristic.
 * @return Bitmask of BLEProperty flags, or a default-constructed (empty) BLEProperty if the handle is invalid.
 */
BLEProperty BLECharacteristic::getProperties() const {
  return _impl ? _impl->properties : BLEProperty{};
}

/**
 * @brief Get the ATT permissions for this characteristic value.
 * @return Bitmask of BLEPermission flags, or a default-constructed (empty) BLEPermission if the handle is invalid.
 */
BLEPermission BLECharacteristic::getPermissions() const {
  return _impl ? _impl->permissions : BLEPermission{};
}

/**
 * @brief Set the ATT permissions for this characteristic value.
 * @param permissions Bitmask of BLEPermission flags.
 * @note No-op if the handle is invalid (BLE_CHECK_IMPL guard).
 *       Changes take effect at registration time; mutating permissions after
 *       the service is started requires re-registration.
 */
void BLECharacteristic::setPermissions(BLEPermission permissions) {
  BLE_CHECK_IMPL();
  impl.permissions = permissions;
}

/**
 * @brief Find a descriptor by UUID on this characteristic.
 * @param uuid UUID to search for among attached descriptors.
 * @return Handle to the first matching descriptor, or an invalid BLEDescriptor if not found.
 */
BLEDescriptor BLECharacteristic::getDescriptor(const BLEUUID &uuid) {
  BLE_CHECK_IMPL(BLEDescriptor());
  for (auto &d : impl.descriptors) {
    if (d->uuid == uuid) {
      return BLEDescriptor(d);
    }
  }
  return BLEDescriptor();
}

/**
 * @brief Get all descriptors attached to this characteristic.
 * @return Vector of descriptor handles. Empty if the characteristic is invalid.
 */
std::vector<BLEDescriptor> BLECharacteristic::getDescriptors() const {
  std::vector<BLEDescriptor> result;
  BLE_CHECK_IMPL(result);
  result.reserve(impl.descriptors.size());
  for (auto &d : impl.descriptors) {
    result.push_back(BLEDescriptor(d));
  }
  return result;
}

/**
 * @brief Remove a descriptor from this characteristic.
 * @param desc Handle to the descriptor to remove. Silently ignored if the
 *             descriptor is not attached or either handle is invalid.
 * @note Removes only the first matching Impl pointer. The descriptor's
 *       shared_ptr may still be alive if held elsewhere.
 */
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

/**
 * @brief Get the UUID of this characteristic.
 * @return The characteristic UUID, or a default-constructed BLEUUID if the handle is invalid.
 */
BLEUUID BLECharacteristic::getUUID() const {
  return _impl ? _impl->uuid : BLEUUID();
}

/**
 * @brief Get the ATT attribute handle of this characteristic.
 * @return The 16-bit attribute handle, or 0 if the handle is invalid or not yet registered.
 */
uint16_t BLECharacteristic::getHandle() const {
  return _impl ? _impl->handle : 0;
}

/**
 * @brief Get the parent service that owns this characteristic.
 * @return Handle to the parent BLEService. The returned handle uses a non-owning
 *         shared_ptr (no-op deleter) since the service lifetime is managed by the server.
 *         Returns an invalid BLEService if this characteristic is not attached.
 */
BLEService BLECharacteristic::getService() const {
  return _impl && _impl->service ? BLEService(_impl->service->shared_from_this()) : BLEService();
}

/**
 * @brief Get a human-readable string representation of this characteristic.
 * @return String in the form "BLECharacteristic(uuid=...)", or
 *         "BLECharacteristic(null)" if the handle is invalid.
 */
String BLECharacteristic::toString() const {
  BLE_CHECK_IMPL("BLECharacteristic(null)");
  return "BLECharacteristic(uuid=" + impl.uuid.toString() + ")";
}

/**
 * @brief Set the characteristic value from a signed int.
 * @param value Widened to uint32_t before storage (host byte order).
 */
void BLECharacteristic::setValue(int value) {
  setValue(static_cast<uint32_t>(value));
}

/**
 * @brief Register a callback invoked when a client reads this characteristic.
 * @param handler The read callback, or nullptr to clear. Invoked in the BLE
 *                stack context before the read response is sent.
 */
void BLECharacteristic::onRead(ReadHandler handler) {
  BLE_CHECK_IMPL();
  impl.onReadCb = handler;
}

/**
 * @brief Register a callback invoked when a client writes to this characteristic.
 * @param handler The write callback, or nullptr to clear. Invoked in the BLE
 *                stack context after the value has been updated.
 */
void BLECharacteristic::onWrite(WriteHandler handler) {
  BLE_CHECK_IMPL();
  impl.onWriteCb = handler;
}

/**
 * @brief Register a callback invoked after a notification is queued.
 * @param handler The notify callback, or nullptr to clear.
 */
void BLECharacteristic::onNotify(NotifyHandler handler) {
  BLE_CHECK_IMPL();
  impl.onNotifyCb = handler;
}

/**
 * @brief Register a callback invoked when a client subscribes or unsubscribes.
 * @param handler The subscribe callback, or nullptr to clear. Triggered by
 *                CCCD writes from the remote client.
 */
void BLECharacteristic::onSubscribe(SubscribeHandler handler) {
  BLE_CHECK_IMPL();
  impl.onSubscribeCb = handler;
}

/**
 * @brief Register a callback invoked with notification/indication delivery status.
 * @param handler The status callback, or nullptr to clear. For indications,
 *                reports confirmation receipt or timeout.
 */
void BLECharacteristic::onStatus(StatusHandler handler) {
  BLE_CHECK_IMPL();
  impl.onStatusCb = handler;
}

/**
 * @brief Remove all previously registered callbacks from this characteristic.
 * @note Sets onRead, onWrite, onNotify, onSubscribe, and onStatus handlers to nullptr.
 */
void BLECharacteristic::resetCallbacks() {
  BLE_CHECK_IMPL();
  impl.onReadCb = nullptr;
  impl.onWriteCb = nullptr;
  impl.onNotifyCb = nullptr;
  impl.onSubscribeCb = nullptr;
  impl.onStatusCb = nullptr;
}

/**
 * @brief Get the number of clients currently subscribed (notify or indicate).
 * @return Subscriber count, or 0 if the handle is invalid.
 */
size_t BLECharacteristic::getSubscribedCount() const {
  return _impl ? _impl->subscribers.size() : 0;
}

/**
 * @brief Check whether a specific client is subscribed.
 * @param connHandle Connection handle to query.
 * @return true if the client has a non-zero subscription value (notifications
 *         and/or indications enabled), false otherwise or if the handle is invalid.
 */
bool BLECharacteristic::isSubscribed(uint16_t connHandle) const {
  BLE_CHECK_IMPL(false);
  for (const auto &sub : impl.subscribers) {
    if (sub.first == connHandle) {
      return sub.second > 0;
    }
  }
  return false;
}

/**
 * @brief Get the connection handles of all subscribed clients.
 * @return Vector of connection handles with non-zero subscription values.
 *         Empty if the handle is invalid or no clients are subscribed.
 */
std::vector<uint16_t> BLECharacteristic::getSubscribedConnections() const {
  std::vector<uint16_t> result;
  BLE_CHECK_IMPL(result);
  for (const auto &sub : impl.subscribers) {
    if (sub.second > 0) {
      result.push_back(sub.first);
    }
  }
  return result;
}

/**
 * @brief Set the characteristic value from a raw byte buffer.
 * @param data Pointer to the data bytes to copy.
 * @param length Number of bytes to copy from @p data.
 * @note Thread-safe: acquires the value mutex before modifying the internal buffer.
 *       No-op if the handle is invalid (BLE_CHECK_IMPL guard).
 * @note High-frequency updates with verbose logging of large payloads can be
 *       costly; keep logging off hot paths or at appropriate log levels.
 */
void BLECharacteristic::setValue(const uint8_t *data, size_t length) {
  BLE_CHECK_IMPL();
  log_d("Characteristic %s: setValue len=%u", impl.uuid.toString().c_str(), length);
  BLELockGuard lock(impl.valueMtx);
  impl.value.assign(data, data + length);
}

/**
 * @brief Get a pointer to the raw characteristic value.
 * @param length If non-null, receives the length of the value in bytes.
 *               Set to 0 when the handle is invalid.
 * @return Pointer to the internal value buffer, or nullptr if the handle is
 *         invalid or the value is empty.
 * @note Thread-safe: acquires the value mutex. The returned pointer is only
 *       valid while the caller holds no references that could trigger a
 *       concurrent setValue().
 */
const uint8_t *BLECharacteristic::getValue(size_t *length) const {
  if (!_impl) {
    if (length) {
      *length = 0;
    }
    return nullptr;
  }
  BLELockGuard lock(_impl->valueMtx);
  if (length) {
    *length = _impl->value.size();
  }
  return _impl->value.empty() ? nullptr : _impl->value.data();
}

/**
 * @brief Get the characteristic value as an Arduino String.
 * @return The value interpreted as a UTF-8 string, or an empty string if
 *         the handle is invalid.
 * @note Thread-safe: acquires the value mutex for the duration of the copy.
 */
String BLECharacteristic::getStringValue() const {
  BLE_CHECK_IMPL("");
  BLELockGuard lock(impl.valueMtx);
  return String(reinterpret_cast<const char *>(impl.value.data()), impl.value.size());
}

#endif /* BLE_ENABLED */
