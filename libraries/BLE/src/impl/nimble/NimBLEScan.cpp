/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
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
 * @file NimBLEScan.cpp
 * @brief NimBLE (BLE 5) implementation of BLE scan discovery and related GAP events.
 */

#include "impl/BLEGuards.h"
#if BLE_NIMBLE

#include "BLE.h"

#include "NimBLEScan.h"
#include "NimBLEAdvertisedDevice.h"
#include "impl/BLEImplHelpers.h"
#include "esp32-hal-log.h"

#if BLE_SCANNING_SUPPORTED

#include <host/ble_hs.h>
#include <host/ble_gap.h>

namespace {

/**
 * @brief Invokes the per-result callback with a discovered device, if set.
 * @param impl Scan implementation that holds callbacks and state.
 * @param device Device reported by the stack.
 */
void dispatchResult(BLEScan::Impl *impl, BLEAdvertisedDevice device) {
  BLEScan::ResultHandler cb;
  {
    BLELockGuard lock(impl->mtx);
    cb = impl->onResultCb;
  }
  if (cb) {
    cb(device);
  }
}

/**
 * @brief Merges or appends a scan result into the in-memory result list.
 * @param impl Scan implementation whose result list is updated.
 * @param device Advertised device to append or replace (by address).
 */
void appendOrReplaceResult(BLEScan::Impl *impl, const BLEAdvertisedDevice &device) {
  BLELockGuard lock(impl->mtx);
  impl->results.appendOrReplace(device);
}

/**
 * @brief Invokes the scan-complete callback with the current result set, if set.
 * @param impl Scan implementation that holds the callback and results.
 */
void dispatchComplete(BLEScan::Impl *impl) {
  BLEScan::CompleteHandler cb;
  BLEScanResults results;
  {
    BLELockGuard lock(impl->mtx);
    cb = impl->onCompleteCb;
    results = impl->results;
  }
  if (cb) {
    cb(results);
  }
}

#if BLE5_SUPPORTED
/**
 * @brief Invokes the periodic advertising sync callback, if set.
 * @param impl Scan implementation that holds the callback.
 * @param syncHandle Periodic sync link handle from the stack.
 * @param sid Advertising set identifier.
 * @param addr Address of the periodic advertiser.
 * @param phy PHY used for the advertising train.
 * @param interval Periodic advertising interval (stack units).
 */
void dispatchPeriodicSync(BLEScan::Impl *impl, uint16_t syncHandle, uint8_t sid, const BTAddress &addr, BLEPhy phy, uint16_t interval) {
  BLEScan::PeriodicSyncHandler cb;
  {
    BLELockGuard lock(impl->mtx);
    cb = impl->periodicSyncCb;
  }
  if (cb) {
    cb(syncHandle, sid, addr, phy, interval);
  }
}

/**
 * @brief Invokes the periodic report callback with payload data, if set.
 * @param impl Scan implementation that holds the callback.
 * @param syncHandle Periodic sync link handle.
 * @param rssi Reported RSSI in dBm.
 * @param txPower Reported TX power if present.
 * @param data Pointer to advertising payload bytes (may be null if empty).
 * @param len Length of the payload in bytes.
 */
void dispatchPeriodicReport(BLEScan::Impl *impl, uint16_t syncHandle, int8_t rssi, int8_t txPower, const uint8_t *data, size_t len) {
  BLEScan::PeriodicReportHandler cb;
  {
    BLELockGuard lock(impl->mtx);
    cb = impl->periodicReportCb;
  }
  if (cb) {
    cb(syncHandle, rssi, txPower, data, len);
  }
}

/**
 * @brief Invokes the periodic sync lost callback, if set.
 * @param impl Scan implementation that holds the callback.
 * @param syncHandle Periodic sync link that was lost.
 */
void dispatchPeriodicLost(BLEScan::Impl *impl, uint16_t syncHandle) {
  BLEScan::PeriodicLostHandler cb;
  {
    BLELockGuard lock(impl->mtx);
    cb = impl->periodicLostCb;
  }
  if (cb) {
    cb(syncHandle);
  }
}
#endif

}  // namespace

