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

#include "impl/BLEAdvertisingBackend.h"
#include "impl/BLEImplHelpers.h"
#include "impl/BLEMutex.h"
#include "esp32-hal-log.h"

#include <algorithm>

/**
 * @file
 * @brief Common @ref BLEAdvertising helpers (UUID list, flags, stub BLE5 entry points).
 * @note Interval conversion, AD assembly, and on-air start/stop are implemented
 *       in the active backend; keep total AD length within controller limits
 *       (31 bytes per legacy AD payload segment unless using extended adv).
 */

// --------------------------------------------------------------------------
// BLEAdvertising common API (stack-agnostic)
// --------------------------------------------------------------------------

/**
 * @brief Construct a BLEAdvertising handle with no backend.
 *
 * @note The handle is invalid until initialized by BLEClass. Use operator bool() to check.
 */
BLEAdvertising::BLEAdvertising() : _impl(nullptr) {}

/**
 * @brief Check whether this handle references a valid advertising backend.
 * @return true if the implementation pointer is non-null, false otherwise.
 */
BLEAdvertising::operator bool() const {
  return _impl != nullptr;
}

// --------------------------------------------------------------------------
// Service UUID management
// --------------------------------------------------------------------------

/**
 * @brief Append a service UUID to the advertisement payload.
 * @param uuid Service UUID to advertise.
 * @note Duplicates are not checked; the same UUID can be added multiple times.
 */
void BLEAdvertising::addServiceUUID(const BLEUUID &uuid) {
  BLE_CHECK_IMPL();
  log_d("Advertising: addServiceUUID %s", uuid.toString().c_str());
  BLELockGuard lock(impl.mtx);
  impl.serviceUUIDs.push_back(uuid);
}

/**
 * @brief Remove a service UUID from the advertisement payload.
 * @param uuid Service UUID to remove.
 * @note Uses the erase-remove idiom; all occurrences of @p uuid are removed in O(n) time.
 */
void BLEAdvertising::removeServiceUUID(const BLEUUID &uuid) {
  BLE_CHECK_IMPL();
  log_d("Advertising: removeServiceUUID %s", uuid.toString().c_str());
  BLELockGuard lock(impl.mtx);
  auto &v = impl.serviceUUIDs;
  v.erase(std::remove(v.begin(), v.end(), uuid), v.end());
}

/**
 * @brief Remove all service UUIDs from the advertisement payload.
 */
void BLEAdvertising::clearServiceUUIDs() {
  BLE_CHECK_IMPL();
  log_d("Advertising: clearServiceUUIDs");
  BLELockGuard lock(impl.mtx);
  impl.serviceUUIDs.clear();
}

// --------------------------------------------------------------------------
// isAdvertising
// --------------------------------------------------------------------------

/**
 * @brief Check whether legacy advertising is currently active.
 * @return true if advertising is running, false if stopped or handle is invalid.
 * @note Null-safe: returns false when _impl is nullptr.
 */
bool BLEAdvertising::isAdvertising() const {
  return _impl && _impl->advertising;
}

// --------------------------------------------------------------------------
// Extended advertising setters (BLE5 -- not yet supported)
// --------------------------------------------------------------------------

void BLEAdvertising::setExtType(uint8_t, BLEAdvType) {
  log_w("%s not supported", __func__);
}

void BLEAdvertising::setExtPhy(uint8_t, BLEPhy, BLEPhy) {
  log_w("%s not supported", __func__);
}

void BLEAdvertising::setExtTxPower(uint8_t, int8_t) {
  log_w("%s not supported", __func__);
}

void BLEAdvertising::setExtInterval(uint8_t, uint16_t, uint16_t) {
  log_w("%s not supported", __func__);
}

void BLEAdvertising::setExtChannelMap(uint8_t, uint8_t) {
  log_w("%s not supported", __func__);
}

void BLEAdvertising::setExtSID(uint8_t, uint8_t) {
  log_w("%s not supported", __func__);
}

void BLEAdvertising::setExtAnonymous(uint8_t, bool) {
  log_w("%s not supported", __func__);
}

void BLEAdvertising::setExtIncludeTxPower(uint8_t, bool) {
  log_w("%s not supported", __func__);
}

void BLEAdvertising::setExtScanReqNotify(uint8_t, bool) {
  log_w("%s not supported", __func__);
}

BTStatus BLEAdvertising::setExtAdvertisementData(uint8_t, const BLEAdvertisementData &) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

BTStatus BLEAdvertising::setExtScanResponseData(uint8_t, const BLEAdvertisementData &) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

BTStatus BLEAdvertising::setExtInstanceAddress(uint8_t, const BTAddress &) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

BTStatus BLEAdvertising::startExtended(uint8_t, uint32_t, uint8_t) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

BTStatus BLEAdvertising::stopExtended(uint8_t) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

BTStatus BLEAdvertising::removeExtended(uint8_t) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

BTStatus BLEAdvertising::clearExtended() {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

// --------------------------------------------------------------------------
// Periodic advertising setters (BLE5 -- not yet supported)
// --------------------------------------------------------------------------

void BLEAdvertising::setPeriodicAdvInterval(uint8_t, uint16_t, uint16_t) {
  log_w("%s not supported", __func__);
}

void BLEAdvertising::setPeriodicAdvTxPower(uint8_t, bool) {
  log_w("%s not supported", __func__);
}

BTStatus BLEAdvertising::setPeriodicAdvData(uint8_t, const BLEAdvertisementData &) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

BTStatus BLEAdvertising::startPeriodicAdv(uint8_t) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

BTStatus BLEAdvertising::stopPeriodicAdv(uint8_t) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

#endif /* BLE_ENABLED */
