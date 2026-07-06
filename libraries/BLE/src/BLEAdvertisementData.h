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

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include <stdint.h>
#include <vector>
#include "WString.h"
#include "BLEUUID.h"

/**
 * @brief AD Flags -- Core Spec Vol 3, Part C, Section 11.1.3.
 *
 * Combine with bitwise OR: `BLEAdvFlag::GeneralDisc | BLEAdvFlag::BrEdrNotSupported`.
 */
enum class BLEAdvFlag : uint8_t {
  LimitedDisc = 0x01,       ///< LE Limited Discoverable Mode
  GeneralDisc = 0x02,       ///< LE General Discoverable Mode
  BrEdrNotSupported = 0x04  ///< BR/EDR Not Supported (BLE-only device)
};

inline constexpr BLEAdvFlag operator|(BLEAdvFlag a, BLEAdvFlag b) {
  return static_cast<BLEAdvFlag>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

/**
 * @brief Builder for raw BLE advertisement data payloads.
 *
 * Constructs AD structures per Core Spec Vol 3, Part C, Section 11. By default a
 * payload targets the 31-octet legacy advertising/scan-response limit; construct
 * with @ref BLEAdvertisementData(size_t) to build a larger extended-advertising
 * payload. A field that would overflow the active limit is skipped with a
 * warning. setName() falls back to a Shortened Local Name (0x08) when the full
 * name does not fit. UUID and multi-byte values are emitted in little-endian wire
 * order.
 */
class BLEAdvertisementData {
public:
  /// Maximum AD bytes in a legacy advertising / scan-response PDU (Vol 6, Part B, §2.3.1.2).
  static constexpr size_t LEGACY_MAX_PAYLOAD = 31;
  /// Maximum AD bytes the host can stage for one extended advertising set (Vol 4, Part E, §7.8.54).
  static constexpr size_t EXTENDED_MAX_PAYLOAD = 1650;

  /// Construct a legacy (31-octet) advertisement payload.
  BLEAdvertisementData() = default;

  /**
   * @brief Construct an extended-advertising payload with a larger byte budget.
   * @param maxPayloadLen Payload cap in bytes; clamped to
   *        [@ref LEGACY_MAX_PAYLOAD, @ref EXTENDED_MAX_PAYLOAD]. Use this only for
   *        data fed to the BLE5 `setExt*`/`setPeriodicAdvData` paths.
   */
  explicit BLEAdvertisementData(size_t maxPayloadLen);

  /**
   * @brief Set the AD Flags field (typically GeneralDisc | BrEdrNotSupported).
   * @param flags Bitmask of BLEAdvFlag values.
   */
  void setFlags(BLEAdvFlag flags);

  /**
   * @brief Add a service UUID to the Complete List of Service UUIDs field.
   * @param uuid The service UUID to advertise.
   */
  void addServiceUUID(const BLEUUID &uuid);

  /**
   * @brief Set a Partial (Incomplete) List of Service UUIDs field.
   * @param uuid The service UUID to advertise.
   */
  void setPartialServices(const BLEUUID &uuid);

  /**
   * @brief Set a Service Data field.
   * @param uuid The service UUID this data belongs to.
   * @param data Pointer to the service data payload.
   * @param len  Length of @p data in bytes.
   */
  void setServiceData(const BLEUUID &uuid, const uint8_t *data, size_t len);

  /**
   * @brief Set a Manufacturer Specific Data field.
   * @param companyId The Bluetooth SIG-assigned company identifier.
   * @param data      Pointer to the manufacturer-specific payload (after the company ID).
   * @param len       Length of @p data in bytes.
   */
  void setManufacturerData(uint16_t companyId, const uint8_t *data, size_t len);

  /**
   * @brief Set the local name field.
   * @param name     The device name string.
   * @param complete If true, emits a Complete Local Name; if false, a Shortened Local Name.
   */
  void setName(const String &name, bool complete = true);

  /**
   * @brief Set the Appearance field.
   * @param appearance GAP appearance value (e.g., 0x0340 for Generic Remote Control).
   */
  void setAppearance(uint16_t appearance);

  /**
   * @brief Set the Slave Connection Interval Range AD field (0x12).
   * @param minInterval Minimum connection interval in units of 1.25 ms.
   * @param maxInterval Maximum connection interval in units of 1.25 ms.
   */
  void setPreferredParams(uint16_t minInterval, uint16_t maxInterval);

  /**
   * @brief Set the TX Power Level field.
   * @param txPower Transmit power in dBm.
   */
  void setTxPower(int8_t txPower);

  /**
   * @brief Append a raw AD structure to the payload.
   * @param data Pointer to a complete AD structure (length + type + value).
   * @param len  Total length of the AD structure in bytes.
   * @note Rejected with a warning if it would push the payload past the active
   *       payload limit (31 octets by default; larger when constructed extended).
   */
  void addRaw(const uint8_t *data, size_t len);

  /**
   * @brief Clear all previously set AD structures.
   */
  void clear();

  /**
   * @brief Get a pointer to the assembled advertisement payload.
   * @return Pointer to the raw payload bytes.
   */
  const uint8_t *data() const;

  /**
   * @brief Get the total length of the assembled advertisement payload.
   * @return Length in bytes.
   */
  size_t length() const;

private:
  std::vector<uint8_t> _payload;
  size_t _maxLen = LEGACY_MAX_PAYLOAD;

  // Append one AD structure: [length][type][head...][tail...]. `tail` may be
  // null for single-segment fields. Rejects the field if its value exceeds the
  // 254-octet single-structure limit or would overflow the active payload cap.
  void appendField(uint8_t type, const uint8_t *head, size_t headLen, const uint8_t *tail = nullptr, size_t tailLen = 0);
  void addField(uint8_t type, const uint8_t *data, size_t len);
};

#endif /* BLE_ENABLED */
