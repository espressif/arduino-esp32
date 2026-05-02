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

#include "impl/BLEAdvertisedDeviceImpl.h"
#include "impl/BLEImplHelpers.h"
#include "esp32-hal-log.h"

// --------------------------------------------------------------------------
// BLEAdvertisedDevice::Impl::parsePayload (shared payload parsing logic)
// --------------------------------------------------------------------------

/**
 * @brief Parse a raw BLE advertisement payload into structured fields.
 * @param data Pointer to the raw AD structure bytes.
 * @param len Total length of the payload in bytes.
 * @note Iterates AD structures (length-type-value) and populates name, UUIDs,
 *       service data, manufacturer data, TX power, and appearance fields.
 *       128-bit UUIDs are byte-reversed from the over-the-air little-endian
 *       format to the BLEUUID big-endian internal representation.
 */
void BLEAdvertisedDevice::Impl::parsePayload(const uint8_t *data, size_t len) {
  payload.assign(data, data + len);
  size_t pos = 0;
  while (pos < len) {
    uint8_t adLen = data[pos];
    if (adLen == 0 || pos + 1 + adLen > len) {
      break;
    }
    uint8_t adType = data[pos + 1];
    const uint8_t *adData = data + pos + 2;
    uint8_t dataLen = adLen - 1;

    switch (adType) {
      case 0x01:  // Flags
        break;
      case 0x02:  // Incomplete 16-bit UUIDs
      case 0x03:  // Complete 16-bit UUIDs
        for (size_t i = 0; i + 1 < dataLen; i += 2) {
          uint16_t uuid16 = adData[i] | (adData[i + 1] << 8);
          serviceUUIDs.push_back(BLEUUID(uuid16));
        }
        break;
      case 0x04:  // Incomplete 32-bit UUIDs
      case 0x05:  // Complete 32-bit UUIDs
        for (size_t i = 0; i + 3 < dataLen; i += 4) {
          uint32_t uuid32 = adData[i] | (adData[i + 1] << 8) | (adData[i + 2] << 16) | (adData[i + 3] << 24);
          serviceUUIDs.push_back(BLEUUID(uuid32));
        }
        break;
      case 0x06:  // Incomplete 128-bit UUIDs
      case 0x07:  // Complete 128-bit UUIDs
        for (size_t i = 0; i + 15 < dataLen; i += 16) {
          serviceUUIDs.push_back(BLEUUID(adData + i, 16, true));
        }
        break;
      case 0x08:  // Shortened Local Name
      case 0x09:  // Complete Local Name
        name = String(reinterpret_cast<const char *>(adData), dataLen);
        hasName = true;
        break;
      case 0x0A:  // TX Power Level
        if (dataLen >= 1) {
          txPower = static_cast<int8_t>(adData[0]);
          hasTXPower = true;
        }
        break;
      case 0x19:  // Appearance
        if (dataLen >= 2) {
          appearance = adData[0] | (adData[1] << 8);
          hasAppearance = true;
        }
        break;
      case 0x14:  // 16-bit Service Solicitation UUIDs
      case 0x15:  // 128-bit Service Solicitation UUIDs
        break;
      case 0x16:  // Service Data - 16-bit UUID
        if (dataLen >= 2) {
          ServiceDataEntry entry;
          entry.uuid = BLEUUID(static_cast<uint16_t>(adData[0] | (adData[1] << 8)));
          if (dataLen > 2) {
            entry.data.assign(adData + 2, adData + dataLen);
          }
          serviceData.push_back(std::move(entry));
        }
        break;
      case 0x20:  // Service Data - 32-bit UUID
        if (dataLen >= 4) {
          ServiceDataEntry entry;
          entry.uuid = BLEUUID(static_cast<uint32_t>(adData[0] | (adData[1] << 8) | (adData[2] << 16) | (adData[3] << 24)));
          if (dataLen > 4) {
            entry.data.assign(adData + 4, adData + dataLen);
          }
          serviceData.push_back(std::move(entry));
        }
        break;
      case 0x21:  // Service Data - 128-bit UUID
        if (dataLen >= 16) {
          ServiceDataEntry entry;
          entry.uuid = BLEUUID(adData, 16, true);
          if (dataLen > 16) {
            entry.data.assign(adData + 16, adData + dataLen);
          }
          serviceData.push_back(std::move(entry));
        }
        break;
      case 0xFF:  // Manufacturer Specific Data
        mfgData.assign(adData, adData + dataLen);
        hasMfgData = true;
        break;
      default: break;
    }
    pos += 1 + adLen;
  }
}

