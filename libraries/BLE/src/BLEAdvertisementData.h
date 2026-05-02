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
#include <vector>
#include "WString.h"
#include "BLEUUID.h"

/**
 * @brief Builder for raw BLE advertisement data payloads.
 *
 * Constructs AD structures per Core Spec Vol 3, Part C, Section 11.
 */
class BLEAdvertisementData {
public:
  /**
   * @brief Set the AD Flags field (typically LE General Discoverable + BR/EDR Not Supported).
   * @param flags Bitmask of AD flag values.
   */
  void setFlags(uint8_t flags);

  /**
   * @brief Set a Complete List of Service UUIDs field.
   * @param uuid The service UUID to advertise.
   */
  void setCompleteServices(const BLEUUID &uuid);

  /**
   * @brief Set a Partial (Incomplete) List of Service UUIDs field.
   * @param uuid The service UUID to advertise.
   */
  void setPartialServices(const BLEUUID &uuid);

  /**
   * @brief Append a service UUID to the existing service list.
   * @param uuid The service UUID to add.
   */
  void addServiceUUID(const BLEUUID &uuid);

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
   * @brief Set the short name field.
   * @param name The device short name string.
   */
  void setShortName(const String &name);

  /**
   * @brief Set the Appearance field.
   * @param appearance GAP appearance value (e.g., 0x0340 for Generic Remote Control).
   */
  void setAppearance(uint16_t appearance);

  /**
   * @brief Set the Peripheral Preferred Connection Parameters field.
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
  void addField(uint8_t type, const uint8_t *data, size_t len);
};

#endif /* BLE_ENABLED */
