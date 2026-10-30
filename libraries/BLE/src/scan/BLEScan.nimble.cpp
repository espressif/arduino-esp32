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
 * @file BLEScan.nimble.cpp
 * @brief NimBLE (BLE 5) implementation of BLE scan discovery and related GAP events.
 */

#include "core/BLEGuards.h"
#if BLE_NIMBLE

#include "BLE.h"
#include "BLE.nimble.h"

#include "scan/BLEScan.nimble.h"
#include "scan/BLEAdvertisedDeviceImpl.h"
#include "scan/BLEScanImpl.h"
#include "core/BLEImplHelpers.h"
#include "advertising/BLEAdvScanHelpers.h"
#include "esp32-hal-log.h"
#include "esp_timer.h"

#include <algorithm>
#include <new>

#include "core/BLEValidationHooks.h"

#if BLE_SCANNING_SUPPORTED

#include <host/ble_hs.h>
#include <host/ble_gap.h>
#include <nimble/nimble_port.h>

namespace {

bool duplicateKeySeen(const BLEScan::Impl &impl, const BTAddress &address, uint8_t sid) {
  return std::any_of(impl.duplicateKeys.begin(), impl.duplicateKeys.end(), [&](const auto &key) {
    return key.first == address && key.second == sid;
  });
}

void markDuplicateKey(BLEScan::Impl &impl, const BTAddress &address, uint8_t sid) {
  if (!duplicateKeySeen(impl, address, sid)) {
    impl.duplicateKeys.emplace_back(address, sid);
  }
}

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
 * @brief Invokes the scan-complete callback with the current result set, if set.
 * @param impl Scan implementation that holds the callback and results.
 */
void dispatchComplete(BLEScan::Impl *impl) {
  BLEScan::CompleteHandler cb;
  BLEScan::Results results;
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

  return BLEScanImplCommon::makeAdvertisedDeviceHandle(impl);
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

  return BLEScanImplCommon::makeAdvertisedDeviceHandle(impl);
}
#endif

static BTStatus nimbleStartScanSession(BLEScan::Impl &impl, uint32_t durationMs, uint32_t generation);

static void retireScanContextCb(struct ble_npl_event *event) {
  auto *impl = static_cast<BLEScan::Impl *>(ble_npl_event_get_arg(event));
  std::vector<BLEScan::Impl::EventContext *> retired;
  {
    BLELockGuard lock(impl ? impl->mtx : nullptr);
    if (impl) {
      retired.swap(impl->retiredScanContexts);
    }
  }
  for (auto *context : retired) {
    delete context;
  }
#if BLE_VALIDATION_HOOKS
  BLEHooks.scan.retiredContexts.fetch_sub(static_cast<int>(retired.size()), std::memory_order_relaxed);
#endif
}

static void retireScanContext(BLEScan::Impl::EventContext *context) {
  if (!context) {
    return;
  }
  BLEScan::Impl *impl = context->impl;
  // On the host task no other scan callback can execute concurrently. The
  // current result handler has already consumed its context before invoking
  // user code, so a nested stop() may reclaim it directly.
  if (nimbleIsHostTask()) {
    delete context;
    return;
  }
  bool shouldQueue = false;
  {
    BLELockGuard lock(impl->mtx);
    impl->retiredScanContexts.push_back(context);
#if BLE_VALIDATION_HOOKS
    BLEHooks.scan.retiredContexts.fetch_add(1, std::memory_order_relaxed);
#endif
    if (impl->nativeAdmission.isOpen() && !impl->retireScanEventInited) {
      ble_npl_event_init(&impl->retireScanEvent, retireScanContextCb, impl);
      impl->retireScanEventInited = true;
    }
    shouldQueue = impl->nativeAdmission.isOpen() && impl->retireScanEventInited;
  }
  // One persistent event drains every pending context. Retry only for a bounded
  // interval; BLE.end() closes the gate and reclaims anything still queued
  // after the host has stopped.
  for (uint8_t attempt = 0; shouldQueue && attempt < 8; ++attempt) {
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &impl->retireScanEvent);
    {
      BLELockGuard lock(impl->mtx);
      if (impl->retiredScanContexts.empty()) {
        break;
      }
    }
    if (!impl->nativeAdmission.isOpen()) {
      break;
    }
    vTaskDelay(1);
  }
}

