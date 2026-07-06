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
#include "BLEProperty.h"

#include <string.h>

/**
 * @file
 * @brief Stack-agnostic @ref BLERemoteCharacteristic implementation; GATT I/O
 *        is completed in the selected backend.
 * @note Some stacks discover descriptors or subscription state lazily on the
 *       first path that needs them. Avoid re-entrant GATT calls from user code
 *       while holding locks the host may also need (classic client deadlock
 *       pattern when discovery and notify registration overlap).
 */

BLERemoteCharacteristic::BLERemoteCharacteristic() : _impl(nullptr) {}

BLERemoteCharacteristic::operator bool() const {
  return _impl != nullptr;
}

BLEUUID BLERemoteCharacteristic::getUUID() const {
  return _impl ? _impl->uuid : BLEUUID();
}

uint16_t BLERemoteCharacteristic::getHandle() const {
  return _impl ? _impl->handle : 0;
}

// The stored properties bitmask uses the spec-defined characteristic property
// bits (Vol 3 Part G 3.3.1.1), which map directly onto BLEProperty.
bool BLERemoteCharacteristic::canRead() const {
  return _impl && (_impl->properties & static_cast<uint8_t>(BLEProperty::Read));
}

bool BLERemoteCharacteristic::canWrite() const {
  return _impl && (_impl->properties & static_cast<uint8_t>(BLEProperty::Write));
}

bool BLERemoteCharacteristic::canWriteNoResponse() const {
  return _impl && (_impl->properties & static_cast<uint8_t>(BLEProperty::WriteNR));
}

bool BLERemoteCharacteristic::canNotify() const {
  return _impl && (_impl->properties & static_cast<uint8_t>(BLEProperty::Notify));
}

bool BLERemoteCharacteristic::canIndicate() const {
  return _impl && (_impl->properties & static_cast<uint8_t>(BLEProperty::Indicate));
}

bool BLERemoteCharacteristic::canBroadcast() const {
  return _impl && (_impl->properties & static_cast<uint8_t>(BLEProperty::Broadcast));
}

const uint8_t *BLERemoteCharacteristic::readRawData(size_t *len) {
  if (!_impl || _impl->value.empty()) {
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

String BLERemoteCharacteristic::readValue(uint32_t timeoutMs) {
  if (!readInto(timeoutMs)) {
    return String();
  }
  return String(reinterpret_cast<const char *>(_impl->value.data()), _impl->value.size());
}

uint8_t BLERemoteCharacteristic::readUInt8(uint32_t timeoutMs) {
  if (!readInto(timeoutMs)) {
    return 0;
  }
  return static_cast<uint8_t>(bleReadLEUint(_impl->value.data(), _impl->value.size(), 1));
}

uint16_t BLERemoteCharacteristic::readUInt16(uint32_t timeoutMs) {
  if (!readInto(timeoutMs)) {
    return 0;
  }
  return static_cast<uint16_t>(bleReadLEUint(_impl->value.data(), _impl->value.size(), 2));
}

uint32_t BLERemoteCharacteristic::readUInt32(uint32_t timeoutMs) {
  if (!readInto(timeoutMs)) {
    return 0;
  }
  return bleReadLEUint(_impl->value.data(), _impl->value.size(), 4);
}

float BLERemoteCharacteristic::readFloat(uint32_t timeoutMs) {
  // memcpy type-punning avoids UB; assumes little-endian IEEE 754 on the wire.
  uint32_t raw = readUInt32(timeoutMs);
  float f;
  memcpy(&f, &raw, sizeof(f));
  return f;
}

size_t BLERemoteCharacteristic::readValue(uint8_t *buf, size_t bufLen, uint32_t timeoutMs) {
  if (!readInto(timeoutMs)) {
    return 0;
  }
  size_t copyLen = (_impl->value.size() < bufLen) ? _impl->value.size() : bufLen;
  memcpy(buf, _impl->value.data(), copyLen);
  return copyLen;
}

BTStatus BLERemoteCharacteristic::writeValue(const String &value, bool withResponse) {
  return writeValue(reinterpret_cast<const uint8_t *>(value.c_str()), value.length(), withResponse);
}

BTStatus BLERemoteCharacteristic::writeValue(uint8_t value, bool withResponse) {
  return writeValue(&value, 1, withResponse);
}

BLERemoteService BLERemoteCharacteristic::getRemoteService() const {
  // Non-owning handle: the parent service is owned by the discovering client.
  return (_impl && _impl->service) ? BLERemoteService(_impl->service->shared_from_this()) : BLERemoteService();
}

String BLERemoteCharacteristic::toString() const {
  BLE_CHECK_IMPL("BLERemoteCharacteristic(empty)");
  return "BLERemoteCharacteristic(uuid=" + impl.uuid.toString() + ")";
}

#endif /* BLE_ENABLED */
