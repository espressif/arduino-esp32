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
 * @file BluedroidAdvertising.cpp
 * @brief Bluedroid (ESP-IDF) implementation of BLE GAP advertising, data setup, and GAP events.
 *
 * Spec references:
 *  - BT Core Spec v5.x, Vol 3, Part C (GAP) — discoverable/connectable modes.
 *  - BT Core Spec v5.x, Vol 6, Part B, §2.3.1 — LE advertising PDU types:
 *    ADV_IND=ConnectableScannable, ADV_DIRECT_IND=ConnectableDirected,
 *    ADV_NONCONN_IND=NonConnectable, ADV_SCAN_IND=ScannableUndirected.
 *  - BT Core Spec v5.x, Vol 6, Part B, §4.4.2 — advertising interval timing:
 *    unit = 0.625 ms; minimum = 0x0020 (20 ms); maximum = 0x4000 (10.24 s).
 *  - BT Core Spec v5.x, Vol 6, Part B, §4.3.3 — advertising filter policy
 *    (scan/connect from all vs. from whitelist only).
 *  - CSS (Supplement to Core Spec), Part A — AD type codes and structures.
 */

#include "impl/BLEGuards.h"
#if BLE_BLUEDROID

#if BLE_ADVERTISING_SUPPORTED

#include "BLE.h"
#include "BluedroidAdvertising.h"

#include "impl/BLESync.h"
#include "impl/BLEMutex.h"
#include "impl/BLEImplHelpers.h"
#include "esp32-hal-log.h"

#include <esp_gap_ble_api.h>
#include <esp_bt_defs.h>
#include <string.h>

static const uint32_t ADV_SYNC_TIMEOUT_MS = 2000;

// --------------------------------------------------------------------------
// Static instance for event routing
// --------------------------------------------------------------------------

BLEAdvertising::Impl *BLEAdvertising::Impl::s_instance = nullptr;

// --------------------------------------------------------------------------
// Helpers: map BLEAdvType to Bluedroid esp_ble_adv_type_t
// --------------------------------------------------------------------------

/**
 * @brief Maps the API advertising type to the Bluedroid GAP advertising type constant.
 * @param type High-level BLEAdvType.
 * @return Corresponding esp_ble_adv_type_t.
 * @note PDU type mapping (BT Core Spec v5.x, Vol 6, Part B, §2.3.1):
 *       ADV_IND (ConnectableScannable/Connectable): undirected connectable + scannable.
 *       ADV_DIRECT_IND high duty (DirectedHighDuty): directed connectable, fast advertising.
 *       ADV_DIRECT_IND low duty (DirectedLowDuty/ConnectableDirected): slower directed.
 *       ADV_SCAN_IND (ScannableUndirected): scannable, not connectable.
 *       ADV_NONCONN_IND (NonConnectable): non-connectable, non-scannable.
 */
static esp_ble_adv_type_t mapAdvType(BLEAdvType type) {
  switch (type) {
    case BLEAdvType::ConnectableScannable: return ADV_TYPE_IND;
    case BLEAdvType::Connectable:          return ADV_TYPE_IND;
    case BLEAdvType::ConnectableDirected:  return ADV_TYPE_DIRECT_IND_HIGH;
    case BLEAdvType::DirectedHighDuty:     return ADV_TYPE_DIRECT_IND_HIGH;
    case BLEAdvType::DirectedLowDuty:      return ADV_TYPE_DIRECT_IND_LOW;
    case BLEAdvType::ScannableUndirected:  return ADV_TYPE_SCAN_IND;
    case BLEAdvType::NonConnectable:       return ADV_TYPE_NONCONN_IND;
    default:                               return ADV_TYPE_IND;
  }
}

/**
 * @brief Maps whitelist flags to Bluedroid advertising filter policy.
 * @param scanWL Restrict scan requests to whitelist.
 * @param connectWL Restrict connection requests to whitelist.
 * @return esp_ble_adv_filter_t policy value.
 * @note Filter policy definitions (BT Core Spec v5.x, Vol 6, Part B, §4.3.3):
 *       - All / All: process all scan and connection requests.
 *       - WL / All: scan requests from whitelist only; any connection request.
 *       - All / WL: any scan request; connection requests from whitelist only.
 *       - WL / WL: both scan and connection requests from whitelist only.
 */
