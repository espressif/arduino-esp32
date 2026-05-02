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
#include "esp32-hal-log.h"

// --------------------------------------------------------------------------
// BLEDescriptor common API (stack-agnostic)
// --------------------------------------------------------------------------

/**
 * @brief Construct a default (invalid) descriptor handle.
 * @note The handle evaluates to false until backed by a live Impl.
 */
BLEDescriptor::BLEDescriptor() : _impl(nullptr) {}

/**
 * @brief Check whether this handle refers to a valid descriptor.
 * @return true if the handle is backed by a live Impl, false otherwise.
 */
BLEDescriptor::operator bool() const {
  return _impl != nullptr;
}

/**
 * @brief Get the UUID of this descriptor.
 * @return The descriptor UUID, or a default-constructed BLEUUID if the handle is invalid.
 */
BLEUUID BLEDescriptor::getUUID() const {
  return _impl ? _impl->uuid : BLEUUID();
}

/**
 * @brief Get the ATT attribute handle of this descriptor.
 * @return The 16-bit attribute handle, or 0 if the handle is invalid or not yet registered.
 */
uint16_t BLEDescriptor::getHandle() const {
  return _impl ? _impl->handle : 0;
}

/**
 * @brief Get the parent characteristic that owns this descriptor.
 * @return Handle to the parent BLECharacteristic. Uses a non-owning shared_ptr
 *         (no-op deleter) since the characteristic lifetime is managed by the service.
 *         Returns an invalid BLECharacteristic if this descriptor is not attached.
 */
BLECharacteristic BLEDescriptor::getCharacteristic() const {
  return _impl && _impl->chr ? BLECharacteristic(_impl->chr->shared_from_this()) : BLECharacteristic();
}

/**
 * @brief Get a human-readable string representation of this descriptor.
 * @return String in the form "BLEDescriptor(uuid=...)", or
 *         "BLEDescriptor(null)" if the handle is invalid.
 */
String BLEDescriptor::toString() const {
  BLE_CHECK_IMPL("BLEDescriptor(null)");
  return "BLEDescriptor(uuid=" + impl.uuid.toString() + ")";
}

/**
 * @brief Set the descriptor value from an Arduino String.
 * @param value String whose raw bytes are copied into the value buffer.
 * @note No null terminator is stored. Delegates to setValue(const uint8_t*, size_t).
 */
