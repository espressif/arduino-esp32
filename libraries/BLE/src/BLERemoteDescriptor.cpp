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
  // Non-owning handle: the parent characteristic is owned by the discovering service.
  return (_impl && _impl->chr) ? BLERemoteCharacteristic(_impl->chr->shared_from_this()) : BLERemoteCharacteristic();
}

String BLERemoteDescriptor::readValue(uint32_t timeoutMs) {
  if (!readInto(timeoutMs)) {
    return String();
  }
  return String(reinterpret_cast<const char *>(_impl->value.data()), _impl->value.size());
}

uint8_t BLERemoteDescriptor::readUInt8(uint32_t timeoutMs) {
  if (!readInto(timeoutMs)) {
    return 0;
  }
  return static_cast<uint8_t>(bleReadLEUint(_impl->value.data(), _impl->value.size(), 1));
}

uint16_t BLERemoteDescriptor::readUInt16(uint32_t timeoutMs) {
  if (!readInto(timeoutMs)) {
    return 0;
  }
  return static_cast<uint16_t>(bleReadLEUint(_impl->value.data(), _impl->value.size(), 2));
}

uint32_t BLERemoteDescriptor::readUInt32(uint32_t timeoutMs) {
  if (!readInto(timeoutMs)) {
    return 0;
  }
  return bleReadLEUint(_impl->value.data(), _impl->value.size(), 4);
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
