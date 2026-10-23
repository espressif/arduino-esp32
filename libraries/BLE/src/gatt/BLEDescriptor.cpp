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
#include "esp32-hal-log.h"

BLEDescriptor::BLEDescriptor() : _impl(nullptr) {}

BLEDescriptor::operator bool() const {
  return _impl != nullptr;
}

BLEUUID BLEDescriptor::getUUID() const {
  BLE_CHECK_IMPL(BLEUUID());
  return impl.uuid;
}

uint16_t BLEDescriptor::getHandle() const {
  BLE_CHECK_IMPL(0);
  return impl.handle;
}

BLECharacteristic BLEDescriptor::getCharacteristic() const {
  BLE_CHECK_IMPL(BLECharacteristic());
  auto chr = impl.chrOwner.lock();
  if (!chr) {
    return BLECharacteristic();
  }
  BLELockGuard lock(impl.mtx);
  return impl.chr == chr.get() ? BLECharacteristic(chr) : BLECharacteristic();
}

String BLEDescriptor::toString() const {
  BLE_CHECK_IMPL("BLEDescriptor(null)");
  return "BLEDescriptor(uuid=" + impl.uuid.toString() + ")";
}

void BLEDescriptor::setValue(const String &value) {
  setValue(reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
}

size_t BLEDescriptor::getLength() const {
  BLE_CHECK_IMPL(0);
  BLELockGuard lock(impl.mtx);
  return impl.value.size();
}

bool BLEDescriptor::isUserDescription() const {
  BLE_CHECK_IMPL(false);
  return impl.uuid == BLEUUID(static_cast<uint16_t>(0x2901));
}

bool BLEDescriptor::isCCCD() const {
  BLE_CHECK_IMPL(false);
  return impl.uuid == BLEUUID(static_cast<uint16_t>(0x2902));
}

bool BLEDescriptor::isPresentationFormat() const {
  BLE_CHECK_IMPL(false);
  return impl.uuid == BLEUUID(static_cast<uint16_t>(0x2904));
}

void BLEDescriptor::setUserDescription(const String &description) {
  if (isUserDescription()) {
    setValue(description);
  }
}

String BLEDescriptor::getUserDescription() const {
  BLE_CHECK_IMPL("");
  if (!isUserDescription()) {
    return "";
  }
  BLELockGuard lock(impl.mtx);
  return String(reinterpret_cast<const char *>(impl.value.data()), impl.value.size());
}

bool BLEDescriptor::getNotifications() const {
  BLE_CHECK_IMPL(false);
  if (!isCCCD()) {
    return false;
  }
  BLELockGuard lock(impl.mtx);
  return impl.value.size() >= 2 && (impl.value[0] & 0x01);
}

bool BLEDescriptor::getIndications() const {
  BLE_CHECK_IMPL(false);
  if (!isCCCD()) {
    return false;
  }
  BLELockGuard lock(impl.mtx);
  return impl.value.size() >= 2 && (impl.value[0] & 0x02);
}

void BLEDescriptor::setNotifications(bool enable) {
  BLE_CHECK_IMPL();
  if (!isCCCD()) {
    return;
  }
  BLELockGuard lock(impl.mtx);
  if (impl.value.size() < 2) {
    impl.value.resize(2, 0);
  }
  if (enable) {
    impl.value[0] |= 0x01;
  } else {
    impl.value[0] &= ~0x01;
  }
}

void BLEDescriptor::setIndications(bool enable) {
  BLE_CHECK_IMPL();
  if (!isCCCD()) {
    return;
  }
  BLELockGuard lock(impl.mtx);
  if (impl.value.size() < 2) {
    impl.value.resize(2, 0);
  }
  if (enable) {
    impl.value[0] |= 0x02;
  } else {
    impl.value[0] &= ~0x02;
  }
}

// Presentation Format layout (0x2904): [0]=format, [1]=exponent, [2..3]=unit,
// [4]=namespace, [5..6]=description; multi-byte fields are little-endian.

static void setPresentationFormatField(BLEDescriptor &desc, BLEDescriptor::Impl *impl, size_t offset, uint16_t val, bool twoBytes) {
  if (!impl || !desc.isPresentationFormat()) {
    return;
  }
  BLELockGuard lock(impl->mtx);
  if (impl->value.size() < 7) {
    impl->value.resize(7, 0);
  }
  impl->value[offset] = static_cast<uint8_t>(val & 0xFF);
  if (twoBytes) {
    impl->value[offset + 1] = static_cast<uint8_t>(val >> 8);
  }
}

void BLEDescriptor::setFormat(uint8_t format) {
  setPresentationFormatField(*this, _impl.get(), 0, format, false);
}

void BLEDescriptor::setExponent(int8_t exponent) {
  setPresentationFormatField(*this, _impl.get(), 1, static_cast<uint8_t>(exponent), false);
}

void BLEDescriptor::setUnit(uint16_t unit) {
  setPresentationFormatField(*this, _impl.get(), 2, unit, true);
}

void BLEDescriptor::setNamespace(uint8_t ns) {
  setPresentationFormatField(*this, _impl.get(), 4, ns, false);
}

void BLEDescriptor::setFormatDescription(uint16_t description) {
  setPresentationFormatField(*this, _impl.get(), 5, description, true);
}

static uint16_t getPresentationFormatField(const BLEDescriptor &desc, const BLEDescriptor::Impl *impl, size_t offset, bool twoBytes) {
  if (!impl || !desc.isPresentationFormat()) {
    return 0;
  }
  BLELockGuard lock(impl->mtx);
  if (impl->value.size() < 7) {
    return 0;
  }
  uint16_t val = impl->value[offset];
  if (twoBytes) {
    val |= static_cast<uint16_t>(impl->value[offset + 1] << 8);
  }
  return val;
}

uint8_t BLEDescriptor::getFormat() const {
  return static_cast<uint8_t>(getPresentationFormatField(*this, _impl.get(), 0, false));
}

int8_t BLEDescriptor::getExponent() const {
  return static_cast<int8_t>(getPresentationFormatField(*this, _impl.get(), 1, false));
}

uint16_t BLEDescriptor::getUnit() const {
  return getPresentationFormatField(*this, _impl.get(), 2, true);
}

uint8_t BLEDescriptor::getNamespace() const {
  return static_cast<uint8_t>(getPresentationFormatField(*this, _impl.get(), 4, false));
}

uint16_t BLEDescriptor::getFormatDescription() const {
  return getPresentationFormatField(*this, _impl.get(), 5, true);
}

void BLEDescriptor::onRead(ReadHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onReadCb = std::move(handler);
}

void BLEDescriptor::onWrite(WriteHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onWriteCb = std::move(handler);
}

void BLEDescriptor::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onReadCb = nullptr;
  impl.onWriteCb = nullptr;
}

BLEDescriptor::BLEDescriptor(const BLEUUID &uuid, uint16_t maxLength) : _impl(nullptr) {
  // Creates an unattached Impl; the buffer is reserved to maxLength but empty.
  auto d = std::make_shared<BLEDescriptor::Impl>();
  d->uuid = uuid;
  d->maxLen = maxLength;
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