/**
 * @brief Converts a legacy NimBLE discovery descriptor into a BLEAdvertisedDevice.
 * @param disc GAP discovery descriptor from NimBLE.
 * @return Wrapper around the parsed advertised device.
 */
BLEAdvertisedDevice BLEScan::Impl::parseDiscEvent(const struct ble_gap_disc_desc *disc) {
  auto impl = std::make_shared<BLEAdvertisedDevice::Impl>();
  impl->address = BTAddress(disc->addr.val, static_cast<BTAddress::Type>(disc->addr.type));
  impl->addrType = static_cast<BTAddress::Type>(disc->addr.type);
  impl->rssi = disc->rssi;
  impl->hasRSSI = true;
  impl->legacy = true;

  switch (disc->event_type) {
    case BLE_HCI_ADV_RPT_EVTYPE_ADV_IND:
      impl->connectable = true;
      impl->scannable = true;
      impl->advType = BLEAdvType::ConnectableScannable;
      break;
    case BLE_HCI_ADV_RPT_EVTYPE_DIR_IND:
      impl->connectable = true;
      impl->directed = true;
      impl->advType = BLEAdvType::ConnectableDirected;
      break;
    case BLE_HCI_ADV_RPT_EVTYPE_SCAN_IND:
      impl->scannable = true;
      impl->advType = BLEAdvType::ScannableUndirected;
      break;
    case BLE_HCI_ADV_RPT_EVTYPE_NONCONN_IND: impl->advType = BLEAdvType::NonConnectable; break;
    default:                                 break;
  }

  if (disc->length_data > 0 && disc->data != nullptr) {
    impl->parsePayload(disc->data, disc->length_data);
  }

  return BLEAdvertisedDevice(impl);
}

#if BLE5_SUPPORTED
/**
 * @brief Converts an extended (BLE 5) discovery descriptor into a BLEAdvertisedDevice.
 * @param disc Extended GAP discovery descriptor from NimBLE.
 * @return Wrapper around the parsed advertised device.
 */
BLEAdvertisedDevice BLEScan::Impl::parseExtDiscEvent(const struct ble_gap_ext_disc_desc *disc) {
  auto impl = std::make_shared<BLEAdvertisedDevice::Impl>();
  impl->address = BTAddress(disc->addr.val, static_cast<BTAddress::Type>(disc->addr.type));
  impl->addrType = static_cast<BTAddress::Type>(disc->addr.type);
  impl->rssi = disc->rssi;
  impl->hasRSSI = true;
  impl->legacy = (disc->props & BLE_HCI_ADV_LEGACY_MASK) != 0;
  impl->connectable = (disc->props & BLE_HCI_ADV_CONN_MASK) != 0;
  impl->scannable = (disc->props & BLE_HCI_ADV_SCAN_MASK) != 0;
  impl->directed = (disc->props & BLE_HCI_ADV_DIRECT_MASK) != 0;
  impl->primaryPhy = static_cast<BLEPhy>(disc->prim_phy);
  impl->secondaryPhy = static_cast<BLEPhy>(disc->sec_phy);
  impl->sid = disc->sid;
  impl->periodicInterval = disc->periodic_adv_itvl;
  if (disc->tx_power != 127) {
    impl->txPower = disc->tx_power;
    impl->hasTXPower = true;
  }

  if (disc->length_data > 0 && disc->data != nullptr) {
    impl->parsePayload(disc->data, disc->length_data);
  }

  return BLEAdvertisedDevice(impl);
}
#endif

/**
 * @brief NimBLE GAP event callback for scan, extended scan, and periodic sync events.
 * @param event GAP event from NimBLE.
 * @param arg User pointer to BLEScan::Impl.
 * @return 0 to indicate the event was handled (NimBLE convention); non-handled paths are not used here.
 */