static esp_ble_adv_filter_t mapFilterPolicy(bool scanWL, bool connectWL) {
  if (scanWL && connectWL) {
    return ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST;
  }
  if (scanWL) {
    return ADV_FILTER_ALLOW_SCAN_WLST_CON_ANY;
  }
  if (connectWL) {
    return ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST;
  }
  return ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
}

// --------------------------------------------------------------------------
// Pack service UUIDs into a flat byte buffer.
// Bluedroid requires service_uuid_len to be a multiple of 16 (128-bit),
// so every UUID is expanded to its 128-bit form in little-endian order.
// --------------------------------------------------------------------------

/**
 * @brief Packs service UUIDs as 128-bit little-endian octets for Bluedroid adv data.
 * @param uuids List of UUIDs to expand and concatenate.
 * @return Contiguous byte buffer suitable for service_uuid_len in esp_ble_adv_data_t.
 */
static std::vector<uint8_t> packServiceUUIDs(const std::vector<BLEUUID> &uuids) {
  std::vector<uint8_t> buf;
  buf.reserve(uuids.size() * 16);
  for (const auto &uuid : uuids) {
    BLEUUID u128 = uuid.to128();
    const uint8_t *be = u128.data();
    for (int i = 15; i >= 0; i--) {
      buf.push_back(be[i]);
    }
  }
  return buf;
}

// --------------------------------------------------------------------------
// GAP event handler
// --------------------------------------------------------------------------

/**
 * @brief Handles Bluedroid GAP advertising data and start/stop completion events for the singleton instance.
 * @param event GAP event code.
 * @param param Event payload from the stack.
 */
