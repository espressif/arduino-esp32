/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
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

#include "BLEScan.h"
#include "impl/BLEImplHelpers.h"
#include "impl/BLEScanBackend.h"
#include "impl/BLEMutex.h"
#include "esp32-hal-log.h"

#include <utility>

/**
 * @brief Construct a BLEScan handle with no backend.
 *
 * @note The handle is invalid until initialized by BLEClass. Use operator bool() to check.
 */
BLEScan::BLEScan() : _impl(nullptr) {}

/**
 * @brief Check whether this handle references a valid scanner backend.
 * @return true if the implementation pointer is non-null and initialized, false otherwise.
 */
BLEScan::operator bool() const {
  BLE_CHECK_IMPL(false);
  (void)impl;
  return true;
}

/**
 * @brief Set the scan interval by converting milliseconds to BLE controller units.
 * @param intervalMs Interval between scan windows, in milliseconds.
 * @note Internally converts to controller units using the formula (ms * 1000) / 625.
 *       No-op when BLE_SCANNING_SUPPORTED is not defined.
 */
void BLEScan::setInterval(uint16_t intervalMs) {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  log_d("Scan: setInterval %u ms", intervalMs);
  impl.interval = (intervalMs * 1000) / 625;
#endif
}

/**
 * @brief Set the scan window by converting milliseconds to BLE controller units.
 * @param windowMs Duration of each scan window, in milliseconds.
 * @note Internally converts to controller units using the formula (ms * 1000) / 625.
 *       No-op when BLE_SCANNING_SUPPORTED is not defined.
 */
void BLEScan::setWindow(uint16_t windowMs) {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  log_d("Scan: setWindow %u ms", windowMs);
  impl.window = (windowMs * 1000) / 625;
#endif
}

/**
 * @brief Enable or disable active scanning.
 * @param active If true, the scanner sends scan-request PDUs to obtain scan-response data.
 * @note No-op when BLE_SCANNING_SUPPORTED is not defined.
 */
void BLEScan::setActiveScan(bool active) {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  log_d("Scan: setActiveScan=%d", active);
  impl.activeScan = active;
#endif
}

/**
 * @brief Enable or disable duplicate filtering.
 * @param filter If true, the stack suppresses repeated advertisements from the same device.
 * @note No-op when BLE_SCANNING_SUPPORTED is not defined.
 */
void BLEScan::setFilterDuplicates(bool filter) {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  log_d("Scan: setFilterDuplicates=%d", filter);
  impl.filterDuplicates = filter;
#endif
}

/**
 * @brief Check whether a scan is currently running.
 * @return true if scanning is active, false if stopped or handle is invalid.
 * @note Null-safe: returns false when _impl is nullptr.
 */
bool BLEScan::isScanning() const {
  return _impl && _impl->scanning;
}

/**
 * @brief Register a handler called for every advertisement received.
 * @param callback The handler to invoke, or nullptr to clear.
 * @note Logs a warning and discards the callback when BLE_SCANNING_SUPPORTED is not defined.
 */
void BLEScan::onResult(ResultHandler callback) {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  impl.onResultCb = callback;
#else
  (void)callback;
  log_w("%s not supported (scanning disabled)", __func__);
#endif
}

/**
 * @brief Register a handler called when the scan completes.
 * @param callback The handler to invoke, or nullptr to clear.
 * @note Logs a warning and discards the callback when BLE_SCANNING_SUPPORTED is not defined.
 */
void BLEScan::onComplete(CompleteHandler callback) {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  impl.onCompleteCb = callback;
#else
  (void)callback;
  log_w("%s not supported (scanning disabled)", __func__);
#endif
}

/**
 * @brief Remove all registered scan callbacks, including periodic sync handlers.
 * @note Clears onResult, onComplete, periodicSync, periodicReport, and periodicLost handlers.
 */
void BLEScan::resetCallbacks() {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  impl.onResultCb = nullptr;
  impl.onCompleteCb = nullptr;
  impl.periodicSyncCb = nullptr;
  impl.periodicReportCb = nullptr;
  impl.periodicLostCb = nullptr;
#endif
}

/**
 * @brief Register a handler for periodic sync establishment events.
 * @param handler The handler to invoke, or nullptr to clear.
 * @note Logs a warning and discards the handler when BLE_SCANNING_SUPPORTED is not defined.
 */
void BLEScan::onPeriodicSync(PeriodicSyncHandler handler) {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  impl.periodicSyncCb = handler;
#else
  (void)handler;
  log_w("%s not supported (scanning disabled)", __func__);
#endif
}

/**
 * @brief Register a handler for periodic advertising report events.
 * @param handler The handler to invoke, or nullptr to clear.
 * @note Logs a warning and discards the handler when BLE_SCANNING_SUPPORTED is not defined.
 */
void BLEScan::onPeriodicReport(PeriodicReportHandler handler) {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  impl.periodicReportCb = handler;
#else
  (void)handler;
  log_w("%s not supported (scanning disabled)", __func__);
#endif
}

/**
 * @brief Register a handler for periodic sync lost events.
 * @param handler The handler to invoke, or nullptr to clear.
 * @note Logs a warning and discards the handler when BLE_SCANNING_SUPPORTED is not defined.
 */
void BLEScan::onPeriodicLost(PeriodicLostHandler handler) {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  impl.periodicLostCb = handler;
#else
  (void)handler;
  log_w("%s not supported (scanning disabled)", __func__);
#endif
}

#if BLE_SCANNING_SUPPORTED

/**
 * @brief Retrieve a snapshot of the current scan results.
 * @return Copy of the accumulated scan results, or an empty BLEScanResults if the handle is invalid.
 * @note Thread-safe: acquires the implementation mutex before copying.
 */
BLEScanResults BLEScan::getResults() {
  if (!_impl) {
    return BLEScanResults();
  }
  BLELockGuard lock(_impl->mtx);
  return _impl->results;
}

/**
 * @brief Discard all accumulated scan results.
 * @note Thread-safe: acquires the implementation mutex before clearing.
 */
void BLEScan::clearResults() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.results = BLEScanResults();
}

/**
 * @brief Remove a specific device from the result set by address.
 * @param address BLE address of the device to erase.
 * @note Thread-safe: acquires the implementation mutex. Performs a linear search
 *       and removes only the first matching entry.
 */
void BLEScan::erase(const BTAddress &address) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  auto &devs = impl.results._devices;
  for (auto it = devs.begin(); it != devs.end(); ++it) {
    if (it->getAddress() == address) {
      devs.erase(it);
      return;
    }
  }
}

#endif /* BLE_SCANNING_SUPPORTED */

#endif /* BLE_ENABLED */
