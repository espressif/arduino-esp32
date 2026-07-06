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

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include "BLEScan.h"
#include "impl/common/BLEImplHelpers.h"
#include "impl/common/BLEAdvScanHelpers.h"
#include "impl/BLEBackend.h"
#include "impl/common/BLEMutex.h"
#include "esp32-hal-log.h"

#include <utility>

/**
 * @file
 * @brief Stack-agnostic @ref BLEScan implementation.
 */

BLEScan::BLEScan() : _impl(nullptr) {}

BLEScan::operator bool() const {
  return _impl != nullptr;
}

// Scan-parameter setters convert milliseconds to controller units ((ms * 1000)
// / 625) and no-op when BLE_SCANNING_SUPPORTED is not defined.

void BLEScan::setInterval(uint16_t intervalMs) {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  log_d("Scan: setInterval %u ms", intervalMs);
  impl.interval = bleMsToUnits0625(intervalMs);
#endif
}

void BLEScan::setWindow(uint16_t windowMs) {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  log_d("Scan: setWindow %u ms", windowMs);
  impl.window = bleMsToUnits0625(windowMs);
#endif
}

void BLEScan::setActiveScan(bool active) {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  log_d("Scan: setActiveScan=%d", active);
  impl.activeScan = active;
#endif
}

void BLEScan::setFilterDuplicates(bool filter) {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  log_d("Scan: setFilterDuplicates=%d", filter);
  impl.filterDuplicates = filter;
#endif
}

bool BLEScan::isScanning() const {
  return _impl && _impl->isScanning;
}

// Callback setters store the handler and no-op with a warning when scanning is
// compiled out (BLE_SCANNING_SUPPORTED).

void BLEScan::onResult(ResultHandler callback) {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  impl.onResultCb = callback;
#else
  (void)callback;
  log_w("%s not supported (scanning disabled)", __func__);
#endif
}

void BLEScan::onComplete(CompleteHandler callback) {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  impl.onCompleteCb = callback;
#else
  (void)callback;
  log_w("%s not supported (scanning disabled)", __func__);
#endif
}

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

void BLEScan::onPeriodicSync(PeriodicSyncHandler handler) {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  impl.periodicSyncCb = handler;
#else
  (void)handler;
  log_w("%s not supported (scanning disabled)", __func__);
#endif
}

void BLEScan::onPeriodicReport(PeriodicReportHandler handler) {
#if BLE_SCANNING_SUPPORTED
  BLE_CHECK_IMPL();
  impl.periodicReportCb = handler;
#else
  (void)handler;
  log_w("%s not supported (scanning disabled)", __func__);
#endif
}

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

// Result-management helpers are thread-safe: each acquires impl.mtx before
// touching the shared results set.

BLEScan::Results BLEScan::getResults() {
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

void BLEScan::Results::appendOrReplace(const BLEAdvertisedDevice &device) {
  for (auto &existing : _devices) {
    // BLE 5 lets one advertiser run several advertising sets concurrently from a
    // single address, distinguished by the Advertising SID (ADI). Key results on
    // (address, SID) so those sets do not overwrite each other. Legacy PDUs carry
    // no ADI and report SID 0xFF, so same-address legacy reports still collapse to
    // a single entry exactly as before.
    if (existing.getAddress() == device.getAddress() && existing.getAdvSID() == device.getAdvSID()) {
      // Preserve an existing entry that has a name from a merged scan response
      // when the incoming duplicate primary ADV carries no name. Overwriting
      // would silently erase the name, causing getName() to return "".
      if (existing.haveName() && !device.haveName()) {
        return;
      }
      existing = device;
      return;
    }
  }
  _devices.push_back(device);
}

void BLEScan::Results::dump() const {
  for (size_t i = 0; i < _devices.size(); i++) {
    log_i("[%d] %s", i, _devices[i].toString().c_str());
  }
}

#endif /* BLE_SCANNING_SUPPORTED */

#endif /* BLE_ENABLED */
