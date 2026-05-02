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

#include "BLEUUID.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "esp32-hal-log.h"

// Bluetooth Base UUID: 00000000-0000-1000-8000-00805F9B34FB (big-endian)
// This is the well-known Bluetooth Base UUID defined in BT Core Spec v5.x,
// Vol 3, Part B, §2.5.1.  All SIG-assigned 16-bit and 32-bit UUIDs are derived
// from it by substituting the first 4 bytes: UUID128 = value32 × 2^96 + BaseUUID.
// Stored in big-endian byte order to match the BLEUUID internal representation.
static constexpr uint8_t kBaseUUID[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0x80, 0x5f, 0x9b, 0x34, 0xfb};

/**
 * @brief Convert a single hex character to its 4-bit numeric value.
 * @param c ASCII hex character ('0'-'9', 'a'-'f', 'A'-'F').
 * @return The nibble value (0-15), or 0xFF if the character is not valid hex.
 */
static uint8_t hexCharToNibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return 0xFF;
}

/**
 * @brief Parse two consecutive hex characters into a single byte.
 * @param hex Pointer to at least 2 hex ASCII characters.
 * @param out Receives the decoded byte value on success.
 * @return True if both characters are valid hex, false otherwise.
 */
static bool hexToByte(const char *hex, uint8_t &out) {
  uint8_t hi = hexCharToNibble(hex[0]);
  uint8_t lo = hexCharToNibble(hex[1]);
  if (hi == 0xFF || lo == 0xFF) {
    return false;
  }
  out = (hi << 4) | lo;
  return true;
}

/**
 * @brief Construct an invalid (empty) UUID with bitSize 0.
 */
BLEUUID::BLEUUID() {}

/**
 * @brief Construct a UUID from a C-string representation.
 * @param uuid Null-terminated string; accepts an optional "0x"/"0X" prefix.
 * @note A null pointer leaves the UUID in the invalid state.
 */
BLEUUID::BLEUUID(const char *uuid) {
  if (uuid == nullptr) {
    return;
  }
  size_t len = strlen(uuid);
  if (len >= 2 && uuid[0] == '0' && (uuid[1] == 'x' || uuid[1] == 'X')) {
    uuid += 2;
    len -= 2;
  }
  parseString(uuid, len);
}

/**
 * @brief Construct a 16-bit UUID, pre-filling the Bluetooth Base UUID template.
 * @param uuid16 The 16-bit UUID value, placed at bytes [2..3] of the base UUID.
 */
BLEUUID::BLEUUID(uint16_t uuid16) {
  memcpy(_uuid, kBaseUUID, 16);
  _uuid[2] = (uuid16 >> 8) & 0xFF;
  _uuid[3] = uuid16 & 0xFF;
  _bitSize = 16;
}

/**
 * @brief Construct a 32-bit UUID, pre-filling the Bluetooth Base UUID template.
 * @param uuid32 The 32-bit UUID value, placed at bytes [0..3] of the base UUID.
 */
BLEUUID::BLEUUID(uint32_t uuid32) {
  memcpy(_uuid, kBaseUUID, 16);
  _uuid[0] = (uuid32 >> 24) & 0xFF;
  _uuid[1] = (uuid32 >> 16) & 0xFF;
  _uuid[2] = (uuid32 >> 8) & 0xFF;
  _uuid[3] = uuid32 & 0xFF;
  _bitSize = 32;
}

/**
 * @brief Construct a 128-bit UUID from a big-endian 16-byte array.
 * @param uuid128 Pointer to 16 bytes in big-endian (display) order.
 */
BLEUUID::BLEUUID(const uint8_t uuid128[16]) {
  memcpy(_uuid, uuid128, 16);
  _bitSize = 128;
}

/**
 * @brief Construct a 128-bit UUID from raw bytes with optional byte-order reversal.
 * @param data Pointer to 16 bytes of UUID data.
 * @param len Must be exactly 16; any other value leaves the UUID invalid.
 * @param reverse If true, bytes are reversed from little-endian to big-endian storage.
 */
BLEUUID::BLEUUID(const uint8_t *data, size_t len, bool reverse) {
  if (data == nullptr || len != 16) {
    return;
  }
  if (reverse) {
    for (int i = 0; i < 16; i++) {
      _uuid[i] = data[15 - i];
    }
  } else {
    memcpy(_uuid, data, 16);
  }
  _bitSize = 128;
}

/**
 * @brief Parse a hex string (without "0x" prefix) into the internal UUID buffer.
 * @param str Pointer to the hex characters.
 * @param len Length of the string: 4 (16-bit), 8 (32-bit), 32 or 36 (128-bit).
 * @note Invalid lengths or malformed hex characters are logged via log_e and
 *       leave the UUID in its prior (typically invalid) state.
 */
