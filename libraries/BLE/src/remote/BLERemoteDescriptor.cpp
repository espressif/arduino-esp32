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

BLERemoteDescriptor::BLERemoteDescriptor() : _impl(nullptr) {}

BLERemoteDescriptor::operator bool() const {
  return _impl != nullptr;
}

BLEUUID BLERemoteDescriptor::getUUID() const {
  return _impl ? _impl->uuid : BLEUUID();
}

uint16_t BLERemoteDescriptor::getHandle() const {
  return _impl ? _impl->handle : 0;
}

BLERemoteCharacteristic BLERemoteDescriptor::getRemoteCharacteristic() const {
  if (!_impl) {
    return BLERemoteCharacteristic();
  }
  auto chr = _impl->chrOwner.lock();
  auto mtx = _impl->cacheMtx;
  BLELockGuard lock(mtx ? mtx->get() : nullptr);
  return (chr && _impl->chr == chr.get()) ? BLERemoteCharacteristic(chr) : BLERemoteCharacteristic();
}

static std::shared_ptr<BLESharedMutex> remoteDescMtx(const BLERemoteDescriptor::Impl *impl) {
  return impl ? impl->cacheMtx : nullptr;
}

String BLERemoteDescriptor::readValue(uint32_t timeoutMs) {
  if (!readInto(timeoutMs)) {
    return String();
  }
  auto mtx = remoteDescMtx(_impl.get());
  BLELockGuard lock(mtx ? mtx->get() : nullptr);
  return String(reinterpret_cast<const char *>(_impl->value.data()), _impl->value.size());
}

uint8_t BLERemoteDescriptor::readUInt8(uint32_t timeoutMs) {
  if (!readInto(timeoutMs)) {
    return 0;
  }
  auto mtx = remoteDescMtx(_impl.get());
  BLELockGuard lock(mtx ? mtx->get() : nullptr);
  return static_cast<uint8_t>(bleReadLEUint(_impl->value.data(), _impl->value.size(), 1));
}

uint16_t BLERemoteDescriptor::readUInt16(uint32_t timeoutMs) {
  if (!readInto(timeoutMs)) {
    return 0;
  }
  auto mtx = remoteDescMtx(_impl.get());
  BLELockGuard lock(mtx ? mtx->get() : nullptr);
  return static_cast<uint16_t>(bleReadLEUint(_impl->value.data(), _impl->value.size(), 2));
}

uint32_t BLERemoteDescriptor::readUInt32(uint32_t timeoutMs) {
  if (!readInto(timeoutMs)) {
    return 0;
  }
  auto mtx = remoteDescMtx(_impl.get());
  BLELockGuard lock(mtx ? mtx->get() : nullptr);
  return bleReadLEUint(_impl->value.data(), _impl->value.size(), 4);
}

const uint8_t *BLERemoteDescriptor::readRawData(size_t *len) const {
  if (!_impl) {
    if (len) {
      *len = 0;
    }
    return nullptr;
  }
  auto mtx = remoteDescMtx(_impl.get());
  BLELockGuard lock(mtx ? mtx->get() : nullptr);
  if (_impl->value.empty()) {
    if (len) {
      *len = 0;
    }
    return nullptr;
  }
  if (len) {
    *len = _impl->value.size();
  }
  return _impl->value.data();
}

bool BLERemoteDescriptor::isUserDescription() const {
  BLE_CHECK_IMPL(false);
  return impl.uuid == BLEUUID(static_cast<uint16_t>(0x2901));
}

bool BLERemoteDescriptor::isCCCD() const {
  BLE_CHECK_IMPL(false);
  return impl.uuid == BLEUUID(static_cast<uint16_t>(0x2902));
}

bool BLERemoteDescriptor::isPresentationFormat() const {
  BLE_CHECK_IMPL(false);
  return impl.uuid == BLEUUID(static_cast<uint16_t>(0x2904));
}

static uint16_t getRemotePresentationFormatField(const BLERemoteDescriptor &desc, const BLERemoteDescriptor::Impl *impl, size_t offset, bool twoBytes) {
  if (!impl || !desc.isPresentationFormat()) {
    return 0;
  }
  auto mtx = remoteDescMtx(impl);
  BLELockGuard lock(mtx ? mtx->get() : nullptr);
  if (impl->value.size() < 7) {
    return 0;
  }
  uint16_t val = impl->value[offset];
  if (twoBytes) {
    val |= static_cast<uint16_t>(impl->value[offset + 1] << 8);
  }
  return val;
}

uint8_t BLERemoteDescriptor::getFormat() const {
  return static_cast<uint8_t>(getRemotePresentationFormatField(*this, _impl.get(), 0, false));
}

int8_t BLERemoteDescriptor::getExponent() const {
  return static_cast<int8_t>(getRemotePresentationFormatField(*this, _impl.get(), 1, false));
}

uint16_t BLERemoteDescriptor::getUnit() const {
  return getRemotePresentationFormatField(*this, _impl.get(), 2, true);
}

uint8_t BLERemoteDescriptor::getNamespace() const {
  return static_cast<uint8_t>(getRemotePresentationFormatField(*this, _impl.get(), 4, false));
}

uint16_t BLERemoteDescriptor::getFormatDescription() const {
  return getRemotePresentationFormatField(*this, _impl.get(), 5, true);
}

BTStatus BLERemoteDescriptor::writeValue(const String &value, bool withResponse) {
  return writeValue(reinterpret_cast<const uint8_t *>(value.c_str()), value.length(), withResponse);
}

BTStatus BLERemoteDescriptor::writeValue(uint8_t value, bool withResponse) {
  return writeValue(&value, 1, withResponse);
}

String BLERemoteDescriptor::toString() const {
  BLE_CHECK_IMPL("BLERemoteDescriptor(empty)");
  return "BLERemoteDescriptor(uuid=" + impl.uuid.toString() + ")";
}

#endif /* BLE_ENABLED */