int BLEScan::Impl::gapEventHandler(struct ble_gap_event *event, void *arg) {
  auto *impl = static_cast<BLEScan::Impl *>(arg);
  if (!impl) {
    return 0;
  }

  switch (event->type) {
    case BLE_GAP_EVENT_DISC:
    {
      if (event->disc.event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP) {
        BTAddress addr(event->disc.addr.val, static_cast<BTAddress::Type>(event->disc.addr.type));
        BLEAdvertisedDevice merged;
        bool found = false;
        {
          BLELockGuard lock(impl->mtx);
          for (auto &d : impl->results._devices) {
            if (d.getAddress() == addr) {
              d.mergeScanResponse(event->disc.data, event->disc.length_data, event->disc.rssi);
              merged = d;
              found = true;
              break;
            }
          }
        }
        if (found) {
          dispatchResult(impl, merged);
          return 0;
        }
      }

      BLEAdvertisedDevice dev = impl->parseDiscEvent(&event->disc);
      dispatchResult(impl, dev);
      appendOrReplaceResult(impl, dev);
      return 0;
    }

    case BLE_GAP_EVENT_DISC_COMPLETE:
    {
      {
        BLELockGuard lock(impl->mtx);
        impl->scanning = false;
      }
      dispatchComplete(impl);
      impl->scanSync.give(BTStatus::OK);
      return 0;
    }

#if BLE5_SUPPORTED
    case BLE_GAP_EVENT_EXT_DISC:
    {
      BLEAdvertisedDevice dev = impl->parseExtDiscEvent(&event->ext_disc);
      dispatchResult(impl, dev);
      appendOrReplaceResult(impl, dev);
      return 0;
    }

    case BLE_GAP_EVENT_PERIODIC_SYNC:
    {
      if (impl->periodicSyncCb) {
        BTAddress addr(event->periodic_sync.adv_addr.val, static_cast<BTAddress::Type>(event->periodic_sync.adv_addr.type));
        dispatchPeriodicSync(
          impl, event->periodic_sync.sync_handle, event->periodic_sync.sid, addr, static_cast<BLEPhy>(event->periodic_sync.adv_phy),
          event->periodic_sync.per_adv_ival
        );
      }
      return 0;
    }

    case BLE_GAP_EVENT_PERIODIC_REPORT:
    {
      if (impl->periodicReportCb) {
        const uint8_t *data = event->periodic_report.om ? OS_MBUF_DATA(event->periodic_report.om, const uint8_t *) : nullptr;
        size_t len = event->periodic_report.om ? OS_MBUF_PKTLEN(event->periodic_report.om) : 0;
        dispatchPeriodicReport(impl, event->periodic_report.sync_handle, event->periodic_report.rssi, event->periodic_report.tx_power, data, len);
      }
      return 0;
    }

    case BLE_GAP_EVENT_PERIODIC_SYNC_LOST:
    {
      if (impl->periodicLostCb) {
        dispatchPeriodicLost(impl, event->periodic_sync_lost.sync_handle);
      }
      return 0;
    }
#endif

    default: return 0;
  }
}

// --------------------------------------------------------------------------
// BLEScan public API
// --------------------------------------------------------------------------

/**
 * @brief Clears any duplicate-advertisement filter state (no-op; NimBLE manages this internally).
 */
void BLEScan::clearDuplicateCache() { /* NimBLE manages this internally */ }

/**
 * @brief Starts a legacy (non-extended) GAP scan for a given duration.
 * @param durationMs Scan duration in milliseconds, or 0 for infinite.
 * @param continueExisting If true, keep existing results; if a scan is already running, behavior depends on reconfiguration.
 * @return Outcome of starting the scan.
 */
BTStatus BLEScan::start(uint32_t durationMs, bool continueExisting) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.scanning && !continueExisting) {
    stop();
  }

  if (!continueExisting) {
    impl.results._devices.clear();
  }

  log_d("Scan: start duration=%u ms active=%d filterDuplicates=%d", durationMs, impl.activeScan, impl.filterDuplicates);

  struct ble_gap_disc_params params = {};
  params.filter_duplicates = impl.filterDuplicates ? 1 : 0;
  params.passive = impl.activeScan ? 0 : 1;
  params.itvl = impl.interval;
  params.window = impl.window;
  params.filter_policy = 0;
  params.limited = 0;

  int rc =
    ble_gap_disc(static_cast<uint8_t>(BLE.getOwnAddressType()), durationMs == 0 ? BLE_HS_FOREVER : (int32_t)durationMs, &params, Impl::gapEventHandler, &impl);
  if (rc != 0) {
    log_e("ble_gap_disc: rc=%d", rc);
    return BTStatus::Fail;
  }
  impl.scanning = true;
  return BTStatus::OK;
}

