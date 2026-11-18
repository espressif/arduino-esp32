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

/**
 * @file BLEScan.cpp
 * @brief Stack-agnostic `BLEScan` implementation: handle lifecycle, result-set
 *        management, scan parameters/callbacks, and feature-disabled stubs.
 */

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "scan/BLEScan.h"
#include "scan/BLEScanImpl.h"
#include "core/BLEImplHelpers.h"
#include "advertising/BLEAdvScanHelpers.h"
#include "core/BLEBackend.h"
#include "core/BLEMutex.h"
#include "esp32-hal-log.h"

#include <utility>

// ==========================================================================
// 1. Handle construction & scan result management
// ==========================================================================

BLEScan::BLEScan() : _impl(nullptr) {}

BLEScan::operator bool() const {
  return _impl != nullptr;
}

bool BLEScan::isScanning() const {
  BLE_CHECK_IMPL(false);
  return impl.isScanning;
}

BLEScan::Results BLEScan::getResults() const {
  BLE_CHECK_IMPL(BLEScan::Results());
  BLELockGuard lock(impl.mtx);
  return impl.results;
}

void BLEScan::clearResults() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.results = BLEScan::Results();
}

void BLEScan::erase(const BTAddress &address) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  // Linear search; removes only the first matching entry.
  auto &devs = impl.results._devices;
  for (auto it = devs.begin(); it != devs.end(); ++it) {
    if (it->getAddress() == address) {
      devs.erase(it);
      return;
    }
  }
}

BLEAdvertisedDevice BLEScan::Results::appendOrReplace(const BLEAdvertisedDevice &device) {
  for (auto &existing : _devices) {
    // BLE 5 lets one advertiser run several advertising sets concurrently from a
    // single address, distinguished by the Advertising SID (ADI). Key results on
    // (address, SID) so those sets do not overwrite each other. Legacy PDUs carry
    // no ADI and report SID 0xFF, so same-address legacy reports still collapse to
    // a single entry exactly as before.
    if (existing.getAddress() == device.getAddress() && existing.getAdvSID() == device.getAdvSID()) {
      // A fresh primary report supersedes the old primary payload and metadata,
      // but it must not erase fields learned from the preceding scan response.
      // Rebuild from the fresh report plus only the cached scan-response suffix;
      // returning the whole cached object would preserve stale RSSI and changed
      // manufacturer/service data from the old primary advertisement.
      BLEAdvertisedDevice merged;
      if (BLEScanImplCommon::preserveScanResponse(device, existing, merged)) {
        existing = merged;
      } else {
        existing = device;
      }
      return existing;
    }
  }
  _devices.push_back(device);
  return device;
}

void BLEScan::Results::dump() const {
  for (size_t i = 0; i < _devices.size(); i++) {
    log_i("[%d] %s", i, _devices[i].toString().c_str());
  }
}

// ==========================================================================
// 2. Scan configuration, callbacks & blocking start (BLE_SCANNING_SUPPORTED)
// ==========================================================================

#if BLE_SCANNING_SUPPORTED

void BLEScan::setInterval(uint16_t intervalMs) {
  BLE_CHECK_IMPL();
  log_d("Scan: setInterval %u ms", intervalMs);
  BLELockGuard lock(impl.mtx);
  impl.interval = BLEAdvInternal::msToUnits0625(intervalMs);
}

void BLEScan::setWindow(uint16_t windowMs) {
  BLE_CHECK_IMPL();
  log_d("Scan: setWindow %u ms", windowMs);
  BLELockGuard lock(impl.mtx);
  impl.window = BLEAdvInternal::msToUnits0625(windowMs);
}

void BLEScan::setActiveScan(bool active) {
  BLE_CHECK_IMPL();
  log_d("Scan: setActiveScan=%d", active);
  BLELockGuard lock(impl.mtx);
  impl.activeScan = active;
}

void BLEScan::setFilterDuplicates(bool filter) {
  BLE_CHECK_IMPL();
  log_d("Scan: setFilterDuplicates=%d", filter);
  BLELockGuard lock(impl.mtx);
  impl.filterDuplicates = filter;
}

void BLEScan::onResult(ResultHandler callback) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onResultCb = std::move(callback);
}

