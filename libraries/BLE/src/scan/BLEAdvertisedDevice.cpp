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

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "scan/BLEAdvertisedDeviceImpl.h"
#include "advertising/BLEBeacon.h"
#include "advertising/BLEEddystone.h"
#include "core/BLEImplHelpers.h"
#include "esp32-hal-log.h"
#include "HEXBuilder.h"

// The shared Impl helpers (parsePayload / clearParsedFields) live in the
// implementation layer: scan/BLEAdvertisedDeviceImpl.cpp.

// --------------------------------------------------------------------------
// BLEAdvertisedDevice public API
// --------------------------------------------------------------------------

BLEAdvertisedDevice::BLEAdvertisedDevice() : _impl(nullptr) {}

BLEAdvertisedDevice::operator bool() const {
  return _impl != nullptr;
}

BTAddress BLEAdvertisedDevice::getAddress() const {
  return _impl ? _impl->address : BTAddress();
}

BTAddress::Type BLEAdvertisedDevice::getAddressType() const {
  return _impl ? _impl->addrType : BTAddress::Type::Public;
}

String BLEAdvertisedDevice::getName() const {
  return _impl ? _impl->name : "";
}

int8_t BLEAdvertisedDevice::getRSSI() const {
  return _impl ? _impl->rssi : -128;
}

int8_t BLEAdvertisedDevice::getTXPower() const {
  return _impl ? _impl->txPower : -128;
}

uint16_t BLEAdvertisedDevice::getAppearance() const {
  return _impl ? _impl->appearance : 0;
}

uint16_t BLEAdvertisedDevice::getMinPreferred() const {
  return _impl ? _impl->minPreferred : 0;
}

uint16_t BLEAdvertisedDevice::getMaxPreferred() const {
  return _impl ? _impl->maxPreferred : 0;
}

BLEAdvType BLEAdvertisedDevice::getAdvType() const {
  return _impl ? _impl->advType : BLEAdvType::ConnectableScannable;
}

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

String BLEAdvertisedDevice::getManufacturerDataString() const {
  BLE_CHECK_IMPL("");
  return HEXBuilder::bytes2hex(impl.mfgData.data(), impl.mfgData.size());
}

uint16_t BLEAdvertisedDevice::getManufacturerCompanyId() const {
  if (!_impl || _impl->mfgData.size() < 2) {
    return 0;
  }
  return _impl->mfgData[0] | (_impl->mfgData[1] << 8);
}

size_t BLEAdvertisedDevice::getServiceUUIDCount() const {
  return _impl ? _impl->serviceUUIDs.size() : 0;
}

BLEUUID BLEAdvertisedDevice::getServiceUUID(size_t index) const {
  if (!_impl || index >= _impl->serviceUUIDs.size()) {
    return BLEUUID();
  }
  return _impl->serviceUUIDs[index];
}

bool BLEAdvertisedDevice::haveServiceUUID() const {
  return _impl && !_impl->serviceUUIDs.empty();
}

bool BLEAdvertisedDevice::isAdvertisingService(const BLEUUID &uuid) const {
  // Width-agnostic: BLEUUID::operator== compares the full 128-bit value.
  BLE_CHECK_IMPL(false);
  for (const auto &u : impl.serviceUUIDs) {
    if (u == uuid) {
      return true;
    }
  }
  return false;
}

size_t BLEAdvertisedDevice::getServiceDataCount() const {
  return _impl ? _impl->serviceData.size() : 0;
}

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

String BLEAdvertisedDevice::getServiceDataString(size_t index) const {
  if (!_impl || index >= _impl->serviceData.size()) {
    return "";
  }
  const auto &bytes = _impl->serviceData[index].data;
  return HEXBuilder::bytes2hex(bytes.data(), bytes.size());
}

BLEUUID BLEAdvertisedDevice::getServiceDataUUID(size_t index) const {
  if (!_impl || index >= _impl->serviceData.size()) {
    return BLEUUID();
  }
  return _impl->serviceData[index].uuid;
}

bool BLEAdvertisedDevice::haveServiceData() const {
  return _impl && !_impl->serviceData.empty();
}

const uint8_t *BLEAdvertisedDevice::getPayload() const {
  if (!_impl || _impl->payload.empty()) {
    return nullptr;
  }
  return _impl->payload.data();
}

size_t BLEAdvertisedDevice::getPayloadLength() const {
  return _impl ? _impl->payload.size() : 0;
}

bool BLEAdvertisedDevice::haveName() const {
  return _impl && _impl->hasName;
}

bool BLEAdvertisedDevice::haveRSSI() const {
  return _impl && _impl->hasRSSI;
}

bool BLEAdvertisedDevice::haveTXPower() const {
  return _impl && _impl->hasTXPower;
}