/**
 * @brief NimBLE GAP event callback for scan, extended scan, and periodic sync events.
 * @param event GAP event from NimBLE.
 * @param arg User pointer to BLEScan::Impl::EventContext.
 * @return 0 to indicate the event was handled (NimBLE convention); non-handled paths are not used here.
 */
int BLEScan::Impl::gapEventHandler(struct ble_gap_event *event, void *arg) {
  auto *context = static_cast<BLEScan::Impl::EventContext *>(arg);
  auto *impl = context ? context->impl : nullptr;
  if (!impl) {
    return 0;
  }

  switch (event->type) {
    case BLE_GAP_EVENT_DISC:
    {
      {
        BLELockGuard lock(impl->mtx);
        if (!context->scanSession || context->generation != impl->activeGeneration || !impl->isScanning) {
          return 0;
        }
      }
      if (event->disc.event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP) {
        BTAddress addr(event->disc.addr.val, static_cast<BTAddress::Type>(event->disc.addr.type));
        BLEAdvertisedDevice merged;
        bool found = false;
        bool duplicate = false;
        {
          BLELockGuard lock(impl->mtx);
          // Legacy PDUs carry no ADI; match the legacy entry (SID 0xFF).
          for (const auto &existing : impl->results) {
            if (existing.getAddress() == addr && existing.getAdvSID() == 0xFF) {
              duplicate = impl->filterDuplicates && duplicateKeySeen(*impl, addr, 0xFF)
                          && BLEScanImplCommon::sameScanResponsePayload(existing, event->disc.data, event->disc.length_data);
              break;
            }
          }
          if (!duplicate) {
            found = BLEScanImplCommon::mergeScanResponse(impl->results, addr, 0xFF, event->disc.data, event->disc.length_data, event->disc.rssi, merged);
            if (found && impl->filterDuplicates) {
              markDuplicateKey(*impl, addr, 0xFF);
            }
          } else {
            found = true;
          }
        }
        if (found && !duplicate) {
          dispatchResult(impl, merged);
          return 0;
        }
        if (duplicate) {
          return 0;
        }
      }

      // Cache the result before invoking the callback so getResults() called from
      // inside onResult already includes this device.
      BLEAdvertisedDevice dev = impl->parseDiscEvent(&event->disc);
      bool duplicate = false;
      {
        BLELockGuard lock(impl->mtx);
        if (impl->filterDuplicates) {
          const bool keySeen = duplicateKeySeen(*impl, dev.getAddress(), dev.getAdvSID());
          for (const auto &existing : impl->results) {
            if (existing.getAddress() == dev.getAddress() && existing.getAdvSID() == dev.getAdvSID()) {
              duplicate = keySeen && BLEScanImplCommon::samePrimaryPayload(existing, dev);
              break;
            }
          }
        }
        if (!duplicate) {
          dev = impl->results.appendOrReplace(dev);
          if (impl->filterDuplicates) {
            markDuplicateKey(*impl, dev.getAddress(), dev.getAdvSID());
          }
        }
      }
      if (!duplicate) {
        dispatchResult(impl, dev);
      }
      return 0;
    }

    case BLE_GAP_EVENT_DISC_COMPLETE:
    {
      // BLE_HS_EPREEMPTED: NimBLE temporarily paused scanning to update the
      // controller resolving list (e.g. ble_gap_unpair / ble_hs_pvcy_add_entry).
      // Resume the scan automatically instead of treating it as completion.
      bool completeCurrent = false;
      bool deleteContext = false;
      {
        BLELockGuard lock(impl->mtx);
        if (context->scanSession && context->generation == impl->activeGeneration && impl->isScanning) {
          if (event->disc_complete.reason == BLE_HS_EPREEMPTED) {
            uint32_t remainingMs = 0;
            bool expired = false;
            if (impl->sessionDeadlineUs == 0) {
              remainingMs = 0;
            } else {
              int64_t remainingUs = impl->sessionDeadlineUs - esp_timer_get_time();
              expired = remainingUs <= 0;
              if (!expired) {
                remainingMs = static_cast<uint32_t>((remainingUs + 999) / 1000);
              }
            }
            if (!expired) {
              if (nimbleStartScanSession(*impl, remainingMs, context->generation) == BTStatus::OK) {
                delete context;
                return 0;
              }
              log_w("Scan: failed to resume after BLE_HS_EPREEMPTED");
            }
          }
          impl->isScanning = false;
          completeCurrent = true;
        }
        if (context->scanSession) {
          if (impl->activeScanContext == context) {
            impl->activeScanContext = nullptr;
          }
          auto held = std::find(impl->nativeHeldScanContexts.begin(), impl->nativeHeldScanContexts.end(), context);
          if (held != impl->nativeHeldScanContexts.end()) {
            impl->nativeHeldScanContexts.erase(held);
#if BLE_VALIDATION_HOOKS
            BLEHooks.scan.heldContexts.fetch_sub(1, std::memory_order_relaxed);
#endif
          }
          deleteContext = !context->retireByStop;
        }
      }
      if (deleteContext) {
        delete context;
      }
      if (completeCurrent) {
        dispatchComplete(impl);
        impl->scanSync.give(BTStatus::OK);
      }
      return 0;
    }

#if BLE5_SUPPORTED
    case BLE_GAP_EVENT_EXT_DISC:
    {
      {
        BLELockGuard lock(impl->mtx);
        if (!context->scanSession || context->generation != impl->activeGeneration || !impl->isScanning) {
          return 0;
        }
      }
      const struct ble_gap_ext_disc_desc &d = event->ext_disc;
      // A scan response carries the fields (notably the Local Name) that the
      // scanner solicited with SCAN_REQ; it must be merged into the device from
      // the preceding advertisement rather than surfaced as a nameless device.
      // Detect it via the ext-report properties (extended PDUs) or the
      // legacy_event_type (legacy PDUs reported over the extended API, which is
      // how an ext-adv-enabled controller reports everything). Mirrors the
      // BLE_GAP_EVENT_DISC scan-response handling above.
      const bool isScanRsp =
        (d.props & BLE_HCI_ADV_SCAN_RSP_MASK) != 0 || ((d.props & BLE_HCI_ADV_LEGACY_MASK) && d.legacy_event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP);
      if (isScanRsp) {
        BTAddress addr(d.addr.val, static_cast<BTAddress::Type>(d.addr.type));
        BLEAdvertisedDevice merged;
        bool found = false;
        bool duplicate = false;
        {
          BLELockGuard lock(impl->mtx);
          for (const auto &existing : impl->results) {
            if (existing.getAddress() == addr && existing.getAdvSID() == d.sid) {
              duplicate =
                impl->filterDuplicates && duplicateKeySeen(*impl, addr, d.sid) && BLEScanImplCommon::sameScanResponsePayload(existing, d.data, d.length_data);
              break;
            }
          }
          if (!duplicate) {
            found = BLEScanImplCommon::mergeScanResponse(impl->results, addr, d.sid, d.data, d.length_data, d.rssi, merged);
            if (found && impl->filterDuplicates) {
              markDuplicateKey(*impl, addr, d.sid);
            }
          } else {
            found = true;
          }
        }
        if (found && !duplicate) {
          dispatchResult(impl, merged);
          return 0;
        }
        if (duplicate) {
          return 0;
        }
        // No prior advertisement to merge into: fall through and surface the
        // scan response as a standalone device so its name is not lost.
      }

      // Cache before invoking the callback (see BLE_GAP_EVENT_DISC).
      BLEAdvertisedDevice dev = impl->parseExtDiscEvent(&event->ext_disc);
      bool duplicate = false;
      {
        BLELockGuard lock(impl->mtx);
        if (impl->filterDuplicates) {
          const bool keySeen = duplicateKeySeen(*impl, dev.getAddress(), dev.getAdvSID());
          for (const auto &existing : impl->results) {
            if (existing.getAddress() == dev.getAddress() && existing.getAdvSID() == dev.getAdvSID()) {
              duplicate = keySeen && BLEScanImplCommon::samePrimaryPayload(existing, dev);
              break;
            }
          }
        }
        if (!duplicate) {
          dev = impl->results.appendOrReplace(dev);
          if (impl->filterDuplicates) {
            markDuplicateKey(*impl, dev.getAddress(), dev.getAdvSID());
          }
        }
      }
      if (!duplicate) {
        dispatchResult(impl, dev);
      }
      return 0;
    }

    case BLE_GAP_EVENT_PERIODIC_SYNC:
    {
      // Track successfully established syncs so BLE.end() can tear them down.
      if (event->periodic_sync.status == 0) {
        BLELockGuard lock(impl->mtx);
        impl->periodicSyncs.push_back(event->periodic_sync.sync_handle);
      }
      BTAddress addr(event->periodic_sync.adv_addr.val, static_cast<BTAddress::Type>(event->periodic_sync.adv_addr.type));
      dispatchPeriodicSync(
        impl, event->periodic_sync.sync_handle, event->periodic_sync.sid, addr, static_cast<BLEPhy>(event->periodic_sync.adv_phy),
        event->periodic_sync.per_adv_ival
      );
      return 0;
    }

    case BLE_GAP_EVENT_PERIODIC_REPORT:
    {
      // Newer NimBLE delivers the periodic report payload as a flat buffer
      // (const uint8_t *data + data_length) instead of an os_mbuf (.om).
      const uint8_t *data = event->periodic_report.data;
      size_t len = event->periodic_report.data_length;
      dispatchPeriodicReport(impl, event->periodic_report.sync_handle, event->periodic_report.rssi, event->periodic_report.tx_power, data, len);
      return 0;
    }

    case BLE_GAP_EVENT_PERIODIC_SYNC_LOST:
    {
      {
        BLELockGuard lock(impl->mtx);
        auto &v = impl->periodicSyncs;
        v.erase(std::remove(v.begin(), v.end(), event->periodic_sync_lost.sync_handle), v.end());
      }
      dispatchPeriodicLost(impl, event->periodic_sync_lost.sync_handle);
      return 0;
    }
#endif

    default: return 0;
  }
}