void BLEScan::onComplete(CompleteHandler callback) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onCompleteCb = std::move(callback);
}

void BLEScan::onPeriodicSync(PeriodicSyncHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.periodicSyncCb = std::move(handler);
}

void BLEScan::onPeriodicReport(PeriodicReportHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.periodicReportCb = std::move(handler);
}

void BLEScan::onPeriodicLost(PeriodicLostHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.periodicLostCb = std::move(handler);
}

void BLEScan::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onResultCb = nullptr;
  impl.onCompleteCb = nullptr;
  impl.periodicSyncCb = nullptr;
  impl.periodicReportCb = nullptr;
  impl.periodicLostCb = nullptr;
}

void BLEScan::clearDuplicateCache() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.duplicateKeys.clear();
}

BLEScan::Results BLEScan::startBlocking(uint32_t durationMs) {
  BLE_CHECK_IMPL(BLEScan::Results());
  if (start(durationMs, false) != BTStatus::OK) {
    log_e("Scan: startBlocking failed to start scan");
    return BLEScan::Results();
  }
  uint32_t waitMs = (durationMs == 0) ? portMAX_DELAY : durationMs + 2000;
  BTStatus waitSt = impl.scanSync.wait(waitMs);
  if (waitSt != BTStatus::OK) {
    log_w("Scan: startBlocking wait timed out");
  }
  BLELockGuard lock(impl.mtx);
  return impl.results;
}

BTStatus BLEScan::stopExtended() {
  return stop();
}

void BLEScan::prepareBackendStart() {
  if (_impl) {
    _impl->nativeAdmission.reopen();
  }
}

#endif /* BLE_SCANNING_SUPPORTED */

// ==========================================================================
// 3. Feature-disabled stubs (!BLE_SCANNING_SUPPORTED)
// ==========================================================================

#if !BLE_SCANNING_SUPPORTED

void BLEScan::setInterval(uint16_t) {}

void BLEScan::setWindow(uint16_t) {}

void BLEScan::setActiveScan(bool) {}

void BLEScan::setFilterDuplicates(bool) {}

void BLEScan::onResult(ResultHandler) {
  log_w("%s not supported (scanning disabled)", __func__);
}

void BLEScan::onComplete(CompleteHandler) {
  log_w("%s not supported (scanning disabled)", __func__);
}

void BLEScan::onPeriodicSync(PeriodicSyncHandler) {
  log_w("%s not supported (scanning disabled)", __func__);
}

void BLEScan::onPeriodicReport(PeriodicReportHandler) {
  log_w("%s not supported (scanning disabled)", __func__);
}

void BLEScan::onPeriodicLost(PeriodicLostHandler) {
  log_w("%s not supported (scanning disabled)", __func__);
}

void BLEScan::resetCallbacks() {}

void BLEScan::clearDuplicateCache() {
  log_w("Scanning not supported");
}

BTStatus BLEScan::start(uint32_t, bool) {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BLEScan::Results BLEScan::startBlocking(uint32_t) {
  log_w("Scanning not supported");
  return BLEScan::Results();
}

BTStatus BLEScan::stop() {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEScan::stopBackend() {
  return BTStatus::NotSupported;
}

BTStatus BLEScan::startExtended(uint32_t, const ExtScanConfig *, const ExtScanConfig *) {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEScan::stopExtended() {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEScan::createPeriodicSync(const BTAddress &, uint8_t, uint16_t, uint16_t) {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEScan::receivePeriodicSync(uint16_t, uint16_t, uint16_t) {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEScan::receivePeriodicSync(const BLEConnInfo &, uint16_t, uint16_t) {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEScan::stopPeriodicSyncReceive(uint16_t) {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEScan::stopPeriodicSyncReceive(const BLEConnInfo &) {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEScan::cancelPeriodicSyncCreate() {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEScan::terminatePeriodicSync(uint16_t) {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

void BLEScan::terminateAllPeriodicSyncs() {}

void BLEScan::resetBackendEvents() {}

void BLEScan::prepareBackendStart() {}

void BLEScan::beginBackendShutdown() {}

#endif /* !BLE_SCANNING_SUPPORTED */

#endif /* BLE_ENABLED */
