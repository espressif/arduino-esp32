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

/**
 * @file BLEAdvertising.cpp
 * @brief Stack-agnostic `BLEAdvertising` helpers: service-UUID staging,
 *        legacy AD payload assembly, and feature-disabled / BLE 5 TX stubs.
 * @note Interval conversion, on-air start/stop, and BLE 5 extended/periodic
 *       advertising TX live in the active backend (`BLEAdvertising.{nimble,bluedroid}.cpp`).
 */

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "core/BLEBackend.h"
#include "core/BLEImplHelpers.h"
#include "core/BLEMutex.h"
#include "advertising/BLEAdvertising.h"
#include "advertising/BLEAdvertisingData.h"
#include "advertising/BLEAdvScanHelpers.h"
#include "BTAddress.h"
#include "esp32-hal-log.h"

#include <algorithm>

// ==========================================================================
// 1. Handle construction, service-UUID list & state queries
// ==========================================================================

BLEAdvertising::BLEAdvertising() : _impl(nullptr) {}

BLEAdvertising::operator bool() const {
  return _impl != nullptr;
}

void BLEAdvertising::addServiceUUID(const BLEUUID &uuid) {
  BLE_CHECK_IMPL();
  log_d("Advertising: addServiceUUID %s", uuid.toString().c_str());
  BLELockGuard lock(impl.mtx);
  impl.serviceUUIDs.push_back(uuid);
}

void BLEAdvertising::removeServiceUUID(const BLEUUID &uuid) {
  BLE_CHECK_IMPL();
  log_d("Advertising: removeServiceUUID %s", uuid.toString().c_str());
  BLELockGuard lock(impl.mtx);
  auto &v = impl.serviceUUIDs;
  v.erase(std::remove(v.begin(), v.end(), uuid), v.end());
}

void BLEAdvertising::clearServiceUUIDs() {
  BLE_CHECK_IMPL();
  log_d("Advertising: clearServiceUUIDs");
  BLELockGuard lock(impl.mtx);
  impl.serviceUUIDs.clear();
}

bool BLEAdvertising::isAdvertising() const {
  if (!_impl) {
    return false;
  }
  if (_impl->isAdvertising.load()) {
    return true;
  }
#if BLE5_SUPPORTED
  for (uint8_t i = 0; i < BLEAdvertising::Impl::kLegacyInstance; i++) {
    if (_impl->extInstances[i].isAdvertising.load()) {
      return true;
    }
  }
#endif
  return false;
}

void BLEAdvertising::setDirectedAddress(const BTAddress &address) {
  BLE_CHECK_IMPL();
  impl.directedPeer = address;
}

BTAddress BLEAdvertising::getDirectedAddress() const {
  if (!_impl) {
    return BTAddress();
  }
  return _impl->directedPeer;
}

// ==========================================================================
// 2. Legacy advertising parameters (BLE_ADVERTISING_SUPPORTED)
// ==========================================================================

#if BLE_ADVERTISING_SUPPORTED

void BLEAdvertising::setType(BLEAdvType type) {
  BLE_CHECK_IMPL();
  impl.advType = type;
}

void BLEAdvertising::setInterval(uint16_t minMs, uint16_t maxMs) {
  BLE_CHECK_IMPL();
  impl.minInterval = BLEAdvInternal::msToUnits0625(minMs);
  impl.maxInterval = BLEAdvInternal::msToUnits0625(maxMs);
}

void BLEAdvertising::setMinPreferred(uint16_t val) {
  BLE_CHECK_IMPL();
  impl.minPreferred = val;
}

void BLEAdvertising::setMaxPreferred(uint16_t val) {
  BLE_CHECK_IMPL();
  impl.maxPreferred = val;
}

void BLEAdvertising::setTxPower(bool include) {
  BLE_CHECK_IMPL();
  impl.includeTxPower = include;
}

void BLEAdvertising::setAppearance(uint16_t appearance) {
  BLE_CHECK_IMPL();
  impl.appearance = appearance;
}

void BLEAdvertising::prepareBackendStart() {
  if (_impl) {
    _impl->nativeAdmission.reopen();
  }
}

#endif /* BLE_ADVERTISING_SUPPORTED */

// ==========================================================================
// 3. Stack-agnostic legacy AD payload builder (BLEAdvertisingData.h)
// ==========================================================================

// Legacy AD placement follows CSS Part A §1.2/§1.3: LE-only Flags in the
// primary AD, and the Local Name in the scan response (leaving the primary AD
// free for service UUIDs) unless scan response is disabled.
BLEAdvertisementData BLEAdvDataBuilder::buildLegacyAdvData(
  const String &name, const std::vector<BLEUUID> &serviceUUIDs, uint16_t appearance, bool includeTxPower, int8_t txPower, bool nameInScanResponse,
  uint16_t minPreferred, uint16_t maxPreferred
) {
  BLEAdvertisementData data;
  // Flags (0x01): LE General Discoverable + BR/EDR Not Supported for LE-only devices.
  data.setFlags(BLEAdvFlag::GeneralDisc | BLEAdvFlag::BrEdrNotSupported);
  if (!nameInScanResponse && name.length() > 0) {
    data.setName(name, true);
  }
  if (includeTxPower) {
    data.setTxPower(txPower);
  }
  if (appearance > 0) {
    data.setAppearance(appearance);
  }
  // Slave Connection Interval Range (0x12), CSS Part A §1.9 — emit when either
  // bound was set via setMinPreferred / setMaxPreferred; a zero side copies the other.
  if (minPreferred != 0 || maxPreferred != 0) {
    const uint16_t minInt = minPreferred != 0 ? minPreferred : maxPreferred;
    const uint16_t maxInt = maxPreferred != 0 ? maxPreferred : minPreferred;
    data.setPreferredParams(minInt, maxInt);
  }
  for (const auto &uuid : serviceUUIDs) {
    data.addServiceUUID(uuid);
  }
  return data;
}