// --------------------------------------------------------------------------
// BLEScan public API
// --------------------------------------------------------------------------

// API contract is documented on the declarations in the public BLE*.h headers; the definitions below carry implementation notes only.

static BTStatus nimbleStartScanSession(BLEScan::Impl &impl, uint32_t durationMs, uint32_t generation) {
  auto *context = new (std::nothrow) BLEScan::Impl::EventContext{&impl, generation, true};
  if (!context) {
    return BTStatus::NoMemory;
  }
#if BLE5_SUPPORTED
  // On ext-adv-enabled controllers legacy ble_gap_disc results are delivered as
  // BLE_GAP_EVENT_EXT_DISC, so drive the extended discovery API directly (1M,
  // uncoded) and let the EXT_DISC handler merge scan responses.
  struct ble_gap_ext_disc_params uncoded = {};
  struct ble_gap_ext_disc_params coded = {};
  uint8_t active = impl.activeScan ? 1 : 0;
  uncoded.passive = !active;
  uncoded.itvl = impl.hasUncodedConfig ? impl.uncodedInterval : impl.interval;
  uncoded.window = impl.hasUncodedConfig ? impl.uncodedWindow : impl.window;
  coded.passive = !active;
  coded.itvl = impl.hasCodedConfig ? impl.codedInterval : impl.interval;
  coded.window = impl.hasCodedConfig ? impl.codedWindow : impl.window;

  uint16_t durationUnits = bleGapDurationMsTo10ms(durationMs);
  // Keep controller filtering disabled. Some controllers key legacy duplicates
  // only by address/event type and suppress a same-length payload update. The
  // host callback filters byte-identical primary and scan-response segments so
  // changed data remains observable while the public duplicate policy holds.
  int rc = ble_gap_ext_disc(
    static_cast<uint8_t>(BLE.getOwnAddressType()), durationUnits, 0, 0, 0, 0, &uncoded, impl.hasCodedConfig ? &coded : NULL, BLEScan::Impl::gapEventHandler,
    context
  );
  if (rc != 0) {
    delete context;
    log_e("ble_gap_ext_disc: rc=%d", rc);
    return BTStatus::Fail;
  }
#else
  struct ble_gap_disc_params params = {};
  params.filter_duplicates = 0;
  params.passive = impl.activeScan ? 0 : 1;
  params.itvl = impl.interval;
  params.window = impl.window;
  params.filter_policy = 0;
  params.limited = 0;

  int rc = ble_gap_disc(
    static_cast<uint8_t>(BLE.getOwnAddressType()), durationMs == 0 ? BLE_HS_FOREVER : (int32_t)durationMs, &params, BLEScan::Impl::gapEventHandler, context
  );
  if (rc != 0) {
    delete context;
    log_e("ble_gap_disc: rc=%d", rc);
    return BTStatus::Fail;
  }
#endif
  impl.activeScanContext = context;
  impl.isScanning = true;
  return BTStatus::OK;
}

