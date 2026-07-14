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
#include "esp32-hal-log.h"

BLEDescriptor::BLEDescriptor() : _impl(nullptr) {}

BLEDescriptor::operator bool() const {
  return _impl != nullptr;
}

BLEUUID BLEDescriptor::getUUID() const {
  return _impl ? _impl->uuid : BLEUUID();
}

uint16_t BLEDescriptor::getHandle() const {
  return _impl ? _impl->handle : 0;
}

BLECharacteristic BLEDescriptor::getCharacteristic() const {
  // Non-owning handle: the characteristic lifetime is owned by the service, so
  // the returned handle must not outlive it.
  return _impl && _impl->chr ? BLECharacteristic(_impl->chr->shared_from_this()) : BLECharacteristic();
}

String BLEDescriptor::toString() const {
  BLE_CHECK_IMPL("BLEDescriptor(null)");
  return "BLEDescriptor(uuid=" + impl.uuid.toString() + ")";
}

void BLEDescriptor::setValue(const String &value) {
  setValue(reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
}

size_t BLEDescriptor::getLength() const {
  return _impl ? _impl->value.size() : 0;
}

bool BLEDescriptor::isUserDescription() const {
  return _impl && _impl->uuid == BLEUUID(static_cast<uint16_t>(0x2901));
}

bool BLEDescriptor::isCCCD() const {
  return _impl && _impl->uuid == BLEUUID(static_cast<uint16_t>(0x2902));
}

bool BLEDescriptor::isPresentationFormat() const {
  return _impl && _impl->uuid == BLEUUID(static_cast<uint16_t>(0x2904));
}

void BLEDescriptor::setUserDescription(const String &description) {
  if (isUserDescription()) {
    setValue(description);
  }
}

String BLEDescriptor::getUserDescription() const {
  if (!isUserDescription() || !_impl) {
    return "";
  }
  return String(reinterpret_cast<const char *>(_impl->value.data()), _impl->value.size());
}

bool BLEDescriptor::getNotifications() const {
  return isCCCD() && _impl && _impl->value.size() >= 2 && (_impl->value[0] & 0x01);
}

bool BLEDescriptor::getIndications() const {
  return isCCCD() && _impl && _impl->value.size() >= 2 && (_impl->value[0] & 0x02);
}

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

// Presentation Format layout (0x2904): [0]=format, [1]=exponent, [2..3]=unit,
// [4]=namespace, [5..6]=description; multi-byte fields are little-endian.

void BLEDescriptor::setFormat(uint8_t format) {
  if (isPresentationFormat() && _impl && _impl->value.size() >= 7) {
    _impl->value[0] = format;
  }
}

void BLEDescriptor::setExponent(int8_t exponent) {
  if (isPresentationFormat() && _impl && _impl->value.size() >= 7) {
    _impl->value[1] = static_cast<uint8_t>(exponent);
  }
}

void BLEDescriptor::setUnit(uint16_t unit) {
  if (isPresentationFormat() && _impl && _impl->value.size() >= 7) {
    _impl->value[2] = unit & 0xFF;
    _impl->value[3] = unit >> 8;
  }
}

void BLEDescriptor::setNamespace(uint8_t ns) {
  if (isPresentationFormat() && _impl && _impl->value.size() >= 7) {
    _impl->value[4] = ns;
  }
}

void BLEDescriptor::setFormatDescription(uint16_t description) {
  if (isPresentationFormat() && _impl && _impl->value.size() >= 7) {
    _impl->value[5] = description & 0xFF;
    _impl->value[6] = description >> 8;
  }
}

void BLEDescriptor::onRead(ReadHandler handler) {
  BLE_CHECK_IMPL();
  impl.onReadCb = handler;
}

void BLEDescriptor::onWrite(WriteHandler handler) {
  BLE_CHECK_IMPL();
  impl.onWriteCb = handler;
}

void BLEDescriptor::resetCallbacks() {
  BLE_CHECK_IMPL();
  impl.onReadCb = nullptr;
  impl.onWriteCb = nullptr;
}

BLEDescriptor::BLEDescriptor(const BLEUUID &uuid, uint16_t maxLength) : _impl(nullptr) {
  // Creates an unattached Impl; the buffer is reserved to maxLength but empty.
  auto d = std::make_shared<BLEDescriptor::Impl>();
  d->uuid = uuid;
  d->value.reserve(maxLength);
  _impl = d;
}

void BLEDescriptor::setValue(const uint8_t *data, size_t length) {
  BLE_CHECK_IMPL();
  log_d("Descriptor %s: setValue len=%u", impl.uuid.toString().c_str(), length);
  BLELockGuard lock(impl.mtx);
  impl.value.assign(data, data + length);
}

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

BLEDescriptor BLEDescriptor::createUserDescription(const String &text) {
  auto d = std::make_shared<BLEDescriptor::Impl>();
  d->uuid = BLEUUID(static_cast<uint16_t>(0x2901));
  d->value.assign(text.c_str(), text.c_str() + text.length());
  return BLEDescriptor(d);
}

BLEDescriptor BLEDescriptor::createPresentationFormat() {
  // 7-byte zero-initialized value: format, exponent, unit, namespace, description.
  auto d = std::make_shared<BLEDescriptor::Impl>();
  d->uuid = BLEUUID(static_cast<uint16_t>(0x2904));
  d->value.resize(7, 0);
  return BLEDescriptor(d);
}

BLEDescriptor BLEDescriptor::createCCCD() {
  // 2-byte zero-initialized value: notifications and indications both disabled.
  auto d = std::make_shared<BLEDescriptor::Impl>();
  d->uuid = BLEUUID(static_cast<uint16_t>(0x2902));
  d->value.resize(2, 0);
  return BLEDescriptor(d);
}

#endif /* BLE_ENABLED */