void BLEDescriptor::setValue(const String &value) {
  setValue(reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
}

/**
 * @brief Get the current value length.
 * @return Number of bytes stored in the descriptor value, or 0 if the handle is invalid.
 */
size_t BLEDescriptor::getLength() const {
  return _impl ? _impl->value.size() : 0;
}

/**
 * @brief Check whether this descriptor is a User Description (UUID 0x2901).
 * @return true if the descriptor UUID matches 0x2901.
 */
bool BLEDescriptor::isUserDescription() const {
  return _impl && _impl->uuid == BLEUUID(static_cast<uint16_t>(0x2901));
}

/**
 * @brief Check whether this descriptor is a CCCD (UUID 0x2902).
 * @return true if the descriptor UUID matches 0x2902.
 */
bool BLEDescriptor::isCCCD() const {
  return _impl && _impl->uuid == BLEUUID(static_cast<uint16_t>(0x2902));
}

/**
 * @brief Check whether this descriptor is a Presentation Format (UUID 0x2904).
 * @return true if the descriptor UUID matches 0x2904.
 */
bool BLEDescriptor::isPresentationFormat() const {
  return _impl && _impl->uuid == BLEUUID(static_cast<uint16_t>(0x2904));
}

/**
 * @brief Set the User Description string (0x2901).
 * @param description Human-readable description text.
 * @note No-op if this descriptor is not a 0x2901 User Description.
 */
void BLEDescriptor::setUserDescription(const String &description) {
  if (isUserDescription()) {
    setValue(description);
  }
}

/**
 * @brief Get the User Description string (0x2901).
 * @return The stored description text, or an empty string if this is not
 *         a 0x2901 descriptor or the handle is invalid.
 */
String BLEDescriptor::getUserDescription() const {
  if (!isUserDescription() || !_impl) {
    return "";
  }
  return String(reinterpret_cast<const char *>(_impl->value.data()), _impl->value.size());
}

/**
 * @brief Check whether notifications are enabled in this CCCD.
 * @return true if this is a valid CCCD with at least 2 bytes and bit 0 is set.
 */
bool BLEDescriptor::getNotifications() const {
  return isCCCD() && _impl && _impl->value.size() >= 2 && (_impl->value[0] & 0x01);
}

/**
 * @brief Check whether indications are enabled in this CCCD.
 * @return true if this is a valid CCCD with at least 2 bytes and bit 1 is set.
 */
bool BLEDescriptor::getIndications() const {
  return isCCCD() && _impl && _impl->value.size() >= 2 && (_impl->value[0] & 0x02);
}

/**
 * @brief Enable or disable notifications in this CCCD.
 * @param enable true to set bit 0, false to clear it.
 * @note No-op if this descriptor is not a CCCD or the handle is invalid.
 *       Auto-resizes the value buffer to 2 bytes if shorter.
 */
void BLEDescriptor::setNotifications(bool enable) {
  if (!isCCCD() || !_impl) {
    return;
  }
  if (_impl->value.size() < 2) {
    _impl->value.resize(2, 0);
  }
  if (enable) {
    _impl->value[0] |= 0x01;
  } else {
    _impl->value[0] &= ~0x01;
  }
}

/**
 * @brief Enable or disable indications in this CCCD.
 * @param enable true to set bit 1, false to clear it.
 * @note No-op if this descriptor is not a CCCD or the handle is invalid.
 *       Auto-resizes the value buffer to 2 bytes if shorter.
 */
void BLEDescriptor::setIndications(bool enable) {
  if (!isCCCD() || !_impl) {
    return;
  }
  if (_impl->value.size() < 2) {
    _impl->value.resize(2, 0);
  }
  if (enable) {
    _impl->value[0] |= 0x02;
  } else {
    _impl->value[0] &= ~0x02;
  }
}

/**
 * @brief Set the format field of a Presentation Format descriptor (byte 0).
 * @param format One of the FORMAT_* constants (e.g. FORMAT_UINT8).
 * @note No-op unless this is a 0x2904 descriptor with a value of at least 7 bytes.
 */
void BLEDescriptor::setFormat(uint8_t format) {
  if (isPresentationFormat() && _impl && _impl->value.size() >= 7) {
    _impl->value[0] = format;
  }
}

/**
 * @brief Set the exponent field of a Presentation Format descriptor (byte 1).
 * @param exponent Signed exponent applied to the characteristic value.
 * @note No-op unless this is a 0x2904 descriptor with a value of at least 7 bytes.
 */
void BLEDescriptor::setExponent(int8_t exponent) {
  if (isPresentationFormat() && _impl && _impl->value.size() >= 7) {
    _impl->value[1] = static_cast<uint8_t>(exponent);
  }
}

/**
 * @brief Set the unit field of a Presentation Format descriptor (bytes 2-3, little-endian).
 * @param unit Bluetooth Assigned Number for the unit (e.g., 0x2700 for unitless).
 * @note No-op unless this is a 0x2904 descriptor with a value of at least 7 bytes.
 */
void BLEDescriptor::setUnit(uint16_t unit) {
  if (isPresentationFormat() && _impl && _impl->value.size() >= 7) {
    _impl->value[2] = unit & 0xFF;
    _impl->value[3] = unit >> 8;
  }
}

/**
 * @brief Set the namespace field of a Presentation Format descriptor (byte 4).
 * @param ns Bluetooth namespace (typically 0x01 for Bluetooth SIG).
 * @note No-op unless this is a 0x2904 descriptor with a value of at least 7 bytes.
 */
void BLEDescriptor::setNamespace(uint8_t ns) {
  if (isPresentationFormat() && _impl && _impl->value.size() >= 7) {
    _impl->value[4] = ns;
  }
}

/**
 * @brief Set the description field of a Presentation Format descriptor (bytes 5-6, little-endian).
 * @param description Bluetooth SIG-defined description enumeration value.
 * @note No-op unless this is a 0x2904 descriptor with a value of at least 7 bytes.
 */
void BLEDescriptor::setFormatDescription(uint16_t description) {
  if (isPresentationFormat() && _impl && _impl->value.size() >= 7) {
    _impl->value[5] = description & 0xFF;
    _impl->value[6] = description >> 8;
  }
}

/**
 * @brief Register a callback invoked when a client reads this descriptor.
 * @param handler The read callback, or nullptr to clear. Invoked in the BLE
 *                stack context before the read response is sent.
 */
void BLEDescriptor::onRead(ReadHandler handler) {
  BLE_CHECK_IMPL();
  impl.onReadCb = handler;
}

/**
 * @brief Register a callback invoked when a client writes to this descriptor.
 * @param handler The write callback, or nullptr to clear. Invoked in the BLE
 *                stack context after the value has been updated.
 */
void BLEDescriptor::onWrite(WriteHandler handler) {
  BLE_CHECK_IMPL();
  impl.onWriteCb = handler;
}

/**
 * @brief Remove all previously registered callbacks from this descriptor.
 * @note Sets both onRead and onWrite handlers to nullptr.
 */
void BLEDescriptor::resetCallbacks() {
  BLE_CHECK_IMPL();
  impl.onReadCb = nullptr;
  impl.onWriteCb = nullptr;
}

/**
 * @brief Construct a standalone descriptor with a given UUID and max value length.
 * @param uuid UUID for the descriptor.
 * @param maxLength Maximum value length in bytes. The internal buffer is
 *                  reserve()'d to this size but starts empty.
 * @note Prefer BLECharacteristic::createDescriptor() to add a descriptor to a
 *       characteristic directly. This constructor creates an unattached Impl.
 */
BLEDescriptor::BLEDescriptor(const BLEUUID &uuid, uint16_t maxLength) : _impl(nullptr) {
  auto d = std::make_shared<BLEDescriptor::Impl>();
  d->uuid = uuid;
  d->value.reserve(maxLength);
  _impl = d;
}

/**
 * @brief Set the descriptor value from a raw byte buffer.
 * @param data Pointer to the data bytes to copy.
 * @param length Number of bytes to copy from @p data.
 * @note Thread-safe: acquires the descriptor mutex before modifying the
 *       internal buffer. No-op if the handle is invalid (BLE_CHECK_IMPL guard).
 */
void BLEDescriptor::setValue(const uint8_t *data, size_t length) {
  BLE_CHECK_IMPL();
  log_d("Descriptor %s: setValue len=%u", impl.uuid.toString().c_str(), length);
  BLELockGuard lock(impl.mtx);
  impl.value.assign(data, data + length);
}

/**
 * @brief Get a pointer to the raw descriptor value.
 * @param length If non-null, receives the length of the value in bytes.
 *               Set to 0 when the handle is invalid.
 * @return Pointer to the internal value buffer, or nullptr if the handle is
 *         invalid or the value is empty.
 * @note Thread-safe: acquires the descriptor mutex. The returned pointer is
 *       only valid while no concurrent setValue() can occur.
 */
const uint8_t *BLEDescriptor::getValue(size_t *length) const {
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

/**
 * @brief Create a Characteristic User Description descriptor (UUID 0x2901).
 * @param text Human-readable description text to initialize the value with.
 * @return A new BLEDescriptor configured as a User Description (0x2901).
 */
BLEDescriptor BLEDescriptor::createUserDescription(const String &text) {
  auto d = std::make_shared<BLEDescriptor::Impl>();
  d->uuid = BLEUUID(static_cast<uint16_t>(0x2901));
  d->value.assign(text.c_str(), text.c_str() + text.length());
  return BLEDescriptor(d);
}

/**
 * @brief Create a Characteristic Presentation Format descriptor (UUID 0x2904).
 * @return A new BLEDescriptor configured as a Presentation Format with a
 *         7-byte zero-initialized value (format, exponent, unit, namespace, description).
 */
BLEDescriptor BLEDescriptor::createPresentationFormat() {
  auto d = std::make_shared<BLEDescriptor::Impl>();
  d->uuid = BLEUUID(static_cast<uint16_t>(0x2904));
  d->value.resize(7, 0);
  return BLEDescriptor(d);
}

/**
 * @brief Create a Client Characteristic Configuration Descriptor (UUID 0x2902).
 * @return A new BLEDescriptor configured as a CCCD with a 2-byte
 *         zero-initialized value (notifications and indications both disabled).
 * @note On NimBLE, the stack auto-creates the CCCD for NOTIFY/INDICATE
 *       characteristics; manually adding one causes a validation error.
 */
BLEDescriptor BLEDescriptor::createCCCD() {
  auto d = std::make_shared<BLEDescriptor::Impl>();
  d->uuid = BLEUUID(static_cast<uint16_t>(0x2902));
  d->value.resize(2, 0);
  return BLEDescriptor(d);
}

#endif /* BLE_ENABLED */