void BLEScan::clearDuplicateCache() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.duplicateKeys.clear();
}

static BTStatus nimbleStopScanSession(BLEScan::Impl &impl);

BTStatus BLEScan::start(uint32_t durationMs, bool appendToExistingResults) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLEAdmissionGuard publisher(impl.nativeAdmission);
  if (!publisher) {
    return BTStatus::InvalidState;
  }
  if (!bleGapDurationMsValid(durationMs)) {
    return BTStatus::InvalidParam;
  }
  if (impl.isScanning) {
    BTStatus stopStatus = nimbleStopScanSession(impl);
    if (!stopStatus) {
      return stopStatus;
    }
  }

  if (!appendToExistingResults) {
    BLELockGuard lock(impl.mtx);
    impl.results._devices.clear();
    impl.duplicateKeys.clear();
  }

  {
    BLELockGuard lock(impl.mtx);
    impl.lastDurationMs = durationMs;
    impl.sessionDeadlineUs = durationMs == 0 ? 0 : esp_timer_get_time() + static_cast<int64_t>(durationMs) * 1000;
#if BLE5_SUPPORTED
    impl.hasUncodedConfig = false;
    impl.hasCodedConfig = false;
#endif
    const uint32_t generation = ++impl.activeGeneration;
    log_d("Scan: start duration=%u ms active=%d filterDuplicates=%d", durationMs, impl.activeScan, impl.filterDuplicates);
    impl.scanSync.take();
    BTStatus status = nimbleStartScanSession(impl, durationMs, generation);
    if (status != BTStatus::OK) {
      impl.scanSync.give(status);
      return status;
    }
  }
  return BTStatus::OK;
}