/**
 * @brief Clears results, starts a scan, and blocks until the scan finishes or times out.
 * @param durationMs Duration to pass to the stack (completion is also gated by the internal sync object).
 * @return Collected results after the blocking wait; may be empty on error.
 */
BLEScanResults BLEScan::startBlocking(uint32_t durationMs) {
  BLE_CHECK_IMPL(BLEScanResults());
  impl.results._devices.clear();
  impl.scanSync.take();

  struct ble_gap_disc_params params = {};
  params.filter_duplicates = impl.filterDuplicates ? 1 : 0;
  params.passive = impl.activeScan ? 0 : 1;
  params.itvl = impl.interval;
  params.window = impl.window;

  int rc = ble_gap_disc(static_cast<uint8_t>(BLE.getOwnAddressType()), (int32_t)durationMs, &params, Impl::gapEventHandler, &impl);
  if (rc != 0) {
    impl.scanSync.give(BTStatus::Fail);
    log_e("ble_gap_disc: rc=%d", rc);
    return BLEScanResults();
  }
  impl.scanning = true;
  impl.scanSync.wait(durationMs + 2000);
  return impl.results;
}

/**
 * @brief Cancels an in-progress GAP scan.
 * @return Outcome of stopping; OK if not scanning.
 */
BTStatus BLEScan::stop() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!impl.scanning) {
    return BTStatus::OK;
  }
  log_d("Scan: stop");
  int rc = ble_gap_disc_cancel();
  impl.scanning = false;
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    log_e("Scan: ble_gap_disc_cancel rc=%d", rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

/**
 * @brief Starts extended scanning on supported PHYs (uncoded and optionally coded).
 * @param durationMs Scan window in milliseconds (stack-specific scaling in implementation).
 * @param codedConfig Optional coded PHY timing; null to mirror defaults/uncoded.
 * @param uncodedConfig Optional 1M PHY timing; null to use default interval/window.
 * @return NotSupported when BLE5_SUPPORTED is off; otherwise success or fail from the stack.
 */
BTStatus BLEScan::startExtended(uint32_t durationMs, const ExtScanConfig *codedConfig, const ExtScanConfig *uncodedConfig) {
#if BLE5_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  impl.results._devices.clear();

  struct ble_gap_ext_disc_params uncodedParams = {};
  struct ble_gap_ext_disc_params codedParams = {};
  uint8_t active = impl.activeScan ? 1 : 0;

  uncodedParams.passive = !active;
  uncodedParams.itvl = uncodedConfig ? ((uncodedConfig->interval * 1000) / 625) : impl.interval;
  uncodedParams.window = uncodedConfig ? ((uncodedConfig->window * 1000) / 625) : impl.window;

  codedParams.passive = !active;
  codedParams.itvl = codedConfig ? ((codedConfig->interval * 1000) / 625) : impl.interval;
  codedParams.window = codedConfig ? ((codedConfig->window * 1000) / 625) : impl.window;

  int rc = ble_gap_ext_disc(
    static_cast<uint8_t>(BLE.getOwnAddressType()), durationMs == 0 ? 0 : (durationMs / 10), 0, impl.filterDuplicates ? 1 : 0, 0, 0, &uncodedParams,
    codedConfig ? &codedParams : NULL, Impl::gapEventHandler, &impl
  );
  if (rc != 0) {
    log_e("ble_gap_ext_disc: rc=%d", rc);
    return BTStatus::Fail;
  }
  impl.scanning = true;
  return BTStatus::OK;
#else
  log_w("Scan: startExtended not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

/**
 * @brief Stops extended scanning (delegates to stop() on NimBLE).
 * @return Same as stop().
 */
BTStatus BLEScan::stopExtended() {
  return stop();
}

/**
 * @brief Creates a periodic advertising sync to a specific advertiser and SID.
 * @param addr Advertiser address.
 * @param sid Advertising set ID to sync to.
 * @param skipCount Maximum number of events that may be skipped.
 * @param timeoutMs Sync timeout in milliseconds (scaled for the stack inside this call).
 * @return Not supported when extended features are off; otherwise stack result.
 */
BTStatus BLEScan::createPeriodicSync(const BTAddress &addr, uint8_t sid, uint16_t skipCount, uint16_t timeoutMs) {
#if BLE5_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);

  struct ble_gap_periodic_sync_params params = {};
  params.skip = skipCount;
  params.sync_timeout = timeoutMs / 10;

  ble_addr_t bleAddr;
  bleAddr.type = static_cast<uint8_t>(addr.type());
  memcpy(bleAddr.val, addr.data(), 6);

  int rc = ble_gap_periodic_adv_sync_create(&bleAddr, sid, &params, Impl::gapEventHandler, &impl);
  if (rc != 0) {
    log_e("ble_gap_periodic_adv_sync_create: rc=%d", rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
#else
  log_w("Scan: createPeriodicSync not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

/**
 * @brief Cancels an in-progress periodic sync establishment request.
 * @return Not supported when BLE5 is off; otherwise stack result.
 */
BTStatus BLEScan::cancelPeriodicSync() {
#if BLE5_SUPPORTED
  int rc = ble_gap_periodic_adv_sync_create_cancel();
  if (rc != 0) {
    log_e("Scan: cancelPeriodicSync failed rc=%d", rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
#else
  log_w("Scan: cancelPeriodicSync not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

/**
 * @brief Terminates an established periodic sync by handle.
 * @param syncHandle Stack handle of the periodic sync to drop.
 * @return Not supported when BLE5 is off; otherwise stack result.
 */
BTStatus BLEScan::terminatePeriodicSync(uint16_t syncHandle) {
#if BLE5_SUPPORTED
  int rc = ble_gap_periodic_adv_sync_terminate(syncHandle);
  if (rc != 0) {
    log_e("Scan: terminatePeriodicSync handle=%u rc=%d", syncHandle, rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
#else
  log_w("Scan: terminatePeriodicSync not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

// --------------------------------------------------------------------------
// BLEClass::getScan() -- NimBLE factory method
// --------------------------------------------------------------------------

/**
 * @brief Returns a BLEScan instance backed by a process-wide shared implementation.
 * @return Valid scan object if BLE is initialized; otherwise an empty/invalid handle.
 */
BLEScan BLEClass::getScan() {
  if (!isInitialized()) {
    log_e("getScan: BLE not initialized");
    return BLEScan();
  }
  static std::shared_ptr<BLEScan::Impl> scanImpl;
  if (!scanImpl) {
    scanImpl = std::make_shared<BLEScan::Impl>();
  }
  return BLEScan(scanImpl);
}

#else /* !BLE_SCANNING_SUPPORTED -- stubs */

// Stubs for BLE_SCANNING_SUPPORTED == 0: log; start* return NotSupported or empty results/scan.

void BLEScan::clearDuplicateCache() {
  log_w("Scanning not supported");
}

BTStatus BLEScan::start(uint32_t, bool) {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BLEScanResults BLEScan::startBlocking(uint32_t) {
  log_w("Scanning not supported");
  return BLEScanResults();
}

BTStatus BLEScan::stop() {
  log_w("Scanning not supported");
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

BTStatus BLEScan::cancelPeriodicSync() {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEScan::terminatePeriodicSync(uint16_t) {
  log_w("Scanning not supported");
  return BTStatus::NotSupported;
}

BLEScan BLEClass::getScan() {
  log_w("Scanning not supported");
  return BLEScan();
}

#endif /* BLE_SCANNING_SUPPORTED */

#endif /* BLE_NIMBLE */