bool BLEAdvertisedDevice::haveAppearance() const {
  return _impl && _impl->hasAppearance;
}

bool BLEAdvertisedDevice::havePreferredParams() const {
  return _impl && _impl->hasPreferredParams;
}

bool BLEAdvertisedDevice::haveManufacturerData() const {
  return _impl && _impl->hasMfgData;
}

bool BLEAdvertisedDevice::isConnectable() const {
  return _impl && _impl->connectable;
}

bool BLEAdvertisedDevice::isScannable() const {
  return _impl && _impl->scannable;
}

bool BLEAdvertisedDevice::isDirected() const {
  return _impl && _impl->directed;
}

bool BLEAdvertisedDevice::isLegacyAdvertisement() const {
  // Defaults to legacy (true) when the handle is invalid.
  return _impl ? _impl->legacy : true;
}

BLEPhy BLEAdvertisedDevice::getPrimaryPhy() const {
  return _impl ? _impl->primaryPhy : BLEPhy::PHY_1M;
}

BLEPhy BLEAdvertisedDevice::getSecondaryPhy() const {
  return _impl ? _impl->secondaryPhy : BLEPhy::PHY_1M;
}

uint8_t BLEAdvertisedDevice::getAdvSID() const {
  return _impl ? _impl->sid : 0xFF;
}

uint16_t BLEAdvertisedDevice::getPeriodicInterval() const {
  return _impl ? _impl->periodicInterval : 0;
}

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

bool BLEAdvertisedDevice::getBeacon(BLEBeacon &beacon) const {
  BLE_CHECK_IMPL(false);
  // iBeacon requires the Apple company ID (0x004C, little-endian 4C 00) before
  // the 0x02 0x15 subtype; without it any vendor's 0x02 0x15 payload misparses.
  if (!impl.hasMfgData || impl.mfgData.size() < 25 || impl.mfgData[0] != 0x4C || impl.mfgData[1] != 0x00 || impl.mfgData[2] != 0x02
      || impl.mfgData[3] != 0x15) {
    return false;
  }
  beacon.setFromPayload(impl.mfgData.data(), impl.mfgData.size());
  return true;
}

BLEBeacon BLEAdvertisedDevice::getBeacon() const {
  BLEBeacon beacon;
  (void)getBeacon(beacon);
  return beacon;
}

bool BLEAdvertisedDevice::getEddystoneURL(BLEEddystoneURL &url) const {
  BLE_CHECK_IMPL(false);
  const BLEUUID eddy = BLEEddystoneURL::serviceUUID();
  for (const auto &sd : impl.serviceData) {
    if (sd.uuid == eddy && sd.data.size() >= 3 && sd.data[0] == 0x10) {
      url.setFromServiceData(sd.data.data(), sd.data.size());
      return true;
    }
  }
  return false;
}

BLEEddystoneURL BLEAdvertisedDevice::getEddystoneURL() const {
  BLEEddystoneURL url;
  (void)getEddystoneURL(url);
  return url;
}

bool BLEAdvertisedDevice::getEddystoneTLM(BLEEddystoneTLM &tlm) const {
  BLE_CHECK_IMPL(false);
  const BLEUUID eddy = BLEEddystoneTLM::serviceUUID();
  for (const auto &sd : impl.serviceData) {
    if (sd.uuid == eddy && sd.data.size() >= 14 && sd.data[0] == 0x20) {
      tlm.setFromServiceData(sd.data.data(), sd.data.size());
      return true;
    }
  }
  return false;
}

BLEEddystoneTLM BLEAdvertisedDevice::getEddystoneTLM() const {
  BLEEddystoneTLM tlm;
  (void)getEddystoneTLM(tlm);
  return tlm;
}

String BLEAdvertisedDevice::toString() const {
  BLE_CHECK_IMPL("BLEAdvertisedDevice(empty)");
  String s = "Name: " + impl.name + ", Addr: " + impl.address.toString();
  s += ", RSSI: " + String(impl.rssi);
  return s;
}

// --------------------------------------------------------------------------
// Scan-response merge (result assembly lives in BLEScan::Results, BLEScan.cpp)
// --------------------------------------------------------------------------

void BLEAdvertisedDevice::mergeScanResponse(const uint8_t *data, size_t len, int8_t rssi) {
  BLE_CHECK_IMPL();
  if (data && len > 0) {
    const size_t baseLen = (impl.advPayloadLen <= impl.payload.size()) ? impl.advPayloadLen : impl.payload.size();
    std::vector<uint8_t> combined(impl.payload.begin(), impl.payload.begin() + baseLen);
    combined.insert(combined.end(), data, data + len);
    impl.clearParsedFields();
    impl.parsePayload(combined.data(), combined.size());
    impl.advPayloadLen = baseLen;

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

#endif /* BLE_ENABLED */