/**
 * @brief Reset all parsed advertisement fields to their default (empty) state.
 * @note Called before re-parsing when merging scan response data.
 */
void BLEAdvertisedDevice::Impl::clearParsedFields() {
  name = "";
  txPower = -128;
  appearance = 0;
  mfgData.clear();
  serviceUUIDs.clear();
  serviceData.clear();
  hasName = false;
  hasTXPower = false;
  hasAppearance = false;
  hasMfgData = false;
}

// --------------------------------------------------------------------------
// BLEAdvertisedDevice public API
// --------------------------------------------------------------------------

/**
 * @brief Construct an empty (invalid) advertised device with no backing data.
 */
BLEAdvertisedDevice::BLEAdvertisedDevice() : _impl(nullptr) {}

/**
 * @brief Check whether this handle refers to a valid advertised device.
 * @return True if the underlying Impl data is present.
 */
BLEAdvertisedDevice::operator bool() const {
  return _impl != nullptr;
}

/**
 * @brief Get the advertiser's BLE address.
 * @return The device address, or a default @c BTAddress if invalid.
 */
BTAddress BLEAdvertisedDevice::getAddress() const {
  return _impl ? _impl->address : BTAddress();
}

/**
 * @brief Get the address type (public, random, etc.).
 * @return The address type, or @c Public if invalid.
 */
BTAddress::Type BLEAdvertisedDevice::getAddressType() const {
  return _impl ? _impl->addrType : BTAddress::Type::Public;
}

/**
 * @brief Get the advertised local name.
 * @return The name, or an empty @c String if not present or invalid.
 */
String BLEAdvertisedDevice::getName() const {
  return _impl ? _impl->name : "";
}

/**
 * @brief Get the received signal strength.
 * @return RSSI in dBm, or -128 if unavailable.
 */
int8_t BLEAdvertisedDevice::getRSSI() const {
  return _impl ? _impl->rssi : -128;
}

/**
 * @brief Get the advertised TX power level.
 * @return TX power in dBm, or -128 if not present.
 */
int8_t BLEAdvertisedDevice::getTXPower() const {
  return _impl ? _impl->txPower : -128;
}

/**
 * @brief Get the advertised appearance value.
 * @return GAP appearance code, or 0 if not present.
 */
uint16_t BLEAdvertisedDevice::getAppearance() const {
  return _impl ? _impl->appearance : 0;
}

/**
 * @brief Get the advertisement PDU type.
 * @return The advertisement type, or @c ConnectableScannable if invalid.
 */
BLEAdvType BLEAdvertisedDevice::getAdvType() const {
  return _impl ? _impl->advType : BLEAdvType::ConnectableScannable;
}

/**
 * @brief Get the raw manufacturer-specific data.
 * @param len If non-null, receives the data length in bytes (including company ID).
 * @return Pointer to the data, or nullptr if not present.
 */
const uint8_t *BLEAdvertisedDevice::getManufacturerData(size_t *len) const {
  if (!_impl || _impl->mfgData.empty()) {
    if (len) {
      *len = 0;
    }
    return nullptr;
  }
  if (len) {
    *len = _impl->mfgData.size();
  }
  return _impl->mfgData.data();
}

/**
 * @brief Get the manufacturer-specific data as a lowercase hex-encoded String.
 * @return Hex string, or an empty String if not present.
 */
String BLEAdvertisedDevice::getManufacturerDataString() const {
  BLE_CHECK_IMPL("");
  String s;
  for (uint8_t b : impl.mfgData) {
    char hex[3];
    snprintf(hex, sizeof(hex), "%02x", b);
    s += hex;
  }
  return s;
}

/**
 * @brief Extract the 16-bit company identifier from manufacturer data.
 * @return The company ID (little-endian in the payload), or 0 if fewer than 2 bytes.
 */
uint16_t BLEAdvertisedDevice::getManufacturerCompanyId() const {
  if (!_impl || _impl->mfgData.size() < 2) {
    return 0;
  }
  return _impl->mfgData[0] | (_impl->mfgData[1] << 8);
}