void BLEUUID::parseString(const char *str, size_t len) {
  if (len == 4) {
    // 16-bit: "180D"
    uint8_t hi, lo;
    if (!hexToByte(str, hi) || !hexToByte(str + 2, lo)) {
      log_e("Invalid 16-bit UUID string");
      return;
    }
    uint16_t val = (static_cast<uint16_t>(hi) << 8) | lo;
    *this = BLEUUID(val);
  } else if (len == 8) {
    // 32-bit: "0000180D"
    uint8_t b[4];
    for (int i = 0; i < 4; i++) {
      if (!hexToByte(str + i * 2, b[i])) {
        log_e("Invalid 32-bit UUID string");
        return;
      }
    }
    uint32_t val = (static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) | (static_cast<uint32_t>(b[2]) << 8) | b[3];
    *this = BLEUUID(val);
  } else if (len == 36) {
    // 128-bit with dashes: "0000180d-0000-1000-8000-00805f9b34fb"
    uint8_t buf[16];
    int byteIdx = 0;
    for (size_t i = 0; i < len && byteIdx < 16;) {
      if (str[i] == '-') {
        i++;
        continue;
      }
      if (i + 1 >= len || !hexToByte(str + i, buf[byteIdx])) {
        log_e("Invalid 128-bit UUID string");
        return;
      }
      byteIdx++;
      i += 2;
    }
    if (byteIdx != 16) {
      log_e("Invalid 128-bit UUID string (wrong byte count)");
      return;
    }
    memcpy(_uuid, buf, 16);
    _bitSize = 128;
  } else if (len == 32) {
    // 128-bit without dashes: "0000180d00001000800000805f9b34fb"
    uint8_t buf[16];
    for (int i = 0; i < 16; i++) {
      if (!hexToByte(str + i * 2, buf[i])) {
        log_e("Invalid 128-bit UUID string (no dashes)");
        return;
      }
    }
    memcpy(_uuid, buf, 16);
    _bitSize = 128;
  } else {
    log_e("UUID string must be 4, 8, 32, or 36 characters (got %u)", (unsigned)len);
  }
}

/**
 * @brief Test equality by comparing the full 16-byte internal arrays.
 * @param other The UUID to compare with.
 * @return True if both are valid and represent the same 128-bit value.
 * @note Two invalid UUIDs are not considered equal.
 */
bool BLEUUID::operator==(const BLEUUID &other) const {
  if (!isValid() || !other.isValid()) {
    return false;
  }
  return memcmp(_uuid, other._uuid, sizeof(_uuid)) == 0;
}

/**
 * @brief Test inequality against another UUID.
 * @param other The UUID to compare with.
 * @return True if the UUIDs differ or either is invalid.
 */
bool BLEUUID::operator!=(const BLEUUID &other) const {
  return !(*this == other);
}

/**
 * @brief Lexicographic less-than for use in ordered containers (std::map, std::set).
 * @param other The UUID to compare with.
 * @return True if this UUID is lexicographically less than @p other.
 * @note Returns false if either UUID is invalid.
 */
bool BLEUUID::operator<(const BLEUUID &other) const {
  if (!isValid() || !other.isValid()) {
    return false;
  }
  return memcmp(_uuid, other._uuid, sizeof(_uuid)) < 0;
}

/**
 * @brief Convert to a lowercase hex string whose format reflects the native bit size.
 * @return "180d" (16-bit), "0000180d" (32-bit),
 *         "0000180d-0000-1000-8000-00805f9b34fb" (128-bit), or "<invalid>".
 */
String BLEUUID::toString() const {
  if (!isValid()) {
    return "<invalid>";
  }

  if (_bitSize == 16) {
    char buf[5];
    snprintf(buf, sizeof(buf), "%02x%02x", _uuid[2], _uuid[3]);
    return String(buf);
  }

  if (_bitSize == 32) {
    char buf[9];
    snprintf(buf, sizeof(buf), "%02x%02x%02x%02x", _uuid[0], _uuid[1], _uuid[2], _uuid[3]);
    return String(buf);
  }

  // 128-bit: "0000180d-0000-1000-8000-00805f9b34fb"
  char buf[37];
  snprintf(
    buf, sizeof(buf), "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", _uuid[0], _uuid[1], _uuid[2], _uuid[3], _uuid[4], _uuid[5],
    _uuid[6], _uuid[7], _uuid[8], _uuid[9], _uuid[10], _uuid[11], _uuid[12], _uuid[13], _uuid[14], _uuid[15]
  );
  return String(buf);
}

/**
 * @brief Get the native bit size (16, 32, or 128).
 * @return The bit size, or 0 if the UUID is invalid/unset.
 */
uint8_t BLEUUID::bitSize() const {
  return _bitSize;
}

/**
 * @brief Return a 128-bit version of this UUID.
 * @return A copy with bitSize() == 128. Returns *this unchanged if already 128-bit
 *         or invalid.
 * @note 16/32-bit constructors already embed the Bluetooth Base UUID, so this
 *       only flips the bitSize field without modifying the stored bytes.
 */
BLEUUID BLEUUID::to128() const {
  if (!isValid() || _bitSize == 128) {
    return *this;
  }
  // For 16-bit and 32-bit, the full 128-bit UUID is already stored in _uuid
  // (constructors fill the base UUID template), so just change the bitSize.
  BLEUUID result = *this;
  result._bitSize = 128;
  return result;
}

/**
 * @brief Check if the UUID has been set to a valid value.
 * @return True if bitSize is non-zero.
 */
bool BLEUUID::isValid() const {
  return _bitSize != 0;
}

/**
 * @brief Extract the 16-bit UUID value from the internal big-endian buffer.
 * @return The 16-bit value from bytes [2..3].
 * @note Only meaningful when bitSize() == 16; otherwise returns an arbitrary value.
 */
uint16_t BLEUUID::toUint16() const {
  return (static_cast<uint16_t>(_uuid[2]) << 8) | _uuid[3];
}

/**
 * @brief Extract the 32-bit UUID value from the internal big-endian buffer.
 * @return The 32-bit value from bytes [0..3].
 * @note Only meaningful when bitSize() == 32; otherwise returns an arbitrary value.
 */
uint32_t BLEUUID::toUint32() const {
  return (static_cast<uint32_t>(_uuid[0]) << 24) | (static_cast<uint32_t>(_uuid[1]) << 16) | (static_cast<uint32_t>(_uuid[2]) << 8) | _uuid[3];
}

#endif /* BLE_ENABLED */
