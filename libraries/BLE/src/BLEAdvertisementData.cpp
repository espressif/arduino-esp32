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

#include "BLEAdvertisementData.h"
#include "impl/BLEImplHelpers.h"
#include "esp32-hal-log.h"

// --------------------------------------------------------------------------
// BLEAdvertisementData implementation (stack-agnostic)
//
// All AD type codes and structure formats are defined in the Bluetooth
// Supplement to the Core Specification (CSS), Part A.
// The overall AD structure format (Length | Type | Value) is defined in
// BT Core Spec v5.x, Vol 3, Part C, §11.
// --------------------------------------------------------------------------

/**
 * @brief Append an AD structure (length + type + value) to the payload.
 * @param type The AD type code (CSS Part A assigned value, e.g. 0x01 for Flags).
 * @param data Pointer to the AD value bytes.
 * @param len Length of @p data in bytes.
 * @note Legacy advertising PDUs (ADV_IND, ADV_SCAN_IND, ADV_NONCONN_IND) are limited
 *       to 31 octets of advertising data per BT Core Spec v5.x, Vol 6, Part B, §2.3.1.2.
 *       Extended advertising (BLE 5) allows up to 254 octets per PDU.
 */
void BLEAdvertisementData::addField(uint8_t type, const uint8_t *data, size_t len) {
  if (len + 2 + _payload.size() > 31) {
    log_w("Ad field would exceed 31 bytes, skipping type=0x%02x", type);
    return;
  }
  _payload.push_back(static_cast<uint8_t>(len + 1));
  _payload.push_back(type);
  _payload.insert(_payload.end(), data, data + len);
}

/**
 * @brief Set the AD Flags field (AD type 0x01).
 * @param flags Bitmask of AD flag values.
 * @note CSS Part A, §1.3.  Standard values:
 *       - Bit 0: LE Limited Discoverable Mode
 *       - Bit 1: LE General Discoverable Mode
 *       - Bit 2: BR/EDR Not Supported
 *       - Bit 3: Simultaneous LE+BR/EDR (Controller)
 *       - Bit 4: Simultaneous LE+BR/EDR (Host)
 *       LE-only devices should set bits 1 and 2
 *       (BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP).
 */
void BLEAdvertisementData::setFlags(uint8_t flags) {
  addField(0x01, &flags, 1);
}

/**
 * @brief Add a Complete List of Service UUIDs AD field.
 * @param uuid The service UUID to advertise.
 * @note AD types: 0x03 (Complete 16-bit UUID List), 0x05 (Complete 32-bit),
 *       0x07 (Complete 128-bit) — CSS Part A, §1.1.
 *       All UUID values on the wire are in little-endian byte order as
 *       required by BT Core Spec v5.x, Vol 3, Part B, §2.5.1.
 */
void BLEAdvertisementData::setCompleteServices(const BLEUUID &uuid) {
  uint8_t type;
  uint8_t buf[16];
  size_t len;
  switch (uuid.bitSize()) {
    case 16:
      type = 0x03;
      buf[0] = uuid.data()[3];
      buf[1] = uuid.data()[2];
      len = 2;
      break;
    case 32:
      type = 0x05;
      buf[0] = uuid.data()[3];
      buf[1] = uuid.data()[2];
      buf[2] = uuid.data()[1];
      buf[3] = uuid.data()[0];
      len = 4;
      break;
    default:
    {
      type = 0x07;
      BLEUUID u128 = uuid.to128();
      const uint8_t *be = u128.data();
      for (int i = 0; i < 16; i++) {
        buf[15 - i] = be[i];
      }
      len = 16;
      break;
    }
  }
  addField(type, buf, len);
}

/**
 * @brief Add an Incomplete List of Service UUIDs AD field.
 * @param uuid The service UUID to advertise.
 * @note AD types: 0x02 (Incomplete 16-bit UUID List), 0x04 (32-bit),
 *       0x06 (128-bit) — CSS Part A, §1.1.
 *       "Incomplete" indicates that more UUIDs exist than can fit in the AD payload.
 *       UUID bytes are in little-endian wire order (BT Core Spec v5.x, Vol 3, Part B, §2.5.1).
 */
void BLEAdvertisementData::setPartialServices(const BLEUUID &uuid) {
  uint8_t type;
  uint8_t buf[16];
  size_t len;
  switch (uuid.bitSize()) {
    case 16:
      type = 0x02;
      buf[0] = uuid.data()[3];
      buf[1] = uuid.data()[2];
      len = 2;
      break;
    case 32:
      type = 0x04;
      buf[0] = uuid.data()[3];
      buf[1] = uuid.data()[2];
      buf[2] = uuid.data()[1];
      buf[3] = uuid.data()[0];
      len = 4;
      break;
    default:
    {
      type = 0x06;
      BLEUUID u128 = uuid.to128();
      const uint8_t *be = u128.data();
      for (int i = 0; i < 16; i++) {
        buf[15 - i] = be[i];
      }
      len = 16;
      break;
    }
  }
  addField(type, buf, len);
}

/**
 * @brief Append a service UUID via a Complete List AD field.
 * @param uuid The service UUID to add.
 */
void BLEAdvertisementData::addServiceUUID(const BLEUUID &uuid) {
  setCompleteServices(uuid);
}

/**
 * @brief Set a Service Data AD field.
 * @param uuid The service UUID this data belongs to (16-bit or 128-bit).
 * @param data Pointer to the service data payload.
 * @param len Length of @p data in bytes.
 * @note AD types: 0x16 (Service Data — 16-bit UUID), 0x20 (32-bit), 0x21 (128-bit)
 *       — CSS Part A, §1.11.
 *       Format: [UUID (LE)] [service-specific data].
 *       UUID bytes are in little-endian wire order.
 */