/**
 * @brief Get the number of service UUIDs in the advertisement.
 * @return The count, or 0 if invalid.
 */
size_t BLEAdvertisedDevice::getServiceUUIDCount() const {
  return _impl ? _impl->serviceUUIDs.size() : 0;
}

/**
 * @brief Get a service UUID by index.
 * @param index Zero-based index into the advertised service UUID list.
 * @return The service UUID, or an invalid BLEUUID if index is out of range.
 */
BLEUUID BLEAdvertisedDevice::getServiceUUID(size_t index) const {
  if (!_impl || index >= _impl->serviceUUIDs.size()) {
    return BLEUUID();
  }
  return _impl->serviceUUIDs[index];
}

/**
 * @brief Check if the advertisement contains any service UUIDs.
 * @return True if at least one UUID is present.
 */
bool BLEAdvertisedDevice::haveServiceUUID() const {
  return _impl && !_impl->serviceUUIDs.empty();
}

/**
 * @brief Check if a specific service UUID is advertised.
 * @param uuid The service UUID to search for.
 * @return True if found (compared at 128-bit width via to128()).
 */
bool BLEAdvertisedDevice::isAdvertisingService(const BLEUUID &uuid) const {
  BLE_CHECK_IMPL(false);
  BLEUUID target = uuid.to128();
  for (const auto &u : impl.serviceUUIDs) {
    if (u.to128() == target) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Get the number of service data entries.
 * @return The count, or 0 if invalid.
 */
size_t BLEAdvertisedDevice::getServiceDataCount() const {
  return _impl ? _impl->serviceData.size() : 0;
}

/**
 * @brief Get raw service data by index.
 * @param index Zero-based index into the service data list.
 * @param len If non-null, receives the data length in bytes.
 * @return Pointer to the data, or nullptr if index is out of range.
 */
const uint8_t *BLEAdvertisedDevice::getServiceData(size_t index, size_t *len) const {
  if (!_impl || index >= _impl->serviceData.size()) {
    if (len) {
      *len = 0;
    }
    return nullptr;
  }
  if (len) {
    *len = _impl->serviceData[index].data.size();
  }
  return _impl->serviceData[index].data.data();
}

/**
 * @brief Get service data as a lowercase hex-encoded String.
 * @param index Zero-based index into the service data list.
 * @return Hex string, or an empty String if index is out of range.
 */
String BLEAdvertisedDevice::getServiceDataString(size_t index) const {
  if (!_impl || index >= _impl->serviceData.size()) {
    return "";
  }
  String s;
  for (uint8_t b : _impl->serviceData[index].data) {
    char hex[3];
    snprintf(hex, sizeof(hex), "%02x", b);
    s += hex;
  }
  return s;
}

/**
 * @brief Get the UUID associated with a service data entry.
 * @param index Zero-based index into the service data list.
 * @return The UUID, or an invalid BLEUUID if index is out of range.
 */
BLEUUID BLEAdvertisedDevice::getServiceDataUUID(size_t index) const {
  if (!_impl || index >= _impl->serviceData.size()) {
    return BLEUUID();
  }
  return _impl->serviceData[index].uuid;
}

/**
 * @brief Check if the advertisement contains any service data.
 * @return True if at least one entry is present.
 */
bool BLEAdvertisedDevice::haveServiceData() const {
  return _impl && !_impl->serviceData.empty();
}

/**
 * @brief Get a pointer to the raw advertisement payload.
 * @return Pointer to the raw AD structure bytes, or nullptr if empty.
 */
const uint8_t *BLEAdvertisedDevice::getPayload() const {
  if (!_impl || _impl->payload.empty()) {
    return nullptr;
  }
  return _impl->payload.data();
}

/**
 * @brief Get the total length of the raw advertisement payload.
 * @return Payload length in bytes.
 */
size_t BLEAdvertisedDevice::getPayloadLength() const {
  return _impl ? _impl->payload.size() : 0;
}

/**
 * @brief Whether a GAP local name was present in the AD data.
 * @return True if a name field was parsed.
 */
bool BLEAdvertisedDevice::haveName() const {
  return _impl && _impl->hasName;
}

/**
 * @brief Whether a valid RSSI sample is stored.
 * @return True if RSSI was recorded for this event.
 */
bool BLEAdvertisedDevice::haveRSSI() const {
  return _impl && _impl->hasRSSI;
}

/**
 * @brief Whether a TX power AD field was present.
 * @return True if a TX power field exists.
 */
bool BLEAdvertisedDevice::haveTXPower() const {
  return _impl && _impl->hasTXPower;
}

/**
 * @brief Whether an appearance AD field was present.
 * @return True if the appearance field exists.
 */
bool BLEAdvertisedDevice::haveAppearance() const {
  return _impl && _impl->hasAppearance;
}

/**
 * @brief Whether manufacturer specific data is present.
 * @return True if MFG data was parsed.
 */
bool BLEAdvertisedDevice::haveManufacturerData() const {
  return _impl && _impl->hasMfgData;
}

/**
 * @brief Whether the event indicates a connectable advertiser.
 * @return True if connectable.
 */
bool BLEAdvertisedDevice::isConnectable() const {
  return _impl && _impl->connectable;
}

/**
 * @brief Whether the event indicates a scannable advertiser.
 * @return True if scannable.
 */
bool BLEAdvertisedDevice::isScannable() const {
  return _impl && _impl->scannable;
}

/**
 * @brief Whether this is a directed advertisement.
 * @return True if directed.
 */
bool BLEAdvertisedDevice::isDirected() const {
  return _impl && _impl->directed;
}

/**
 * @brief Whether the PDU is legacy (not extended advertising).
 * @return True for legacy PDUs; defaults to true if the handle is invalid.
 */
bool BLEAdvertisedDevice::isLegacyAdvertisement() const {
  return _impl ? _impl->legacy : true;
}

/**
 * @brief Primary advertising PHY for extended advertising events.
 * @return The primary @c BLEPhy, or @c PHY_1M.
 */
BLEPhy BLEAdvertisedDevice::getPrimaryPhy() const {
  return _impl ? _impl->primaryPhy : BLEPhy::PHY_1M;
}

/**
 * @brief Secondary (aux) advertising PHY.
 * @return The secondary @c BLEPhy, or @c PHY_1M.
 */
BLEPhy BLEAdvertisedDevice::getSecondaryPhy() const {
  return _impl ? _impl->secondaryPhy : BLEPhy::PHY_1M;
}

/**
 * @brief Advertising set identifier (BLE 5).
 * @return SID (0–15), or 0xFF if unknown.
 */
uint8_t BLEAdvertisedDevice::getAdvSID() const {
  return _impl ? _impl->sid : 0xFF;
}

/**
 * @brief Periodic advertising interval if present.
 * @return Interval in 1.25 ms units, or 0.
 */
uint16_t BLEAdvertisedDevice::getPeriodicInterval() const {
  return _impl ? _impl->periodicInterval : 0;
}

/**
 * @brief Detect the beacon frame type by inspecting manufacturer and service data.
 * @return IBeacon if Apple 0x004C + subtype 0x0215 is found; EddystoneUUID/URL/TLM
 *         if service data UUID 0xFEAA matches; Unknown otherwise.
 */
BLEAdvertisedDevice::FrameType BLEAdvertisedDevice::getFrameType() const {
  BLE_CHECK_IMPL(Unknown);

  // iBeacon: Apple company ID (0x004C) + subtype 0x02 0x15 + 21-byte body
  if (impl.hasMfgData && impl.mfgData.size() >= 25 && impl.mfgData[0] == 0x4C && impl.mfgData[1] == 0x00 && impl.mfgData[2] == 0x02
      && impl.mfgData[3] == 0x15) {
    return IBeacon;
  }

  // Eddystone: service data with UUID 0xFEAA, first byte = frame type
  const BLEUUID eddy((uint16_t)0xFEAA);
  for (const auto &sd : impl.serviceData) {
    if (!(sd.uuid == eddy) || sd.data.empty()) {
      continue;
    }
    switch (sd.data[0]) {
      case 0x00: return EddystoneUUID;
      case 0x10: return EddystoneURL;
      case 0x20: return EddystoneTLM;
      default:   break;
    }
  }

  return Unknown;
}

/**
 * @brief Get a human-readable summary including name, address, and RSSI.
 * @return Formatted String, or "BLEAdvertisedDevice(empty)" if invalid.
 */
String BLEAdvertisedDevice::toString() const {
  BLE_CHECK_IMPL("BLEAdvertisedDevice(empty)");
  String s = "Name: " + impl.name + ", Addr: " + impl.address.toString();
  s += ", RSSI: " + String(impl.rssi);
  return s;
}

// --------------------------------------------------------------------------
// BLEScanResults
// --------------------------------------------------------------------------

/**
 * @brief Merge a SCAN_RSP payload into this device's existing ADV_IND data.
 * @param data Pointer to the scan response payload bytes.
 * @param len Length of the scan response data in bytes.
 * @param rssi RSSI of the scan response.
 * @note Concatenates the ADV_IND and SCAN_RSP payloads, clears and re-parses
 *       all AD structures, then deduplicates service UUIDs and service data
 *       entries (keeping the SCAN_RSP value for duplicate service data UUIDs).
 */
void BLEAdvertisedDevice::mergeScanResponse(const uint8_t *data, size_t len, int8_t rssi) {
  BLE_CHECK_IMPL();
  if (data && len > 0) {
    std::vector<uint8_t> combined(std::move(impl.payload));
    combined.insert(combined.end(), data, data + len);
    impl.clearParsedFields();
    impl.parsePayload(combined.data(), combined.size());

    // Deduplicate UUIDs that may appear in both ADV_IND and SCAN_RSP.
    auto &uuids = impl.serviceUUIDs;
    for (size_t i = 0; i < uuids.size(); ++i) {
      for (size_t j = i + 1; j < uuids.size();) {
        if (uuids[j] == uuids[i]) {
          uuids.erase(uuids.begin() + j);
        } else {
          ++j;
        }
      }
    }

    // Deduplicate service data entries; keep the last (SCAN_RSP) value.
    auto &sd = impl.serviceData;
    for (size_t i = 0; i < sd.size(); ++i) {
      for (size_t j = i + 1; j < sd.size();) {
        if (sd[j].uuid == sd[i].uuid) {
          sd[i].data = std::move(sd[j].data);
          sd.erase(sd.begin() + j);
        } else {
          ++j;
        }
      }
    }
  }
  impl.rssi = rssi;
  impl.hasRSSI = true;
  impl.scannable = true;
}

/**
 * @brief Add a device to the results, replacing any existing entry with the same address.
 * @param device The advertised device to insert or update.
 * @note When the existing entry already has a device name (merged from a prior
 *       scan response) and the incoming device has no name (a duplicate primary
 *       ADV packet received when filterDuplicates is off), the existing entry is
 *       preserved to avoid losing the scan-response data.
 */
void BLEScanResults::appendOrReplace(const BLEAdvertisedDevice &device) {
  for (auto &existing : _devices) {
    if (existing.getAddress() == device.getAddress()) {
      // Preserve an existing entry that has a name from a merged scan response
      // when the incoming duplicate primary ADV carries no name.  Overwriting
      // would silently erase the name, causing getName() to return "" even
      // though the device is still advertising.
      if (existing.haveName() && !device.haveName()) {
        return;
      }
      existing = device;
      return;
    }
  }
  _devices.push_back(device);
}

/**
 * @brief Get the number of devices in the scan result set.
 * @return Number of @c BLEAdvertisedDevice entries.
 */
size_t BLEScanResults::getCount() const {
  return _devices.size();
}

/**
 * @brief Get an advertised device by index.
 * @param index Zero-based index.
 * @return The device, or a default-constructed (invalid) BLEAdvertisedDevice if out of range.
 */
BLEAdvertisedDevice BLEScanResults::getDevice(size_t index) const {
  return (index < _devices.size()) ? _devices[index] : BLEAdvertisedDevice();
}

/**
 * @brief Log all scan results via log_i for debugging.
 */
void BLEScanResults::dump() const {
  for (size_t i = 0; i < _devices.size(); i++) {
    log_i("[%d] %s", i, _devices[i].toString().c_str());
  }
}

/**
 * @brief Iterator to the first scan result (range-based @c for support).
 * @return Pointer to the first element, or a non-dereferenceable pointer if empty.
 */
const BLEAdvertisedDevice *BLEScanResults::begin() const {
  return _devices.data();
}

/**
 * @brief Past-the-end iterator.
 * @return One past the last element.
 */
const BLEAdvertisedDevice *BLEScanResults::end() const {
  return _devices.data() + _devices.size();
}

#endif /* BLE_ENABLED */