void BLEAdvertising::Impl::handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  auto *inst = s_instance;
  if (!inst) {
    return;
  }

  switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
      log_d("ADV_DATA_SET_COMPLETE status=%d", param->adv_data_cmpl.status);
      inst->advSync.give(param->adv_data_cmpl.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
      log_d("SCAN_RSP_DATA_SET_COMPLETE status=%d", param->scan_rsp_data_cmpl.status);
      inst->advSync.give(param->scan_rsp_data_cmpl.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
      log_d("ADV_DATA_RAW_SET_COMPLETE status=%d", param->adv_data_raw_cmpl.status);
      inst->advSync.give(param->adv_data_raw_cmpl.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
      log_d("SCAN_RSP_DATA_RAW_SET_COMPLETE status=%d", param->scan_rsp_data_raw_cmpl.status);
      inst->advSync.give(param->scan_rsp_data_raw_cmpl.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
      log_d("ADV_START_COMPLETE status=%d", param->adv_start_cmpl.status);
      if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
        inst->advertising = true;
      }
      inst->advSync.give(param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
    {
      log_d("ADV_STOP_COMPLETE status=%d", param->adv_stop_cmpl.status);
      inst->advertising = false;
      inst->advSync.give(param->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      BLEAdvertising::CompleteHandler completeCb;
      {
        BLELockGuard lock(inst->mtx);
        completeCb = inst->onCompleteCb;
      }
      if (completeCb) {
        completeCb(0);
      }
      break;
    }

    default: break;
  }
}

// --------------------------------------------------------------------------
// Configuration setters
// --------------------------------------------------------------------------

/**
 * @brief Sets the GAP device name and stores it in the implementation for later adv building.
 * @param name New device name string.
 */
void BLEAdvertising::setName(const String &name) {
  BLE_CHECK_IMPL();
  impl.name = name;
  if (name.length() > 0) {
    esp_ble_gap_set_device_name(name.c_str());
  }
}

/**
 * @brief Enables or disables building a separate scan response payload.
 * @param enable True to configure scan response data when starting.
 */
void BLEAdvertising::setScanResponse(bool enable) {
  BLE_CHECK_IMPL();
  impl.scanResp = enable;
}

/**
 * @brief Sets the GAP advertising PDU type used when starting.
 * @param type Connectable, directed, non-connectable, etc.
 */
void BLEAdvertising::setType(BLEAdvType type) {
  BLE_CHECK_IMPL();
  impl.advType = type;
}

/**
 * @brief Sets min/max advertising interval in milliseconds (converted to 0.625 ms units).
 * @param minMs Minimum interval in ms.
 * @param maxMs Maximum interval in ms.
 */
void BLEAdvertising::setInterval(uint16_t minMs, uint16_t maxMs) {
  BLE_CHECK_IMPL();
  // Convert ms to 0.625ms units: val = ms / 0.625 = ms * 8 / 5
  impl.minInterval = (uint16_t)((uint32_t)minMs * 8 / 5);
  impl.maxInterval = (uint16_t)((uint32_t)maxMs * 8 / 5);
}

/**
 * @brief Sets the preferred min connection interval field in structured adv/scan data.
 * @param val Value in the same units as esp_ble_adv_data_t.min_interval.
 */
void BLEAdvertising::setMinPreferred(uint16_t val) {
  BLE_CHECK_IMPL();
  impl.minPreferred = val;
}

/**
 * @brief Sets the preferred max connection interval field in structured adv/scan data.
 * @param val Value in the same units as esp_ble_adv_data_t.max_interval.
 */
void BLEAdvertising::setMaxPreferred(uint16_t val) {
  BLE_CHECK_IMPL();
  impl.maxPreferred = val;
}

/**
 * @brief Whether to report TX power in configured adv/scan data.
 * @param include True to include radio TX power in AD bytes.
 */
void BLEAdvertising::setTxPower(bool include) {
  BLE_CHECK_IMPL();
  impl.includeTxPower = include;
}

/**
 * @brief Sets the GAP appearance in configured advertising data.
 * @param appearance 16-bit appearance value.
 */
void BLEAdvertising::setAppearance(uint16_t appearance) {
  BLE_CHECK_IMPL();
  impl.appearance = appearance;
}

/**
 * @brief Sets whitelist options used when building esp_ble_adv_params_t filter policy.
 * @param scanRequestWhitelistOnly If true, only whitelisted devices may send scan requests.
 * @param connectWhitelistOnly If true, only whitelisted centrals may connect.
 */
void BLEAdvertising::setScanFilter(bool scanRequestWhitelistOnly, bool connectWhitelistOnly) {
  BLE_CHECK_IMPL();
  impl.scanRequestWhitelistOnly = scanRequestWhitelistOnly;
  impl.connectWhitelistOnly = connectWhitelistOnly;
}

/**
 * @brief Stops advertising if active, clears UUIDs and custom buffers, and restores GAP name when needed.
 */
void BLEAdvertising::reset() {
  BLE_CHECK_IMPL();
  if (impl.advertising) {
    stop();
  }
  impl.serviceUUIDs.clear();
  if (impl.name.length() > 0) {
    impl.name = "";
    esp_ble_gap_set_device_name(BLE.getDeviceName().c_str());
  }
  impl.includeName = true;
  impl.scanResp = true;
  impl.advType = BLEAdvType::ConnectableScannable;
  impl.minInterval = 0x20;
  impl.maxInterval = 0x40;
  impl.minPreferred = 0;
  impl.maxPreferred = 0;
  impl.includeTxPower = false;
  impl.appearance = 0;
  impl.scanRequestWhitelistOnly = false;
  impl.connectWhitelistOnly = false;
  impl.customAdvData = false;
  impl.customScanRespData = false;
  impl.rawAdvData.clear();
  impl.rawScanRespData.clear();
}

// --------------------------------------------------------------------------
// Custom advertisement / scan response data
// --------------------------------------------------------------------------

/**
 * @brief Sets raw custom advertising octets; enables raw adv mode.
 * @param data Encoded AD data bytes.
 */
void BLEAdvertising::setAdvertisementData(const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL();
  impl.rawAdvData.assign(data.data(), data.data() + data.length());
  impl.customAdvData = true;
}

/**
 * @brief Sets raw custom scan response octets; enables custom scan response mode.
 * @param data Encoded scan response data bytes.
 */
void BLEAdvertising::setScanResponseData(const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL();
  impl.rawScanRespData.assign(data.data(), data.data() + data.length());
  impl.customScanRespData = true;
}

// --------------------------------------------------------------------------
// start()
// --------------------------------------------------------------------------

/**
 * @brief Configures advertising (and optionally scan response) data, then starts GAP advertising under a mutex.
 * @param durationMs Reserved for duration-limited advertising; current Bluedroid path uses start/stop events only.
 * @return Synchronization and stack status from config and start.
 */
BTStatus BLEAdvertising::start(uint32_t durationMs) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);

  if (xSemaphoreTakeRecursive(impl.mtx, pdMS_TO_TICKS(ADV_SYNC_TIMEOUT_MS)) != pdTRUE) {
    log_e("start: could not acquire mutex");
    return BTStatus::Fail;
  }

  log_d("start: customAdv=%d, customScanResp=%d, scanResp=%d", impl.customAdvData, impl.customScanRespData, impl.scanResp);

  BTStatus result = BTStatus::OK;

  // --- Configure advertising data ---
  if (impl.customAdvData) {
    // Raw advertising data supplied by user
    impl.advSync.take();
    esp_err_t err = esp_ble_gap_config_adv_data_raw(impl.rawAdvData.data(), impl.rawAdvData.size());
    if (err != ESP_OK) {
      log_e("esp_ble_gap_config_adv_data_raw: %s", esp_err_to_name(err));
      result = BTStatus::Fail;
      goto unlock;
    }
    result = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
    if (!result) {
      log_e("Timeout waiting for raw adv data set");
      goto unlock;
    }

    // Raw scan response if provided
    if (impl.customScanRespData) {
      impl.advSync.take();
      err = esp_ble_gap_config_scan_rsp_data_raw(impl.rawScanRespData.data(), impl.rawScanRespData.size());
      if (err != ESP_OK) {
        log_e("esp_ble_gap_config_scan_rsp_data_raw: %s", esp_err_to_name(err));
        result = BTStatus::Fail;
        goto unlock;
      }
      result = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
      if (!result) {
        log_e("Timeout waiting for raw scan resp data set");
        goto unlock;
      }
    }
  } else {
    // Build structured advertising data via esp_ble_adv_data_t
    std::vector<uint8_t> packedUUIDs = packServiceUUIDs(impl.serviceUUIDs);

    esp_ble_adv_data_t advData = {};
    advData.set_scan_rsp = false;
    // CSS Part A §1.2: Local Name belongs in the scan response to keep
    // primary AD free for flags and service UUIDs. When scan response is
    // disabled, the name must go into primary AD instead.
    advData.include_name = impl.scanResp ? false : impl.includeName;
    advData.include_txpower = impl.includeTxPower;
    advData.min_interval = impl.minPreferred;
    advData.max_interval = impl.maxPreferred;
    advData.appearance = impl.appearance;
    advData.flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT;
    advData.p_service_uuid = packedUUIDs.empty() ? nullptr : packedUUIDs.data();
    advData.service_uuid_len = packedUUIDs.size();

    impl.advSync.take();
    esp_err_t err = esp_ble_gap_config_adv_data(&advData);
    if (err != ESP_OK) {
      log_e("esp_ble_gap_config_adv_data: %s", esp_err_to_name(err));
      result = BTStatus::Fail;
      goto unlock;
    }
    result = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
    if (!result) {
      log_e("Timeout waiting for adv data set");
      goto unlock;
    }

    // Configure scan response data if enabled
    if (impl.scanResp) {
      esp_ble_adv_data_t scanRespData = {};
      scanRespData.set_scan_rsp = true;
      scanRespData.include_name = impl.includeName;
      scanRespData.include_txpower = impl.includeTxPower;
      scanRespData.min_interval = impl.minPreferred;
      scanRespData.max_interval = impl.maxPreferred;
      scanRespData.appearance = impl.appearance;

      impl.advSync.take();
      err = esp_ble_gap_config_adv_data(&scanRespData);
      if (err != ESP_OK) {
        log_e("esp_ble_gap_config_adv_data (scan resp): %s", esp_err_to_name(err));
        result = BTStatus::Fail;
        goto unlock;
      }
      result = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
      if (!result) {
        log_e("Timeout waiting for scan resp data set");
        goto unlock;
      }
    }
  }

  {
    // --- Build advertising parameters ---
    esp_ble_adv_params_t advParams = {};
    advParams.adv_int_min = impl.minInterval;
    advParams.adv_int_max = impl.maxInterval;
    advParams.adv_type = mapAdvType(impl.advType);
    advParams.own_addr_type = static_cast<esp_ble_addr_type_t>(BLE.getOwnAddressType());
    advParams.channel_map = ADV_CHNL_ALL;
    advParams.adv_filter_policy = mapFilterPolicy(impl.scanRequestWhitelistOnly, impl.connectWhitelistOnly);
    advParams.peer_addr_type = BLE_ADDR_TYPE_PUBLIC;

    // --- Start advertising ---
    impl.advSync.take();
    esp_err_t err = esp_ble_gap_start_advertising(&advParams);
    if (err != ESP_OK) {
      log_e("esp_ble_gap_start_advertising: %s", esp_err_to_name(err));
      result = BTStatus::Fail;
      goto unlock;
    }
    result = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
    if (!result) {
      log_e("Failed waiting for adv start (status=%d)", (int)impl.advSync.status());
      goto unlock;
    }
  }

  log_d("Advertising started");

unlock:
  xSemaphoreGiveRecursive(impl.mtx);
  return result;
}

// --------------------------------------------------------------------------
// stop()
// --------------------------------------------------------------------------

/**
 * @brief Stops active advertising, waiting for GAP completion under the advertising mutex.
 * @return OK on success, Fail on mutex or stack errors, or the advSync wait status on timeout.
 */
BTStatus BLEAdvertising::stop() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);

  if (xSemaphoreTakeRecursive(impl.mtx, pdMS_TO_TICKS(ADV_SYNC_TIMEOUT_MS)) != pdTRUE) {
    log_e("stop: could not acquire mutex");
    return BTStatus::Fail;
  }

  if (!impl.advertising) {
    xSemaphoreGiveRecursive(impl.mtx);
    return BTStatus::OK;
  }

  impl.advSync.take();
  esp_err_t err = esp_ble_gap_stop_advertising();
  if (err != ESP_OK) {
    log_e("esp_ble_gap_stop_advertising: %s", esp_err_to_name(err));
    xSemaphoreGiveRecursive(impl.mtx);
    return BTStatus::Fail;
  }
  BTStatus s = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
  if (!s) {
    log_e("Timeout waiting for adv stop");
    xSemaphoreGiveRecursive(impl.mtx);
    return s;
  }

  log_d("Advertising stopped");
  xSemaphoreGiveRecursive(impl.mtx);
  return BTStatus::OK;
}