BLEAdvertisementData BLEAdvDataBuilder::buildLegacyScanRespData(const String &name) {
  BLEAdvertisementData data;
  if (name.length() > 0) {
    data.setName(name, true);
  }
  return data;
}

// ==========================================================================
// 4. Feature-disabled & capability stubs
// ==========================================================================

#if !BLE_ADVERTISING_SUPPORTED

void BLEAdvertising::setName(const String &) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setScanResponse(bool) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setType(BLEAdvType) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setInterval(uint16_t, uint16_t) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setMinPreferred(uint16_t) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setMaxPreferred(uint16_t) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setTxPower(bool) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setAppearance(uint16_t) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setScanFilter(bool, bool) {
  log_w("Advertising not supported");
}

void BLEAdvertising::reset() {
  log_w("Advertising not supported");
}

void BLEAdvertising::prepareBackendStart() {}

void BLEAdvertising::beginBackendShutdown() {}

BTStatus BLEAdvertising::stopBackend() {
  return BTStatus::NotSupported;
}

void BLEAdvertising::setAdvertisementData(const BLEAdvertisementData &) {
  log_w("Advertising not supported");
}

void BLEAdvertising::setScanResponseData(const BLEAdvertisementData &) {
  log_w("Advertising not supported");
}

BTStatus BLEAdvertising::start(uint32_t) {
  log_w("Advertising not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEAdvertising::stop() {
  log_w("Advertising not supported");
  return BTStatus::NotSupported;
}

void BLEAdvertising::onComplete(CompleteHandler) {
  log_w("Advertising not supported");
}

void BLEAdvertising::resetCallbacks() {
  log_w("Advertising not supported");
}

#endif /* !BLE_ADVERTISING_SUPPORTED */

// BLE 5 extended / periodic advertising TX fallback stubs: compiled whenever
// the active backend does not implement extended/periodic TX (including
// Bluedroid builds on BLE 5 silicon where BLE5_SUPPORTED == 1).
#if !BLE5_ADV_TX_SUPPORTED || !BLE_PERIODIC_ADV_TX_SUPPORTED
namespace {

BTStatus advTxNotSupported(const char *fn) {
  log_w("%s not supported in this build (no BLE 5.0 extended/periodic advertising TX backend)", fn);
  return BTStatus::NotSupported;
}

}  // namespace
#endif

#if !BLE5_ADV_TX_SUPPORTED

void BLEAdvertising::setExtType(uint8_t, BLEAdvType) {
  advTxNotSupported(__func__);
}
void BLEAdvertising::setExtPhy(uint8_t, BLEPhy, BLEPhy) {
  advTxNotSupported(__func__);
}
void BLEAdvertising::setExtTxPower(uint8_t, int8_t) {
  advTxNotSupported(__func__);
}
void BLEAdvertising::setExtInterval(uint8_t, uint16_t, uint16_t) {
  advTxNotSupported(__func__);
}
void BLEAdvertising::setExtChannelMap(uint8_t, uint8_t) {
  advTxNotSupported(__func__);
}
void BLEAdvertising::setExtSID(uint8_t, uint8_t) {
  advTxNotSupported(__func__);
}
void BLEAdvertising::setExtAnonymous(uint8_t, bool) {
  advTxNotSupported(__func__);
}
void BLEAdvertising::setExtIncludeTxPower(uint8_t, bool) {
  advTxNotSupported(__func__);
}
void BLEAdvertising::setExtScanReqNotify(uint8_t, bool) {
  advTxNotSupported(__func__);
}
BTStatus BLEAdvertising::setExtAdvertisementData(uint8_t, const BLEAdvertisementData &) {
  return advTxNotSupported(__func__);
}
BTStatus BLEAdvertising::setExtScanResponseData(uint8_t, const BLEAdvertisementData &) {
  return advTxNotSupported(__func__);
}
BTStatus BLEAdvertising::setExtInstanceAddress(uint8_t, const BTAddress &) {
  return advTxNotSupported(__func__);
}
BTStatus BLEAdvertising::startExtended(uint8_t, uint32_t, uint8_t) {
  return advTxNotSupported(__func__);
}
BTStatus BLEAdvertising::stopExtended(uint8_t) {
  return advTxNotSupported(__func__);
}
BTStatus BLEAdvertising::removeExtended(uint8_t) {
  return advTxNotSupported(__func__);
}
BTStatus BLEAdvertising::clearExtended() {
  return advTxNotSupported(__func__);
}

#endif /* !BLE5_ADV_TX_SUPPORTED */

#if !BLE_PERIODIC_ADV_TX_SUPPORTED

void BLEAdvertising::setPeriodicAdvInterval(uint8_t, uint16_t, uint16_t) {
  advTxNotSupported(__func__);
}
void BLEAdvertising::setPeriodicAdvTxPower(uint8_t, bool) {
  advTxNotSupported(__func__);
}
BTStatus BLEAdvertising::setPeriodicAdvData(uint8_t, const BLEAdvertisementData &) {
  return advTxNotSupported(__func__);
}
BTStatus BLEAdvertising::startPeriodicAdv(uint8_t) {
  return advTxNotSupported(__func__);
}
BTStatus BLEAdvertising::stopPeriodicAdv(uint8_t) {
  return advTxNotSupported(__func__);
}

#endif /* !BLE_PERIODIC_ADV_TX_SUPPORTED */

#endif /* BLE_ENABLED */
