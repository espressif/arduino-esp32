/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
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
 * @file BluedroidScan.cpp
 * @brief Bluedroid (ESP-IDF) implementation of BLE GAP scan and result handling.
 */

#include "impl/common/BLEGuards.h"
#if BLE_BLUEDROID

#if BLE_SCANNING_SUPPORTED

#include "BLE.h"

#include "BluedroidScan.h"
#include "impl/common/BLEAdvertisedDeviceImpl.h"
#include "impl/common/BLEScanImpl.h"
#include "impl/common/BLEImplHelpers.h"
#include "impl/common/BLEAdvScanHelpers.h"
#include "esp32-hal-log.h"

#include <esp_gap_ble_api.h>
#include <algorithm>
#include <vector>

// API contract is documented on the declarations in the public BLE*.h headers; the definitions below carry implementation notes only.

BLEScan::Impl *BLEScan::Impl::s_instance = nullptr;

// --------------------------------------------------------------------------
// Configuration
// --------------------------------------------------------------------------

void BLEScan::clearDuplicateCache() {
#ifdef ESP_BLE_DUPLICATE_SCAN_EXCEPTIONAL_LIST_CLEAN_ALL
  esp_ble_gap_clean_duplicate_scan_exceptional_list(ESP_BLE_DUPLICATE_SCAN_EXCEPTIONAL_LIST_CLEAN_ALL, NULL, 0);
#endif
}

// --------------------------------------------------------------------------
// Start / stop
// --------------------------------------------------------------------------

BTStatus BLEScan::start(uint32_t durationMs, bool appendToExistingResults) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);

  if (!appendToExistingResults) {
    BLELockGuard lock(impl.mtx);
    impl.results = BLEScan::Results();
  }

