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

#include <stdint.h>
#include <string.h>
#include "WString.h"

/**
 * @brief Unified Bluetooth address type for both BLE and BT Classic.
 *
 * Replaces the separate BLEAddress and BTAddress classes.
 * Internally stores 6 bytes in LSB-first (Bluetooth standard) order,
 * matching NimBLE's native format.
 *
 * Displayed as "AA:BB:CC:DD:EE:FF" (MSB-first, human-readable convention).
 */
class BTAddress {
public:
  /** @brief Address types per Bluetooth Core Spec Vol 6, Part B, 1.3. */
  enum Type : uint8_t {
    Public = 0,
    Random = 1,
    PublicID = 2,
    RandomID = 3,
  };

  /** @brief Constructs a zero address (all bytes 0x00, type Public). */
  BTAddress();

  /**
   * @brief Construct from raw 6-byte array in LSB-first order.
   * @param addr 6-byte address array (addr[0] = LSB).
   * @param type Address type.
   */
  BTAddress(const uint8_t addr[6], Type type = Public);

  /**
   * @brief Construct from "AA:BB:CC:DD:EE:FF" string.
   * Also accepts "AABBCCDDEEFF" (no separators).
   * @param str Address string (case-insensitive).
   */
  BTAddress(const char *str);

  /** @brief Construct from Arduino String. */
  BTAddress(const String &str);

  bool operator==(const BTAddress &other) const;
  bool operator!=(const BTAddress &other) const;
  bool operator<(const BTAddress &other) const;

  /** @brief Returns true if the address is non-zero. */
  explicit operator bool() const;

  /** @brief Returns pointer to the internal 6-byte array (LSB-first). */
  const uint8_t *data() const;

  /** @brief Returns the address type. */
  Type type() const;

  /**
   * @brief Convert to "AA:BB:CC:DD:EE:FF" string representation.
   * @param uppercase If true, use uppercase hex digits.
   */
  String toString(bool uppercase = false) const;

  /** @brief Returns true if all 6 address bytes are zero. */
  bool isZero() const;

private:
  uint8_t _addr[6]{};
  Type _type = Public;

  void parseString(const char *str);
};