// --------------------------------------------------------------------------
// onComplete
// --------------------------------------------------------------------------

/**
 * @brief Sets the callback invoked when advertising stop completes.
 * @param h Handler moved into the implementation; may be null.
 */
void BLEAdvertising::onComplete(CompleteHandler h) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onCompleteCb = std::move(h);
}

/**
 * @brief Clears the on-stop completion callback.
 */
void BLEAdvertising::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onCompleteCb = nullptr;
}

// --------------------------------------------------------------------------
// BLEClass factory and shortcuts
// --------------------------------------------------------------------------

/**
 * @brief Returns a shared BLEAdvertising instance and registers it for GAP events.
 * @return Valid object if BLE is initialized; otherwise empty.
 */
BLEAdvertising BLEClass::getAdvertising() {
  if (!isInitialized()) {
    return BLEAdvertising();
  }
  static std::shared_ptr<BLEAdvertising::Impl> advImpl;
  if (!advImpl) {
    advImpl = std::make_shared<BLEAdvertising::Impl>();
    BLEAdvertising::Impl::s_instance = advImpl.get();
  }
  return BLEAdvertising(advImpl);
}

/**
 * @brief Starts advertising on the default instance (duration per API default of start()).
 * @return Result of start().
 */
BTStatus BLEClass::startAdvertising() {
  return getAdvertising().start();
}

/**
 * @brief Stops advertising on the default instance.
 * @return Result of stop().
 */
BTStatus BLEClass::stopAdvertising() {
  return getAdvertising().stop();
}

#else /* !BLE_ADVERTISING_SUPPORTED -- stubs */

#include "BLE.h"
#include "esp32-hal-log.h"

// Stubs for BLE_ADVERTISING_SUPPORTED == 0: log or no-op; start/stop return NotSupported.

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

void BLEAdvertising::reset() {}

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

void BLEAdvertising::onComplete(CompleteHandler) {}

void BLEAdvertising::resetCallbacks() {}

BLEAdvertising BLEClass::getAdvertising() {
  log_w("Advertising not supported");
  return BLEAdvertising();
}

BTStatus BLEClass::startAdvertising() {
  log_w("Advertising not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEClass::stopAdvertising() {
  log_w("Advertising not supported");
  return BTStatus::NotSupported;
}

#endif /* BLE_ADVERTISING_SUPPORTED */

#endif /* BLE_BLUEDROID */
