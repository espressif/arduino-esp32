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

#include "BLEAdvertisementData.h"
#include "impl/common/BLEImplHelpers.h"
#include "esp32-hal-log.h"

// A single AD structure carries a 1-octet Length field covering Type + Value, so
// its value can be at most 254 octets regardless of the payload budget
// (BT Core Spec v5.x, Vol 3, Part C, §11).
static constexpr size_t BLE_AD_FIELD_VALUE_MAX = 254;

// AD wire layout: a repeated Length|Type|Value structure (BT Core Spec v5.x,
// Vol 3, Part C, §11; AD type codes per CSS Part A).

BLEAdvertisementData::BLEAdvertisementData(size_t maxPayloadLen) {
  if (maxPayloadLen < LEGACY_MAX_PAYLOAD) {
    maxPayloadLen = LEGACY_MAX_PAYLOAD;
  } else if (maxPayloadLen > EXTENDED_MAX_PAYLOAD) {
    maxPayloadLen = EXTENDED_MAX_PAYLOAD;
  }
  _maxLen = maxPayloadLen;
}

// Append an AD structure, optionally from two value segments (e.g. a UUID
// prefix + payload). Skips the field if its value exceeds the single-structure
// limit or would push the payload past the active cap.
void BLEAdvertisementData::appendField(uint8_t type, const uint8_t *head, size_t headLen, const uint8_t *tail, size_t tailLen) {
  const size_t valueLen = headLen + tailLen;
  if (valueLen > BLE_AD_FIELD_VALUE_MAX || valueLen + 2 + _payload.size() > _maxLen) {
    log_e("Ad field (type=0x%02x, %u B) would exceed the %u-byte payload limit, dropping (data lost)", type, (unsigned)valueLen, (unsigned)_maxLen);
    return;
  }
  _payload.push_back(static_cast<uint8_t>(valueLen + 1));
  _payload.push_back(type);
  _payload.insert(_payload.end(), head, head + headLen);
  if (tail && tailLen) {
    _payload.insert(_payload.end(), tail, tail + tailLen);
  }
}

void BLEAdvertisementData::addField(uint8_t type, const uint8_t *data, size_t len) {
  appendField(type, data, len);
}

// AD type 0x01 (Flags), CSS Part A §1.3. LE-only devices set GeneralDisc | BrEdrNotSupported.
void BLEAdvertisementData::setFlags(BLEAdvFlag flags) {
  uint8_t raw = static_cast<uint8_t>(flags);
  addField(0x01, &raw, 1);
}

// Complete Service UUID List: 0x03 (16-bit), 0x05 (32-bit), 0x07 (128-bit); little-endian.
void BLEAdvertisementData::addServiceUUID(const BLEUUID &uuid) {
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

// Incomplete Service UUID List: 0x02 (16-bit), 0x04 (32-bit), 0x06 (128-bit); little-endian.
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

// Service Data: 0x16 (16-bit UUID), 0x20 (32-bit), 0x21 (128-bit), CSS Part A §1.11.
// The AD type follows the UUID's native width (a 32-bit UUID emits 0x20, not the
// 16-byte 0x21 form). BLEUUID::data() holds the full 128-bit big-endian value, so
// the low bytes are read directly without a to128() promotion.
void BLEAdvertisementData::setServiceData(const BLEUUID &uuid, const uint8_t *data, size_t len) {
  uint8_t uuidBuf[16];
  uint8_t type;
  size_t uuidLen;
  switch (uuid.bitSize()) {
    case 16:
      type = 0x16;
      uuidBuf[0] = uuid.data()[3];
      uuidBuf[1] = uuid.data()[2];
      uuidLen = 2;
      break;
    case 32:
      type = 0x20;
      uuidBuf[0] = uuid.data()[3];
      uuidBuf[1] = uuid.data()[2];
      uuidBuf[2] = uuid.data()[1];
      uuidBuf[3] = uuid.data()[0];
      uuidLen = 4;
      break;
    default:
      type = 0x21;
      for (int i = 0; i < 16; i++) {
        uuidBuf[i] = uuid.data()[15 - i];
      }
      uuidLen = 16;
      break;
  }
  // appendField assembles [UUID][data] directly into the payload and enforces
  // both the single-structure and the active payload-size limits.
  appendField(type, uuidBuf, uuidLen, data, len);
}

// Manufacturer Specific Data (0xFF), CSS Part A §1.4: [company ID (LE)] [data...].
void BLEAdvertisementData::setManufacturerData(uint16_t companyId, const uint8_t *data, size_t len) {
  const uint8_t company[2] = {static_cast<uint8_t>(companyId & 0xFF), static_cast<uint8_t>(companyId >> 8)};
  appendField(0xFF, company, sizeof(company), data, len);
}

// Local name: 0x09 (Complete) or 0x08 (Shortened), CSS Part A §1.2. If the full
// name does not fit in the remaining AD space, emit a Shortened Local Name
// (0x08) truncated to what fits rather than dropping the field entirely.
void BLEAdvertisementData::setName(const String &name, bool complete) {
  const size_t nameLen = name.length();
  const size_t used = _payload.size() + 2;  // + AD header (length + type)
  const size_t avail = (used < _maxLen) ? (_maxLen - used) : 0;
  if (avail == 0) {
    log_w("No room for local name in AD payload, skipping");
    return;
  }
  const auto *bytes = reinterpret_cast<const uint8_t *>(name.c_str());
  if (complete && nameLen <= avail) {
    addField(0x09, bytes, nameLen);
  } else {
    addField(0x08, bytes, nameLen < avail ? nameLen : avail);
  }
}

// Appearance (0x19), CSS Part A §1.12; 16-bit little-endian GAP appearance value.
void BLEAdvertisementData::setAppearance(uint16_t appearance) {
  uint8_t buf[2] = {static_cast<uint8_t>(appearance & 0xFF), static_cast<uint8_t>(appearance >> 8)};
  addField(0x19, buf, 2);
}

// Slave Connection Interval Range (0x12), CSS Part A §1.9:
// [minInt_lo, minInt_hi, maxInt_lo, maxInt_hi] in units of 1.25 ms.
void BLEAdvertisementData::setPreferredParams(uint16_t minInterval, uint16_t maxInterval) {
  uint8_t buf[4] = {
    static_cast<uint8_t>(minInterval & 0xFF),
    static_cast<uint8_t>(minInterval >> 8),
    static_cast<uint8_t>(maxInterval & 0xFF),
    static_cast<uint8_t>(maxInterval >> 8),
  };
  addField(0x12, buf, 4);
}

// TX Power Level (0x0A), CSS Part A §1.5; one signed byte in dBm.
void BLEAdvertisementData::setTxPower(int8_t txPower) {
  addField(0x0A, reinterpret_cast<uint8_t *>(&txPower), 1);
}

// Appends pre-formed AD structure(s) verbatim. Enforces the active payload limit
// (31 octets by default; larger when constructed extended) so the controller is
// never handed an oversized PDU.
void BLEAdvertisementData::addRaw(const uint8_t *data, size_t len) {
  if (_payload.size() + len > _maxLen) {
    log_e("Raw AD data (%u B) would exceed the %u-byte payload limit, dropping (data lost)", (unsigned)len, (unsigned)_maxLen);
    return;
  }
  _payload.insert(_payload.end(), data, data + len);
}

void BLEAdvertisementData::clear() {
  _payload.clear();
}

const uint8_t *BLEAdvertisementData::data() const {
  return _payload.data();
}

size_t BLEAdvertisementData::length() const {
  return _payload.size();
}

#endif /* BLE_ENABLED */
