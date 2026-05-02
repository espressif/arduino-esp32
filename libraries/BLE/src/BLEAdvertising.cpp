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
// Extended / Periodic advertising stubs (BLE5 -- not yet supported)
// --------------------------------------------------------------------------

/**
 * @brief Configure an extended advertising instance (BLE5 stub).
 * @param config Configuration parameters (ignored).
 * @return BTStatus::NotSupported always; BLE5 extended advertising is not yet implemented.
 */
BTStatus BLEAdvertising::configureExtended(const ExtAdvConfig &) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

/**
 * @brief Set advertisement data for an extended instance (BLE5 stub).
 * @param instance Advertising instance index (ignored).
 * @param data Advertisement payload (ignored).
 * @return BTStatus::NotSupported always; BLE5 extended advertising is not yet implemented.
 */
BTStatus BLEAdvertising::setExtAdvertisementData(uint8_t, const BLEAdvertisementData &) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

/**
 * @brief Set scan-response data for an extended instance (BLE5 stub).
 * @param instance Advertising instance index (ignored).
 * @param data Scan-response payload (ignored).
 * @return BTStatus::NotSupported always; BLE5 extended advertising is not yet implemented.
 */
BTStatus BLEAdvertising::setExtScanResponseData(uint8_t, const BLEAdvertisementData &) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

/**
 * @brief Set a random address for an extended advertising instance (BLE5 stub).
 * @param instance Advertising instance index (ignored).
 * @param addr Address to assign (ignored).
 * @return BTStatus::NotSupported always; BLE5 extended advertising is not yet implemented.
 */
BTStatus BLEAdvertising::setExtInstanceAddress(uint8_t, const BTAddress &) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

/**
 * @brief Start an extended advertising instance (BLE5 stub).
 * @param instance Advertising instance index (ignored).
 * @param durationMs Duration in milliseconds (ignored).
 * @param maxEvents Maximum advertising events (ignored).
 * @return BTStatus::NotSupported always; BLE5 extended advertising is not yet implemented.
 */
BTStatus BLEAdvertising::startExtended(uint8_t, uint32_t, uint8_t) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

/**
 * @brief Stop an extended advertising instance (BLE5 stub).
 * @param instance Advertising instance index (ignored).
 * @return BTStatus::NotSupported always; BLE5 extended advertising is not yet implemented.
 */
BTStatus BLEAdvertising::stopExtended(uint8_t) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

/**
 * @brief Remove an extended advertising instance (BLE5 stub).
 * @param instance Advertising instance index (ignored).
 * @return BTStatus::NotSupported always; BLE5 extended advertising is not yet implemented.
 */
BTStatus BLEAdvertising::removeExtended(uint8_t) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

/**
 * @brief Remove all extended advertising instances (BLE5 stub).
 * @return BTStatus::NotSupported always; BLE5 extended advertising is not yet implemented.
 */
BTStatus BLEAdvertising::clearExtended() {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

/**
 * @brief Configure periodic advertising on an extended instance (BLE5 stub).
 * @param config Periodic advertising parameters (ignored).
 * @return BTStatus::NotSupported always; BLE5 periodic advertising is not yet implemented.
 */
BTStatus BLEAdvertising::configurePeriodicAdv(const PeriodicAdvConfig &) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

/**
 * @brief Set the payload for periodic advertising (BLE5 stub).
 * @param instance Extended advertising instance index (ignored).
 * @param data Periodic advertising payload (ignored).
 * @return BTStatus::NotSupported always; BLE5 periodic advertising is not yet implemented.
 */
BTStatus BLEAdvertising::setPeriodicAdvData(uint8_t, const BLEAdvertisementData &) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

/**
 * @brief Start periodic advertising on an extended instance (BLE5 stub).
 * @param instance Extended advertising instance index (ignored).
 * @return BTStatus::NotSupported always; BLE5 periodic advertising is not yet implemented.
 */
BTStatus BLEAdvertising::startPeriodicAdv(uint8_t) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

/**
 * @brief Stop periodic advertising on an extended instance (BLE5 stub).
 * @param instance Extended advertising instance index (ignored).
 * @return BTStatus::NotSupported always; BLE5 periodic advertising is not yet implemented.
 */
BTStatus BLEAdvertising::stopPeriodicAdv(uint8_t) {
  log_w("%s not supported", __func__);
  return BTStatus::NotSupported;
}

#endif /* BLE_ENABLED */
