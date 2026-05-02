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

#include <stdint.h>
#include "WString.h"

/**
 * @brief Stack-agnostic BLE UUID type supporting 16-bit, 32-bit, and 128-bit UUIDs.
 *
 * Stores internally as uint8_t[16] in big-endian (display) order.
 * A bitSize of 0 indicates an invalid/unset UUID.
 *
 * String parsing accepts:
 *   - "180D" (16-bit)
 *   - "0000180D" (32-bit)
 *   - "0000180d-0000-1000-8000-00805f9b34fb" (128-bit with dashes)
 *   - "0000180d00001000800000805f9b34fb" (128-bit without dashes)
 *
 * @note Backend conversion to NimBLE/Bluedroid native formats happens in Impl files.
 */
class BLEUUID {
public:
  /**
   * @brief Construct an invalid (empty) UUID.
   */
  BLEUUID();

  /**
   * @brief Construct from a string representation.
   * @param uuid String in any of the accepted formats (case-insensitive).
   */
  BLEUUID(const char *uuid);

  /**
   * @brief Construct a 16-bit UUID.
   * @param uuid16 The 16-bit UUID value.
   */
  BLEUUID(uint16_t uuid16);

  /**
   * @brief Construct a 32-bit UUID.
   * @param uuid32 The 32-bit UUID value.
   */
  BLEUUID(uint32_t uuid32);

  /**
   * @brief Construct a 128-bit UUID from raw bytes in big-endian order.
   * @param uuid128 16-byte array where uuid128[0] is the most significant byte.
   */
  BLEUUID(const uint8_t uuid128[16]);

  /**
   * @brief Construct a 128-bit UUID from raw bytes with explicit byte order.
   * @param data    Pointer to 16 bytes of UUID data.
   * @param len     Must be 16 (validated at runtime).
   * @param reverse If true, bytes are in little-endian order and will be reversed.
   */
  BLEUUID(const uint8_t *data, size_t len, bool reverse);

  /**
   * @brief Test equality against another UUID.
   * @param other The UUID to compare with.
   * @return True if both UUIDs represent the same value (compared at 128-bit width).
   */
  bool operator==(const BLEUUID &other) const;

  /**
   * @brief Test inequality against another UUID.
   * @param other The UUID to compare with.
   * @return True if the UUIDs differ.
   */
  bool operator!=(const BLEUUID &other) const;

  /**
   * @brief Lexicographic less-than comparison (for use in ordered containers).
   * @param other The UUID to compare with.
   * @return True if this UUID is lexicographically less than @p other.
   */
  bool operator<(const BLEUUID &other) const;

  /**
   * @brief Convert to human-readable string.
   *
   * Format depends on bit size:
   * - 16-bit: "180d"
   * - 32-bit: "0000180d"
   * - 128-bit: "0000180d-0000-1000-8000-00805f9b34fb"
   *
   * @return The UUID as a lowercase hex String, or an empty String if invalid.
   */
  String toString() const;

  /**
   * @brief Get the native bit size (16, 32, or 128).
   * @return The bit size, or 0 if the UUID is invalid/unset.
   */
  uint8_t bitSize() const;

  /**
   * @brief Convert to full 128-bit form using the Bluetooth Base UUID.
   * @return A new BLEUUID with bitSize() == 128.
   */
  BLEUUID to128() const;

  /**
   * @brief Check if the UUID has been set to a valid value.
   * @return True if the UUID is valid (bitSize() != 0).
   */
  bool isValid() const;

  /**
   * @brief Equivalent to isValid().
   * @return True if the UUID is valid.
   */
  explicit operator bool() const {
    return isValid();
  }

  /**
   * @brief Access the internal 128-bit representation (big-endian).
   * @return Pointer to the 16-byte internal array.
   * @note Only meaningful for 128-bit UUIDs or after calling to128().
   */
  const uint8_t *data() const {
    return _uuid;
  }

  /**
   * @brief Get the 16-bit UUID value.
   * @return The 16-bit value.
   * @note Only valid when bitSize() == 16.
   */
  uint16_t toUint16() const;

  /**
   * @brief Get the 32-bit UUID value.
   * @return The 32-bit value.
   * @note Only valid when bitSize() == 32.
   */
  uint32_t toUint32() const;

private:
  uint8_t _uuid[16]{};
  uint8_t _bitSize = 0;

  void parseString(const char *str, size_t len);
};

#endif /* BLE_ENABLED */
