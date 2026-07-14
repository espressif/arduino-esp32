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

#include "impl/BLEBackend.h"
#include "impl/common/BLEImplHelpers.h"
#include "impl/common/BLEMutex.h"
#include "impl/common/BLEAdvertisingData.h"
#include "esp32-hal-log.h"

#include <algorithm>

/**
 * @file
 * @brief Stack-agnostic @ref BLEAdvertising helpers (service-UUID list + isAdvertising).
 * @note Interval conversion, AD assembly, on-air start/stop, and all BLE5
 *       extended/periodic advertising live in the active backend; keep total AD
 *       length within controller limits (31 bytes per legacy AD payload segment
 *       unless using extended adv).
 */

BLEAdvertising::BLEAdvertising() : _impl(nullptr) {}

BLEAdvertising::operator bool() const {
  return _impl != nullptr;
}

// --------------------------------------------------------------------------
// Service UUID management (guarded by impl.mtx)
// --------------------------------------------------------------------------

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

// --------------------------------------------------------------------------
// isAdvertising
// --------------------------------------------------------------------------

bool BLEAdvertising::isAdvertising() const {
  return _impl && _impl->isAdvertising;
}

// --------------------------------------------------------------------------
// Raw AD payload builder (stack-agnostic) -- see BLEAdvertisingData.h
//
// Legacy AD placement follows CSS Part A §1.2/§1.3: LE-only Flags in the
// primary AD, and the Local Name in the scan response (leaving the primary AD
// free for service UUIDs) unless scan response is disabled.
// --------------------------------------------------------------------------

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

// --------------------------------------------------------------------------
// BLE5 extended / periodic advertising (TX) -- shared NotSupported fallback
//
// The real transmit implementation is backend-specific and lives under a
// capability guard in the active backend (NimBLE: impl/nimble/NimBLEAdvertising.cpp
// under BLE5_ADV_TX_SUPPORTED / BLE_PERIODIC_ADV_SUPPORTED). "NotSupported" is a
// backend-agnostic default, not a Bluedroid trait, so the fallback is defined
// once here rather than copied into each backend. It compiles only when no active
// backend implements the feature -- including a Bluedroid build on BLE5-capable
// silicon (BLE5_SUPPORTED == 1 but BLE5_ADV_TX_SUPPORTED == 0). Exactly one
// definition of each method exists in every build configuration.
// --------------------------------------------------------------------------

#if !BLE5_ADV_TX_SUPPORTED || !BLE_PERIODIC_ADV_TX_SUPPORTED
namespace {
// Single fallback body for every BLE5 TX entry point the active backend does
// not implement. Logs the caller's name and yields NotSupported for the
// BTStatus-returning entry points; void entry points discard the result.
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