// Blocks until the scan completes or times out; completion is gated by the internal sync object.
BLEScan::Results BLEScan::startBlocking(uint32_t durationMs) {
  BLE_CHECK_IMPL(BLEScan::Results());
  if (start(durationMs, false) != BTStatus::OK) {
    return BLEScan::Results();
  }
  impl.scanSync.wait(durationMs == 0 ? portMAX_DELAY : durationMs + 2000);
  BLELockGuard lock(impl.mtx);
  return impl.results;
}

static BTStatus nimbleStopScanSession(BLEScan::Impl &impl) {
  BLEScan::Impl::EventContext *context = nullptr;
  uint32_t previousGeneration = 0;
  {
    BLELockGuard lock(impl.mtx);
    if (!impl.isScanning) {
      return BTStatus::OK;
    }
    log_d("Scan: stop");
    previousGeneration = impl.activeGeneration;
    ++impl.activeGeneration;
    impl.isScanning = false;
    context = impl.activeScanContext;
    if (context) {
      context->retireByStop = true;
    }
  }
  int rc;
#if BLE_VALIDATION_HOOKS
  if (BLEHooks.scan.cancelFailures.exchange(0, std::memory_order_relaxed) > 0) {
    rc = BLE_HS_EINVAL;
  } else if (BLEHooks.scan.forcePreemptedStop.exchange(false, std::memory_order_relaxed)) {
    // Model NimBLE's privacy-preemption window: the native operation retains
    // its callback argument and completes later, while cancel sees no active
    // discovery procedure.
    rc = BLE_HS_EALREADY;
  } else
#endif
  {
    rc = ble_gap_disc_cancel();
  }
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    bool deleteNow = false;
    {
      BLELockGuard lock(impl.mtx);
      if (impl.activeScanContext == context) {
        impl.activeGeneration = previousGeneration;
        impl.isScanning = true;
        if (context) {
          context->retireByStop = false;
        }
      } else {
        // A completion callback ran while cancel returned the error. It left
        // reclamation to us because retireByStop was already set.
        deleteNow = context != nullptr;
      }
    }
    if (deleteNow) {
      delete context;
    }
    log_e("Scan: ble_gap_disc_cancel rc=%d", rc);
    return BTStatus::Fail;
  }
  if (rc == BLE_HS_EALREADY && context) {
    bool completionAlreadyRan = false;
    {
      BLELockGuard lock(impl.mtx);
      if (impl.activeScanContext == context) {
        // Privacy preemption temporarily removes the operation from the active
        // GAP slot. Native code still owns this callback argument and will
        // deliver DISC_COMPLETE after preemption ends.
        impl.activeScanContext = nullptr;
        context->retireByStop = false;
        impl.nativeHeldScanContexts.push_back(context);
#if BLE_VALIDATION_HOOKS
        BLEHooks.scan.heldContexts.fetch_add(1, std::memory_order_relaxed);
#endif
      } else {
        // DISC_COMPLETE ran after cancel() returned but before this lock. It
        // observed retireByStop and deliberately left reclamation to stop().
        completionAlreadyRan = true;
      }
    }
    if (completionAlreadyRan) {
      delete context;
    }
    impl.scanSync.give(BTStatus::OK);
    return BTStatus::OK;
  }
  {
    BLELockGuard lock(impl.mtx);
    if (impl.activeScanContext == context) {
      impl.activeScanContext = nullptr;
    }
  }
  retireScanContext(context);
  impl.scanSync.give(BTStatus::OK);
  return BTStatus::OK;
}

