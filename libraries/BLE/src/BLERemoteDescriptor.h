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

#pragma once

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include "WString.h"
#include "BTStatus.h"
#include "BLEUUID.h"
#include <memory>

class BLERemoteCharacteristic;

/**
 * @brief Remote GATT descriptor discovered on a peer device.
 *
 * Shared handle. Obtained via BLERemoteCharacteristic::getDescriptor().
 */
class BLERemoteDescriptor {
public:
  BLERemoteDescriptor();
  ~BLERemoteDescriptor() = default;
  BLERemoteDescriptor(const BLERemoteDescriptor &) = default;
  BLERemoteDescriptor &operator=(const BLERemoteDescriptor &) = default;
  BLERemoteDescriptor(BLERemoteDescriptor &&) = default;
  BLERemoteDescriptor &operator=(BLERemoteDescriptor &&) = default;

  /**
   * @brief Check whether this handle refers to a valid remote descriptor.
   * @return True if the underlying descriptor data is present, false otherwise.
   */
  explicit operator bool() const;

  /**
   * @brief Read the descriptor value from the remote device.
   * @param timeoutMs Maximum time in milliseconds to wait for a response.
   * @return The value as a String, or an empty String on failure/timeout.
   */
  String readValue(uint32_t timeoutMs = 3000);

  /**
   * @brief Read the descriptor value and interpret the first byte as uint8_t.
   * @param timeoutMs Maximum time in milliseconds to wait for a response.
   * @return The first byte of the value, or 0 on failure.
   */
  uint8_t readUInt8(uint32_t timeoutMs = 3000);

  /**
   * @brief Read the descriptor value and interpret the first two bytes as uint16_t.
   * @param timeoutMs Maximum time in milliseconds to wait for a response.
   * @return The little-endian 16-bit value, or 0 on failure.
   */
  uint16_t readUInt16(uint32_t timeoutMs = 3000);

  /**
   * @brief Read the descriptor value and interpret the first four bytes as uint32_t.
   * @param timeoutMs Maximum time in milliseconds to wait for a response.
   * @return The little-endian 32-bit value, or 0 on failure.
   */
  uint32_t readUInt32(uint32_t timeoutMs = 3000);

  /**
   * @brief Write a value to the remote descriptor.
   * @param data         Pointer to the data to write.
   * @param len          Number of bytes to write.
   * @param withResponse If true, use Write Request (ATT); if false, use Write Command (no ACK).
   * @return BTStatus indicating success or failure.
   */
  BTStatus writeValue(const uint8_t *data, size_t len, bool withResponse = true);
  BTStatus writeValue(const String &value, bool withResponse = true);
  BTStatus writeValue(uint8_t value, bool withResponse = true);

  /**
   * @brief Get the parent characteristic of this descriptor.
   * @return The parent BLERemoteCharacteristic handle.
   */
  BLERemoteCharacteristic getRemoteCharacteristic() const;

  /**
   * @brief Get the UUID of this descriptor.
   * @return The descriptor UUID.
   */
  BLEUUID getUUID() const;

  /**
   * @brief Get the attribute handle of this descriptor.
   * @return The GATT attribute handle.
   */
  uint16_t getHandle() const;

  /**
   * @brief Get a human-readable representation of this descriptor.
   * @return A String describing the descriptor UUID and handle.
   */
  String toString() const;

  struct Impl;

private:
  explicit BLERemoteDescriptor(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;
  friend class BLERemoteCharacteristic;
};

#endif /* BLE_ENABLED */