void BLEAdvertisementData::setServiceData(const BLEUUID &uuid, const uint8_t *data, size_t len) {
  std::vector<uint8_t> buf;
  if (uuid.bitSize() == 16) {
    buf.push_back(uuid.data()[3]);
    buf.push_back(uuid.data()[2]);
  } else {
    BLEUUID u128 = uuid.to128();
    const uint8_t *be = u128.data();
    for (int i = 15; i >= 0; i--) {
      buf.push_back(be[i]);
    }
  }
  buf.insert(buf.end(), data, data + len);
  uint8_t type = (uuid.bitSize() == 16) ? 0x16 : 0x21;
  addField(type, buf.data(), buf.size());
}

/**
 * @brief Set a Manufacturer Specific Data AD field (type 0xFF).
 * @param companyId Bluetooth SIG-assigned company identifier (little-endian on wire).
 * @param data Pointer to the manufacturer-specific payload (after the company ID).
 * @param len Length of @p data in bytes.
 * @note CSS Part A, §1.4.  Format: [Company ID low byte] [Company ID high byte] [data...].
 *       Company identifiers are assigned in the Bluetooth Assigned Numbers document.
 */
void BLEAdvertisementData::setManufacturerData(uint16_t companyId, const uint8_t *data, size_t len) {
  std::vector<uint8_t> buf;
  buf.push_back(companyId & 0xFF);
  buf.push_back((companyId >> 8) & 0xFF);
  buf.insert(buf.end(), data, data + len);
  addField(0xFF, buf.data(), buf.size());
}

/**
 * @brief Set the local name AD field.
 * @param name The device name string (UTF-8).
 * @param complete If true, emits Complete Local Name (0x09); if false, Shortened Local Name (0x08).
 * @note CSS Part A, §1.2.  The "Shortened" name is used when the full name does not fit
 *       in the 31-byte legacy AD payload; it must be a prefix of the complete name.
 *       The complete name is the definitive name returned by the GAP Device Name
 *       characteristic (BT Core Spec v5.x, Vol 3, Part C, §12.1).
 */
void BLEAdvertisementData::setName(const String &name, bool complete) {
  addField(complete ? 0x09 : 0x08, reinterpret_cast<const uint8_t *>(name.c_str()), name.length());
}

/**
 * @brief Set a Shortened Local Name AD field.
 * @param name The shortened device name string (must be a prefix of the complete name).
 */
void BLEAdvertisementData::setShortName(const String &name) {
  setName(name, false);
}

/**
 * @brief Set the Appearance AD field (AD type 0x19).
 * @param appearance GAP appearance value (16-bit, stored little-endian on wire).
 * @note CSS Part A, §1.12.  Appearance values are defined in the Bluetooth Assigned
 *       Numbers document (GAP Appearance Values).  Examples: 0x0000 = Unknown,
 *       0x0180 = Generic Heart Rate Sensor, 0x0340 = Generic Remote Control.
 */
void BLEAdvertisementData::setAppearance(uint16_t appearance) {
  uint8_t buf[2] = {static_cast<uint8_t>(appearance & 0xFF), static_cast<uint8_t>(appearance >> 8)};
  addField(0x19, buf, 2);
}

/**
 * @brief Set the Peripheral Preferred Connection Parameters AD field (AD type 0x12).
 * @param minInterval Minimum connection interval in units of 1.25 ms (range 6–3200).
 * @param maxInterval Maximum connection interval in units of 1.25 ms (range 6–3200).
 * @note CSS Part A, §1.13.  Stored as four little-endian bytes: [minInt_lo, minInt_hi,
 *       maxInt_lo, maxInt_hi].  The slave latency and supervision timeout fields of the
 *       full PPCP structure are omitted here; use the Connection Parameter Update
 *       procedure after connection for those (BT Core Spec v5.x, Vol 3, Part A, §4.20).
 */
void BLEAdvertisementData::setPreferredParams(uint16_t minInterval, uint16_t maxInterval) {
  uint8_t buf[4] = {
    static_cast<uint8_t>(minInterval & 0xFF),
    static_cast<uint8_t>(minInterval >> 8),
    static_cast<uint8_t>(maxInterval & 0xFF),
    static_cast<uint8_t>(maxInterval >> 8),
  };
  addField(0x12, buf, 4);
}

/**
 * @brief Set the TX Power Level AD field (AD type 0x0A).
 * @param txPower Transmit power in dBm (signed, range typically -70..+20 dBm).
 * @note CSS Part A, §1.5.  One signed byte.  Scanners use this together with RSSI
 *       to estimate path loss: Path Loss = TX Power - RSSI.
 */
void BLEAdvertisementData::setTxPower(int8_t txPower) {
  addField(0x0A, reinterpret_cast<uint8_t *>(&txPower), 1);
}

/**
 * @brief Append raw bytes directly to the payload without wrapping in an AD structure.
 * @param data Pointer to a pre-formed AD structure (length + type + value).
 * @param len Total length of the data in bytes.
 * @note Does not enforce the 31-byte limit; caller is responsible for validity.
 */
void BLEAdvertisementData::addRaw(const uint8_t *data, size_t len) {
  _payload.insert(_payload.end(), data, data + len);
}

/**
 * @brief Clear all previously set AD structures, resetting the payload to empty.
 */
void BLEAdvertisementData::clear() {
  _payload.clear();
}

/**
 * @brief Get a pointer to the assembled advertisement payload.
 * @return Pointer to the raw payload bytes.
 */
const uint8_t *BLEAdvertisementData::data() const {
  return _payload.data();
}

/**
 * @brief Get the total length of the assembled advertisement payload.
 * @return Length in bytes.
 */
size_t BLEAdvertisementData::length() const {
  return _payload.size();
}

#endif /* BLE_ENABLED */