BTStatus BLEScan::stop() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  // Publish this caller before it acquires any context pointer. Shutdown closes
  // the gate then waits for this count, while a caller that lost the gate race
  // backs out without touching native-owned state.
  BLEAdmissionGuard publisher(impl.nativeAdmission);
  if (!publisher) {
    return BTStatus::OK;
  }
  return nimbleStopScanSession(impl);
}

BTStatus BLEScan::startExtended(uint32_t durationMs, const ExtScanConfig *codedConfig, const ExtScanConfig *uncodedConfig) {
#if BLE5_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLEAdmissionGuard publisher(impl.nativeAdmission);
  if (!publisher) {
    return BTStatus::InvalidState;
  }
  if (!bleGapDurationMsValid(durationMs)) {
    return BTStatus::InvalidParam;
  }
  if (impl.isScanning) {
    BTStatus stopStatus = nimbleStopScanSession(impl);
    if (!stopStatus) {
      return stopStatus;
    }
  }
  {
    BLELockGuard lock(impl.mtx);
    impl.results._devices.clear();
    impl.duplicateKeys.clear();
  }

  // ExtScanConfig interval/window are in milliseconds (matching setInterval/setWindow);
  // impl.interval/impl.window are already stored in 0.625 ms controller units.
  {
    BLELockGuard lock(impl.mtx);
    impl.lastDurationMs = durationMs;
    impl.sessionDeadlineUs = durationMs == 0 ? 0 : esp_timer_get_time() + static_cast<int64_t>(durationMs) * 1000;
    impl.hasUncodedConfig = (uncodedConfig != nullptr);
    if (uncodedConfig) {
      impl.uncodedInterval = bleMsToUnits0625(uncodedConfig->interval);
      impl.uncodedWindow = bleMsToUnits0625(uncodedConfig->window);
    }
    impl.hasCodedConfig = (codedConfig != nullptr);
    if (codedConfig) {
      impl.codedInterval = bleMsToUnits0625(codedConfig->interval);
      impl.codedWindow = bleMsToUnits0625(codedConfig->window);
    }
    const uint32_t generation = ++impl.activeGeneration;
    return nimbleStartScanSession(impl, durationMs, generation);
  }
#else
  log_w("Scan: startExtended not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

// Delegates to stop() on NimBLE.
BTStatus BLEScan::stopExtended() {
  return stop();
}

BTStatus BLEScan::createPeriodicSync(const BTAddress &addr, uint8_t sid, uint16_t skipCount, uint16_t timeoutMs) {
#if BLE5_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLEAdmissionGuard publisher(impl.nativeAdmission);
  if (!publisher) {
    return BTStatus::InvalidState;
  }

  struct ble_gap_periodic_sync_params params = {};
  params.skip = skipCount;
  params.sync_timeout = timeoutMs / 10;

  ble_addr_t bleAddr;
  bleAddr.type = static_cast<uint8_t>(addr.type());
  memcpy(bleAddr.val, addr.data(), 6);

  int rc = ble_gap_periodic_adv_sync_create(&bleAddr, sid, &params, BLEScan::Impl::gapEventHandler, &impl.periodicContext);
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

BTStatus BLEScan::cancelPeriodicSync() {
#if BLE5_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLEAdmissionGuard publisher(impl.nativeAdmission);
  if (!publisher) {
    return BTStatus::InvalidState;
  }
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

BTStatus BLEScan::terminatePeriodicSync(uint16_t syncHandle) {
#if BLE5_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BLEAdmissionGuard publisher(impl.nativeAdmission);
  if (!publisher) {
    return BTStatus::InvalidState;
  }
  int rc = ble_gap_periodic_adv_sync_terminate(syncHandle);
  if (rc != 0) {
    log_e("Scan: terminatePeriodicSync handle=%u rc=%d", syncHandle, rc);
    return BTStatus::Fail;
  }
  if (_impl) {
    BLELockGuard lock(_impl->mtx);
    auto &v = _impl->periodicSyncs;
    v.erase(std::remove(v.begin(), v.end(), syncHandle), v.end());
  }
  return BTStatus::OK;
#else
  log_w("Scan: terminatePeriodicSync not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

// Internal teardown hook (see BLEScan.h). Runs from BLEClass::end() while the
// NimBLE host is still enabled, so terminating here avoids the benign
// BLE_HS_EDISABLED race that occurs if a sync is left active across host stop.
void BLEScan::terminateAllPeriodicSyncs() {
#if BLE5_SUPPORTED
  if (!_impl) {
    return;
  }
  std::vector<uint16_t> handles;
  {
    BLELockGuard lock(_impl->mtx);
    handles.swap(_impl->periodicSyncs);
  }
  for (uint16_t h : handles) {
    ble_gap_periodic_adv_sync_terminate(h);
  }
#endif
}

void BLEScan::resetBackendEvents() {
  if (!_impl) {
    return;
  }
  std::vector<BLEScan::Impl::EventContext *> retired;
  {
    BLELockGuard lock(_impl->mtx);
    if (_impl->retireScanEventInited) {
      if (ble_npl_event_is_queued(&_impl->retireScanEvent)) {
        ble_npl_eventq_remove(nimble_port_get_dflt_eventq(), &_impl->retireScanEvent);
      }
      ble_npl_event_deinit(&_impl->retireScanEvent);
      _impl->retireScanEventInited = false;
    }
    retired.swap(_impl->retiredScanContexts);
#if BLE_VALIDATION_HOOKS
    BLEHooks.scan.retiredContexts.fetch_sub(static_cast<int>(retired.size()), std::memory_order_relaxed);
#endif
    retired.insert(retired.end(), _impl->nativeHeldScanContexts.begin(), _impl->nativeHeldScanContexts.end());
#if BLE_VALIDATION_HOOKS
    BLEHooks.scan.heldContexts.fetch_sub(static_cast<int>(_impl->nativeHeldScanContexts.size()), std::memory_order_relaxed);
#endif
    _impl->nativeHeldScanContexts.clear();
    if (_impl->activeScanContext) {
      retired.push_back(_impl->activeScanContext);
      _impl->activeScanContext = nullptr;
    }
  }
  for (auto *context : retired) {
    delete context;
  }
}

void BLEScan::prepareBackendStart() {
  if (_impl) {
    _impl->nativeAdmission.reopen();
  }
}

void BLEScan::beginBackendShutdown() {
  if (!_impl) {
    return;
  }
  _impl->nativeAdmission.closeAndWait();
  // Public stop() is closed by the gate above, so cancel directly while the
  // host is still running. This also releases an infinite startBlocking(0)
  // waiter before nimble_port_stop() makes further completion impossible.
  BTStatus stopStatus = nimbleStopScanSession(*_impl);
  if (!stopStatus) {
    {
      BLELockGuard lock(_impl->mtx);
      ++_impl->activeGeneration;
      _impl->isScanning = false;
    }
    _impl->scanSync.give(stopStatus);
  }
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

BLEScan::Results BLEScan::startBlocking(uint32_t) {
  log_w("Scanning not supported");
  return BLEScan::Results();
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

void BLEScan::terminateAllPeriodicSyncs() {}

void BLEScan::resetBackendEvents() {}

void BLEScan::prepareBackendStart() {}

void BLEScan::beginBackendShutdown() {}

#endif /* BLE_SCANNING_SUPPORTED */

#endif /* BLE_NIMBLE */
