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

#include "impl/BLERemoteTypesBackend.h"
#include "impl/BLEImplHelpers.h"

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

// --------------------------------------------------------------------------
// BLERemoteCharacteristic common API (stack-agnostic)
// --------------------------------------------------------------------------

/**
 * @brief Construct an invalid (empty) remote characteristic handle.
 * @note The handle must be populated via BLERemoteService before use.
 */
BLERemoteCharacteristic::BLERemoteCharacteristic() : _impl(nullptr) {}

/**
 * @brief Check whether this handle refers to a valid remote characteristic.
 * @return True if the underlying implementation is present, false otherwise.
 */
BLERemoteCharacteristic::operator bool() const {
  return _impl != nullptr;
}

/**
 * @brief Get the UUID of this characteristic.
 * @return The characteristic UUID, or a default-constructed (empty) BLEUUID if the handle is invalid.
 */
BLEUUID BLERemoteCharacteristic::getUUID() const {
  return _impl ? _impl->uuid : BLEUUID();
}

/**
 * @brief Read the characteristic value and interpret the first byte as uint8_t.
 * @param timeoutMs Maximum time in milliseconds to wait for the GATT read response.
 * @return The first byte of the value, or 0 if the read fails, times out,
 *         or the response is empty.
 * @note Blocks until the remote device responds or @p timeoutMs elapses.
 */
uint8_t BLERemoteCharacteristic::readUInt8(uint32_t timeoutMs) {
  String v = readValue(timeoutMs);
  return v.length() >= 1 ? static_cast<uint8_t>(v[0]) : 0;
}

/**
 * @brief Read the characteristic value and interpret the first two bytes as uint16_t.
 * @param timeoutMs Maximum time in milliseconds to wait for the GATT read response.
 * @return The little-endian 16-bit value, or 0 if the response contains fewer than 2 bytes.
 * @note Blocks until the remote device responds or @p timeoutMs elapses.
 */
uint16_t BLERemoteCharacteristic::readUInt16(uint32_t timeoutMs) {
  String v = readValue(timeoutMs);
  if (v.length() < 2) {
    return 0;
  }
  return static_cast<uint16_t>(static_cast<uint8_t>(v[0])) | (static_cast<uint16_t>(static_cast<uint8_t>(v[1])) << 8);
}

/**
 * @brief Read the characteristic value and interpret the first four bytes as uint32_t.
 * @param timeoutMs Maximum time in milliseconds to wait for the GATT read response.
 * @return The little-endian 32-bit value, or 0 if the response contains fewer than 4 bytes.
 * @note Blocks until the remote device responds or @p timeoutMs elapses.
 */
uint32_t BLERemoteCharacteristic::readUInt32(uint32_t timeoutMs) {
  String v = readValue(timeoutMs);
  if (v.length() < 4) {
    return 0;
  }
  return static_cast<uint32_t>(static_cast<uint8_t>(v[0])) | (static_cast<uint32_t>(static_cast<uint8_t>(v[1])) << 8)
         | (static_cast<uint32_t>(static_cast<uint8_t>(v[2])) << 16) | (static_cast<uint32_t>(static_cast<uint8_t>(v[3])) << 24);
}

/**
 * @brief Read the characteristic value and interpret the first four bytes as a float.
 * @param timeoutMs Maximum time in milliseconds to wait for the GATT read response.
 * @return The IEEE 754 float value, or 0.0f if the response contains fewer than 4 bytes.
 * @note Uses memcpy for type-punning to avoid undefined behavior. Assumes the remote
 *       device stores the float in little-endian IEEE 754 format.
 */
float BLERemoteCharacteristic::readFloat(uint32_t timeoutMs) {
  uint32_t raw = readUInt32(timeoutMs);
  float f;
  memcpy(&f, &raw, sizeof(f));
  return f;
}

/**
 * @brief Read the characteristic value into a caller-supplied buffer.
 * @param buf       Destination buffer to copy the value into.
 * @param bufLen    Size of @p buf in bytes.
 * @param timeoutMs Maximum time in milliseconds to wait for the GATT read response.
 * @return Number of bytes copied into @p buf. If @p bufLen is smaller than the
 *         received value, the output is silently truncated to @p bufLen bytes.
 * @note Blocks until the remote device responds or @p timeoutMs elapses.
 */
size_t BLERemoteCharacteristic::readValue(uint8_t *buf, size_t bufLen, uint32_t timeoutMs) {
  String v = readValue(timeoutMs);
  size_t copyLen = (v.length() < bufLen) ? v.length() : bufLen;
  memcpy(buf, v.c_str(), copyLen);
  return copyLen;
}

/**
 * @brief Write a String value to the remote characteristic.
 * @param value        The String whose raw bytes are written.
 * @param withResponse If true, use Write Request (ATT); if false, use Write Command (no ACK).
 * @return BTStatus indicating success or failure.
 */
BTStatus BLERemoteCharacteristic::writeValue(const String &value, bool withResponse) {
  return writeValue(reinterpret_cast<const uint8_t *>(value.c_str()), value.length(), withResponse);
}

/**
 * @brief Write a single byte value to the remote characteristic.
 * @param value        The byte to write.
 * @param withResponse If true, use Write Request (ATT); if false, use Write Command (no ACK).
 * @return BTStatus indicating success or failure.
 */
BTStatus BLERemoteCharacteristic::writeValue(uint8_t value, bool withResponse) {
  return writeValue(&value, 1, withResponse);
}

/**
 * @brief Get the parent service of this characteristic.
 * @return A BLERemoteService handle wrapping the parent, or an invalid handle
 *         if the characteristic or its parent service is invalid.
 * @note Returns a non-owning handle; the parent service's lifetime is managed
 *       by the BLEClient that discovered it.
 */
BLERemoteService BLERemoteCharacteristic::getRemoteService() const {
  return (_impl && _impl->service) ? BLERemoteService(_impl->service->shared_from_this())
                                   : BLERemoteService();
}

/**
 * @brief Get a human-readable representation of this characteristic.
 * @return A String describing the characteristic UUID, or a placeholder if the handle is invalid.
 */
String BLERemoteCharacteristic::toString() const {
  BLE_CHECK_IMPL("BLERemoteCharacteristic(empty)");
  return "BLERemoteCharacteristic(uuid=" + impl.uuid.toString() + ")";
}

#endif /* BLE_ENABLED */