#if BLE5_SUPPORTED
  // On BLE5-capable Bluedroid legacy results are delivered as
  // ESP_GAP_BLE_EXT_ADV_REPORT_EVT, so drive the extended scan API and let the
  // report handler merge scan responses. NOTE: not buildable on today's esp32
  // Bluedroid libs (BLE5 off); not yet HW-tested.
  esp_ble_ext_scan_params_t extParams = {};
  extParams.own_addr_type = static_cast<esp_ble_addr_type_t>(BLE.getOwnAddressType());
  extParams.filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
  extParams.scan_duplicate = impl.filterDuplicates ? BLE_SCAN_DUPLICATE_ENABLE : BLE_SCAN_DUPLICATE_DISABLE;
  extParams.cfg_mask = ESP_BLE_GAP_EXT_SCAN_CFG_UNCODE_MASK;
  extParams.uncoded_cfg.scan_type = impl.activeScan ? BLE_SCAN_TYPE_ACTIVE : BLE_SCAN_TYPE_PASSIVE;
  extParams.uncoded_cfg.scan_interval = impl.interval;
  extParams.uncoded_cfg.scan_window = impl.window;

  impl.scanSync.take();
  esp_err_t err = esp_ble_gap_set_ext_scan_params(&extParams);
  if (err != ESP_OK) {
    impl.scanSync.give(BTStatus::Fail);
    log_e("esp_ble_gap_set_ext_scan_params: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  BTStatus rc = impl.scanSync.wait(5000);
  if (rc != BTStatus::OK) {
    log_e("Ext scan param set did not complete (status=%d)", static_cast<int>(rc.code()));
    return rc;
  }

  // Extended scan duration is in 10 ms units; 0 == scan until stopped.
  uint32_t durationUnits = (durationMs == 0) ? 0 : (durationMs / 10);
  err = esp_ble_gap_start_ext_scan(durationUnits, 0);
  if (err != ESP_OK) {
    log_e("esp_ble_gap_start_ext_scan: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
#else
  // Configure scan parameters
  esp_ble_scan_params_t scanParams = {};
  scanParams.scan_type = impl.activeScan ? BLE_SCAN_TYPE_ACTIVE : BLE_SCAN_TYPE_PASSIVE;
  scanParams.own_addr_type = static_cast<esp_ble_addr_type_t>(BLE.getOwnAddressType());
  scanParams.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
  scanParams.scan_interval = impl.interval;
  scanParams.scan_window = impl.window;
  scanParams.scan_duplicate = impl.filterDuplicates ? BLE_SCAN_DUPLICATE_ENABLE : BLE_SCAN_DUPLICATE_DISABLE;

  // Set scan params and wait for completion event
  impl.scanSync.take();
  esp_err_t err = esp_ble_gap_set_scan_params(&scanParams);
  if (err != ESP_OK) {
    impl.scanSync.give(BTStatus::Fail);
    log_e("esp_ble_gap_set_scan_params: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }

  BTStatus rc = impl.scanSync.wait(5000);
  if (rc != BTStatus::OK) {
    log_e("Scan param set did not complete (status=%d)", static_cast<int>(rc.code()));
    return rc;
  }

  // Bluedroid uses seconds; convert ms -> s, 0 means infinite
  uint32_t durationSec = durationMs / 1000;

  err = esp_ble_gap_start_scanning(durationSec);
  if (err != ESP_OK) {
    log_e("esp_ble_gap_start_scanning: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }

  return BTStatus::OK;
#endif /* BLE5_SUPPORTED */
}

BLEScan::Results BLEScan::startBlocking(uint32_t durationMs) {
  BLE_CHECK_IMPL(BLEScan::Results());

  BTStatus rc = start(durationMs, false);
  if (rc != BTStatus::OK) {
    log_e("Scan: startBlocking failed to start scan");
    return BLEScan::Results();
  }

  // start() already armed scanSync (took it and consumed the param-set
  // completion), so the semaphore is drained and ready. Wait directly for the
  // scan-completion (INQ_CMPL_EVT) give(); re-taking here would race with an
  // early completion and drop the signal.
  uint32_t waitMs = (durationMs == 0) ? portMAX_DELAY : durationMs + 2000;
  BTStatus waitSt = impl.scanSync.wait(waitMs);
  if (waitSt != BTStatus::OK) {
    log_w("Scan: startBlocking wait timed out");
  }

  BLELockGuard lock(impl.mtx);
  return impl.results;
}

BTStatus BLEScan::stop() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
#if BLE5_SUPPORTED
  esp_err_t err = esp_ble_gap_stop_ext_scan();
#else
  esp_err_t err = esp_ble_gap_stop_scanning();
#endif
  if (err != ESP_OK) {
    log_e("stop scanning: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  impl.isScanning = false;
  return BTStatus::OK;
}

// --------------------------------------------------------------------------
// Extended / Periodic
// --------------------------------------------------------------------------

BTStatus BLEScan::startExtended(uint32_t durationMs, const ExtScanConfig *codedConfig, const ExtScanConfig *uncodedConfig) {
#if BLE5_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  {
    BLELockGuard lock(impl.mtx);
    impl.results = BLEScan::Results();
  }

  esp_ble_ext_scan_params_t extParams = {};
  extParams.own_addr_type = static_cast<esp_ble_addr_type_t>(BLE.getOwnAddressType());
  extParams.filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
  extParams.scan_duplicate = impl.filterDuplicates ? BLE_SCAN_DUPLICATE_ENABLE : BLE_SCAN_DUPLICATE_DISABLE;
  esp_ble_scan_type_t type = impl.activeScan ? BLE_SCAN_TYPE_ACTIVE : BLE_SCAN_TYPE_PASSIVE;
  // ExtScanConfig interval/window are in milliseconds; impl.interval/window are
  // already in 0.625 ms controller units.
  extParams.cfg_mask = ESP_BLE_GAP_EXT_SCAN_CFG_UNCODE_MASK;
  extParams.uncoded_cfg.scan_type = type;
  extParams.uncoded_cfg.scan_interval = uncodedConfig ? bleMsToUnits0625(uncodedConfig->interval) : impl.interval;
  extParams.uncoded_cfg.scan_window = uncodedConfig ? bleMsToUnits0625(uncodedConfig->window) : impl.window;
  if (codedConfig) {
    extParams.cfg_mask |= ESP_BLE_GAP_EXT_SCAN_CFG_CODE_MASK;
    extParams.coded_cfg.scan_type = type;
    extParams.coded_cfg.scan_interval = bleMsToUnits0625(codedConfig->interval);
    extParams.coded_cfg.scan_window = bleMsToUnits0625(codedConfig->window);
  }

  impl.scanSync.take();
  esp_err_t err = esp_ble_gap_set_ext_scan_params(&extParams);
  if (err != ESP_OK) {
    impl.scanSync.give(BTStatus::Fail);
    log_e("esp_ble_gap_set_ext_scan_params: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  BTStatus rc = impl.scanSync.wait(5000);
  if (rc != BTStatus::OK) {
    return rc;
  }

  uint32_t durationUnits = (durationMs == 0) ? 0 : (durationMs / 10);
  err = esp_ble_gap_start_ext_scan(durationUnits, 0);
  if (err != ESP_OK) {
    log_e("esp_ble_gap_start_ext_scan: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
#else
  (void)durationMs;
  (void)codedConfig;
  (void)uncodedConfig;
  log_w("%s not supported on this Bluedroid build (BLE 5.0 unavailable)", __func__);
  return BTStatus::NotSupported;
#endif
}

BTStatus BLEScan::stopExtended() {
  return stop();
}

BTStatus BLEScan::createPeriodicSync(const BTAddress &addr, uint8_t sid, uint16_t skipCount, uint16_t timeoutMs) {
#if BLE_PERIODIC_ADV_SUPPORTED
  // Null-check only: the create_sync call does not touch Impl state (unlike NimBLE,
  // which passes &impl as the GAP callback context). Avoid BLE_CHECK_IMPL here —
  // it would leave an unused `impl` reference under -Werror=unused-variable.
  if (!_impl) {
    return BTStatus::InvalidState;
  }

  esp_ble_gap_periodic_adv_sync_params_t params = {};
  params.filter_policy = 0;  // Use SID + address
  params.sid = sid;
  params.addr_type = static_cast<esp_ble_addr_type_t>(addr.type());
  addr.toEspBdAddr(params.addr);
  params.skip = skipCount;
  params.sync_timeout = timeoutMs / 10;  // units of 10 ms

  esp_err_t err = esp_ble_gap_periodic_adv_create_sync(&params);
  if (err != ESP_OK) {
    log_e("esp_ble_gap_periodic_adv_create_sync: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
#else
  (void)addr;
  (void)sid;
  (void)skipCount;
  (void)timeoutMs;
  log_w("%s not supported on Bluedroid (periodic adv unavailable)", __func__);
  return BTStatus::NotSupported;
#endif
}

BTStatus BLEScan::cancelPeriodicSync() {
#if BLE_PERIODIC_ADV_SUPPORTED
  esp_err_t err = esp_ble_gap_periodic_adv_sync_cancel();
  if (err != ESP_OK) {
    log_e("esp_ble_gap_periodic_adv_sync_cancel: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
#else
  log_w("%s not supported on Bluedroid (periodic adv unavailable)", __func__);
  return BTStatus::NotSupported;
#endif
}

BTStatus BLEScan::terminatePeriodicSync(uint16_t syncHandle) {
#if BLE_PERIODIC_ADV_SUPPORTED
  esp_err_t err = esp_ble_gap_periodic_adv_sync_terminate(syncHandle);
  if (err != ESP_OK) {
    log_e("esp_ble_gap_periodic_adv_sync_terminate handle=%u: %s", syncHandle, esp_err_to_name(err));
    return BTStatus::Fail;
  }
  if (_impl) {
    BLELockGuard lock(_impl->mtx);
    auto &v = _impl->periodicSyncs;
    v.erase(std::remove(v.begin(), v.end(), syncHandle), v.end());
  }
  return BTStatus::OK;
#else
  (void)syncHandle;
  log_w("%s not supported on Bluedroid (periodic adv unavailable)", __func__);
  return BTStatus::NotSupported;
#endif
}

// Internal teardown hook (see BLEScan.h). Runs from BLEClass::end() while the
// Bluedroid host is still enabled.
void BLEScan::terminateAllPeriodicSyncs() {
#if BLE_PERIODIC_ADV_SUPPORTED
  if (!_impl) {
    return;
  }
  std::vector<uint16_t> handles;
  {
    BLELockGuard lock(_impl->mtx);
    handles.swap(_impl->periodicSyncs);
  }
  for (uint16_t h : handles) {
    esp_ble_gap_periodic_adv_sync_terminate(h);
  }
#endif
}

// --------------------------------------------------------------------------
// GAP event handler (called from BluedroidCore gapCallback)
// --------------------------------------------------------------------------

/**
 * @brief Handles Bluedroid GAP scan-related callbacks for the singleton scan instance.
 * @param event GAP event type.
 * @param param Event parameters from the stack.
 */
void BLEScan::Impl::handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  auto *inst = s_instance;
  if (!inst) {
    return;
  }

  switch (event) {

    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
      log_d("SCAN_PARAM_SET_COMPLETE status=%d", param->scan_param_cmpl.status);
      inst->scanSync.give(param->scan_param_cmpl.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
      log_d("SCAN_START_COMPLETE status=%d", param->scan_start_cmpl.status);
      if (param->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
        inst->isScanning = true;
      }
      break;

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
      log_d("SCAN_STOP_COMPLETE status=%d", param->scan_stop_cmpl.status);
      inst->isScanning = false;
      break;

    case ESP_GAP_BLE_SCAN_RESULT_EVT:
    {
      switch (param->scan_rst.search_evt) {

        case ESP_GAP_SEARCH_INQ_RES_EVT:
        {
          if (!inst->isScanning) {
            break;
          }

          // Build advertised device from scan result
          auto devImpl = std::make_shared<BLEAdvertisedDevice::Impl>();
          devImpl->address = BTAddress(param->scan_rst.bda, static_cast<BTAddress::Type>(param->scan_rst.ble_addr_type));
          devImpl->addrType = static_cast<BTAddress::Type>(param->scan_rst.ble_addr_type);
          devImpl->rssi = param->scan_rst.rssi;
          devImpl->hasRSSI = true;
          devImpl->legacy = true;

          switch (param->scan_rst.ble_evt_type) {
            case ESP_BLE_EVT_CONN_ADV:
              devImpl->advType = BLEAdvType::ConnectableScannable;
              devImpl->connectable = true;
              devImpl->scannable = true;
              break;
            case ESP_BLE_EVT_CONN_DIR_ADV:
              devImpl->advType = BLEAdvType::ConnectableDirected;
              devImpl->connectable = true;
              devImpl->directed = true;
              break;
            case ESP_BLE_EVT_DISC_ADV:
              devImpl->advType = BLEAdvType::ScannableUndirected;
              devImpl->scannable = true;
              break;
            case ESP_BLE_EVT_NON_CONN_ADV: devImpl->advType = BLEAdvType::NonConnectable; break;
            default:                       break;
          }

          devImpl->parsePayload(param->scan_rst.ble_adv, param->scan_rst.adv_data_len + param->scan_rst.scan_rsp_len);

          BLEAdvertisedDevice dev = BLEScanImplCommon::makeAdvertisedDeviceHandle(devImpl);

          // Snapshot callbacks under lock, invoke outside
          {
            BLEScan::ResultHandler resultCb;
            bool isDuplicate = false;
            {
              BLELockGuard lock(inst->mtx);

              // Match appendOrReplace and the extended path: distinct advertising
              // sets from one address (different SIDs) are not duplicates. Legacy
              // PDUs carry SID 0xFF, so this stays address-based among legacy
              // entries while not colliding with extended sets from the same peer.
              if (inst->filterDuplicates) {
                for (const auto &existing : inst->results) {
                  if (existing.getAddress() == dev.getAddress() && existing.getAdvSID() == dev.getAdvSID()) {
                    isDuplicate = true;
                    break;
                  }
                }
              }

              if (!isDuplicate) {
                inst->results.appendOrReplace(dev);
                resultCb = inst->onResultCb;
              }
            }

            if (!isDuplicate) {
              if (resultCb) {
                resultCb(dev);
              }
            }
          }
          break;
        }

        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
        {
          log_d("SCAN_INQ_CMPL_EVT");
          inst->isScanning = false;

          BLEScan::CompleteHandler completeCb;
          BLEScan::Results resultsCopy;
          {
            BLELockGuard lock(inst->mtx);
            completeCb = inst->onCompleteCb;
            resultsCopy = inst->results;
          }

          if (completeCb) {
            completeCb(resultsCopy);
          }

          inst->scanSync.give();
          break;
        }

        default: break;
      }
      break;
    }

#if BLE5_SUPPORTED
    // --- BLE5 extended scan (see BLEScan::start ext path) ---
    case ESP_GAP_BLE_SET_EXT_SCAN_PARAMS_COMPLETE_EVT:
      log_d("SET_EXT_SCAN_PARAMS_COMPLETE status=%d", param->set_ext_scan_params.status);
      inst->scanSync.give(param->set_ext_scan_params.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_EXT_SCAN_START_COMPLETE_EVT:
      log_d("EXT_SCAN_START_COMPLETE status=%d", param->ext_scan_start.status);
      if (param->ext_scan_start.status == ESP_BT_STATUS_SUCCESS) {
        inst->isScanning = true;
      }
      break;

    case ESP_GAP_BLE_EXT_SCAN_STOP_COMPLETE_EVT:
      log_d("EXT_SCAN_STOP_COMPLETE status=%d", param->ext_scan_stop.status);
      inst->isScanning = false;
      inst->scanSync.give(param->ext_scan_stop.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_EXT_ADV_REPORT_EVT:
    {
      if (!inst->isScanning) {
        break;
      }
      const esp_ble_gap_ext_adv_report_t &r = param->ext_adv_report.params;
      BTAddress addr(r.addr, static_cast<BTAddress::Type>(r.addr_type));

      // Merge scan responses (which carry the Local Name) into the device from
      // the preceding advertisement.
      const bool isScanRsp = (r.event_type & ESP_BLE_ADV_REPORT_EXT_SCAN_RSP) != 0;
      if (isScanRsp) {
        BLEAdvertisedDevice merged;
        bool found = false;
        BLEScan::ResultHandler cb;
        {
          BLELockGuard lock(inst->mtx);
          found = BLEScanImplCommon::mergeScanResponse(inst->results, addr, r.sid, r.adv_data, r.adv_data_len, r.rssi, merged);
          cb = inst->onResultCb;
        }
        if (found) {
          if (cb) {
            cb(merged);
          }
          break;
        }
        // No prior advertisement: fall through and surface the scan response as
        // a standalone device so its name is not lost.
      }

      auto devImpl = std::make_shared<BLEAdvertisedDevice::Impl>();
      devImpl->address = addr;
      devImpl->addrType = static_cast<BTAddress::Type>(r.addr_type);
      devImpl->rssi = r.rssi;
      devImpl->hasRSSI = true;
      const uint8_t et = r.event_type;
      const bool legacyReport =
        (et == ESP_BLE_LEGACY_ADV_TYPE_IND || et == ESP_BLE_LEGACY_ADV_TYPE_DIRECT_IND || et == ESP_BLE_LEGACY_ADV_TYPE_SCAN_IND
         || et == ESP_BLE_LEGACY_ADV_TYPE_NONCON_IND || et == ESP_BLE_LEGACY_ADV_TYPE_SCAN_RSP_TO_ADV_IND || et == ESP_BLE_LEGACY_ADV_TYPE_SCAN_RSP_TO_ADV_SCAN_IND);
      devImpl->legacy = legacyReport;
      devImpl->connectable = (et & ESP_BLE_ADV_REPORT_EXT_ADV_IND) != 0 || et == ESP_BLE_LEGACY_ADV_TYPE_IND || et == ESP_BLE_LEGACY_ADV_TYPE_DIRECT_IND;
      devImpl->scannable = (et & ESP_BLE_ADV_REPORT_EXT_SCAN_IND) != 0 || et == ESP_BLE_LEGACY_ADV_TYPE_SCAN_IND;
      devImpl->primaryPhy = static_cast<BLEPhy>(r.primary_phy);
      devImpl->sid = r.sid;
      devImpl->periodicInterval = r.per_adv_interval;
      devImpl->parsePayload(r.adv_data, r.adv_data_len);

      BLEAdvertisedDevice dev = BLEScanImplCommon::makeAdvertisedDeviceHandle(devImpl);
      BLEScan::ResultHandler resultCb;
      bool isDuplicate = false;
      {
        BLELockGuard lock(inst->mtx);
        if (inst->filterDuplicates) {
          // Match appendOrReplace: distinct advertising sets from one address
          // (different SIDs) are not duplicates of each other.
          for (const auto &existing : inst->results) {
            if (existing.getAddress() == dev.getAddress() && existing.getAdvSID() == dev.getAdvSID()) {
              isDuplicate = true;
              break;
            }
          }
        }
        if (!isDuplicate) {
          inst->results.appendOrReplace(dev);
          resultCb = inst->onResultCb;
        }
      }
      if (!isDuplicate && resultCb) {
        resultCb(dev);
      }
      break;
    }

#if BLE_PERIODIC_ADV_SUPPORTED
    case ESP_GAP_BLE_PERIODIC_ADV_SYNC_ESTAB_EVT:
    {
      const auto &e = param->periodic_adv_sync_estab;
      if (e.status != ESP_BT_STATUS_SUCCESS) {
        log_w("PERIODIC_ADV_SYNC_ESTAB failed status=%d", e.status);
        break;
      }
      {
        BLELockGuard lock(inst->mtx);
        inst->periodicSyncs.push_back(e.sync_handle);
      }
      BLEScan::PeriodicSyncHandler cb;
      {
        BLELockGuard lock(inst->mtx);
        cb = inst->periodicSyncCb;
      }
      if (cb) {
        BTAddress addr(e.adv_addr, static_cast<BTAddress::Type>(e.adv_addr_type));
        cb(e.sync_handle, e.sid, addr, static_cast<BLEPhy>(e.adv_phy), e.period_adv_interval);
      }
      break;
    }

    case ESP_GAP_BLE_PERIODIC_ADV_REPORT_EVT:
    {
      const auto &r = param->period_adv_report.params;
      BLEScan::PeriodicReportHandler cb;
      {
        BLELockGuard lock(inst->mtx);
        cb = inst->periodicReportCb;
      }
      if (cb) {
        cb(r.sync_handle, r.rssi, static_cast<int8_t>(r.tx_power), r.data, r.data_length);
      }
      break;
    }

    case ESP_GAP_BLE_PERIODIC_ADV_SYNC_LOST_EVT:
    {
      const uint16_t handle = param->periodic_adv_sync_lost.sync_handle;
      {
        BLELockGuard lock(inst->mtx);
        auto &v = inst->periodicSyncs;
        v.erase(std::remove(v.begin(), v.end(), handle), v.end());
      }
      BLEScan::PeriodicLostHandler cb;
      {
        BLELockGuard lock(inst->mtx);
        cb = inst->periodicLostCb;
      }
      if (cb) {
        cb(handle);
      }
      break;
    }
#endif /* BLE_PERIODIC_ADV_SUPPORTED */
#endif /* BLE5_SUPPORTED */

    default: break;
  }
}

#else /* !BLE_SCANNING_SUPPORTED -- stubs */

#include "BLE.h"
#include "esp32-hal-log.h"

// Stubs for BLE_SCANNING_SUPPORTED == 0: log; start* return NotSupported or empty results/scan.

void BLEScan::clearDuplicateCache() {}

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

#endif /* BLE_SCANNING_SUPPORTED */

#endif /* BLE_BLUEDROID */
