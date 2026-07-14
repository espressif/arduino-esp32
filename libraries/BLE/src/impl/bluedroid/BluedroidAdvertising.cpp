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

#include "impl/common/BLEGuards.h"
#if BLE_BLUEDROID

#if BLE_ADVERTISING_SUPPORTED

#include "BLE.h"
#include "BluedroidAdvertising.h"

#include "impl/common/BLESync.h"
#include "impl/common/BLEMutex.h"
#include "impl/common/BLEImplHelpers.h"
#include "impl/common/BLEAdvScanHelpers.h"
#include "impl/common/BLEAdvertisingData.h"
#include "esp32-hal-log.h"

#include <esp_gap_ble_api.h>
#include <esp_bt_defs.h>
#include <string.h>

// API contract is documented on the declarations in the public BLE*.h headers; the definitions below carry implementation notes only.

static const uint32_t ADV_SYNC_TIMEOUT_MS = 2000;

// --------------------------------------------------------------------------
// Static instance for event routing
// --------------------------------------------------------------------------

BLEAdvertising::Impl *BLEAdvertising::Impl::s_instance = nullptr;

// --------------------------------------------------------------------------
// Helpers: map BLEAdvType to Bluedroid esp_ble_adv_type_t
// --------------------------------------------------------------------------

#if !BLE5_SUPPORTED
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
 * @note Only used by the native-legacy start() path (BLE5_SUPPORTED == 0).
 */
static esp_ble_adv_type_t mapAdvType(BLEAdvType type) {
  switch (type) {
    case BLEAdvType::ConnectableScannable: return ADV_TYPE_IND;
    case BLEAdvType::ConnectableDirected:  return ADV_TYPE_DIRECT_IND_LOW;
    case BLEAdvType::DirectedHighDuty:     return ADV_TYPE_DIRECT_IND_HIGH;
    case BLEAdvType::DirectedLowDuty:      return ADV_TYPE_DIRECT_IND_LOW;
    case BLEAdvType::ScannableUndirected:  return ADV_TYPE_SCAN_IND;
    case BLEAdvType::NonConnectable:       return ADV_TYPE_NONCONN_IND;
    default:                               return ADV_TYPE_IND;
  }
}
#endif /* !BLE5_SUPPORTED */

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
        inst->isAdvertising = true;
      }
      inst->advSync.give(param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
    {
      log_d("ADV_STOP_COMPLETE status=%d", param->adv_stop_cmpl.status);
      inst->isAdvertising = false;
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

#if BLE5_SUPPORTED
    // BLE5 extended advertising completion events feed the same advSync used by
    // the legacy setup path (operations are issued one at a time).
    case ESP_GAP_BLE_EXT_ADV_SET_PARAMS_COMPLETE_EVT:
      log_d("EXT_ADV_SET_PARAMS_COMPLETE status=%d", param->ext_adv_set_params.status);
      inst->advSync.give(param->ext_adv_set_params.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_EXT_ADV_DATA_SET_COMPLETE_EVT:
      log_d("EXT_ADV_DATA_SET_COMPLETE status=%d", param->ext_adv_data_set.status);
      inst->advSync.give(param->ext_adv_data_set.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_EXT_SCAN_RSP_DATA_SET_COMPLETE_EVT:
      log_d("EXT_SCAN_RSP_DATA_SET_COMPLETE status=%d", param->scan_rsp_set.status);
      inst->advSync.give(param->scan_rsp_set.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT:
      log_d("EXT_ADV_START_COMPLETE status=%d", param->ext_adv_start.status);
      if (param->ext_adv_start.status == ESP_BT_STATUS_SUCCESS) {
        inst->isAdvertising = true;
      }
      inst->advSync.give(param->ext_adv_start.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;

    case ESP_GAP_BLE_EXT_ADV_STOP_COMPLETE_EVT:
      log_d("EXT_ADV_STOP_COMPLETE status=%d", param->ext_adv_stop.status);
      inst->isAdvertising = false;
      inst->advSync.give(param->ext_adv_stop.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;
#if BLE_PERIODIC_ADV_SUPPORTED
    case ESP_GAP_BLE_PERIODIC_ADV_SET_PARAMS_COMPLETE_EVT:
      log_d("PERIODIC_ADV_SET_PARAMS_COMPLETE status=%d", param->peroid_adv_set_params.status);
      inst->advSync.give(param->peroid_adv_set_params.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;
    case ESP_GAP_BLE_PERIODIC_ADV_DATA_SET_COMPLETE_EVT:
      log_d("PERIODIC_ADV_DATA_SET_COMPLETE status=%d", param->period_adv_data_set.status);
      inst->advSync.give(param->period_adv_data_set.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;
    case ESP_GAP_BLE_PERIODIC_ADV_START_COMPLETE_EVT:
      log_d("PERIODIC_ADV_START_COMPLETE status=%d", param->period_adv_start.status);
      inst->advSync.give(param->period_adv_start.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;
    case ESP_GAP_BLE_PERIODIC_ADV_STOP_COMPLETE_EVT:
      log_d("PERIODIC_ADV_STOP_COMPLETE status=%d", param->period_adv_stop.status);
      inst->advSync.give(param->period_adv_stop.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
      break;
#endif /* BLE_PERIODIC_ADV_SUPPORTED */
#endif /* BLE5_SUPPORTED */

    default: break;
  }
}

// --------------------------------------------------------------------------
// Configuration setters
// --------------------------------------------------------------------------

void BLEAdvertising::setName(const String &name) {
  BLE_CHECK_IMPL();
  impl.name = name;
  if (name.length() > 0) {
    esp_ble_gap_set_device_name(name.c_str());
  }
}

void BLEAdvertising::setScanResponse(bool enable) {
  BLE_CHECK_IMPL();
  impl.scanResp = enable;
}

void BLEAdvertising::setType(BLEAdvType type) {
  BLE_CHECK_IMPL();
  impl.advType = type;
}

void BLEAdvertising::setInterval(uint16_t minMs, uint16_t maxMs) {
  BLE_CHECK_IMPL();
  impl.minInterval = bleMsToUnits0625(minMs);
  impl.maxInterval = bleMsToUnits0625(maxMs);
}

void BLEAdvertising::setMinPreferred(uint16_t val) {
  BLE_CHECK_IMPL();
  impl.minPreferred = val;
}

void BLEAdvertising::setMaxPreferred(uint16_t val) {
  BLE_CHECK_IMPL();
  impl.maxPreferred = val;
}

void BLEAdvertising::setTxPower(bool include) {
  BLE_CHECK_IMPL();
  impl.includeTxPower = include;
}

void BLEAdvertising::setAppearance(uint16_t appearance) {
  BLE_CHECK_IMPL();
  impl.appearance = appearance;
}

void BLEAdvertising::setScanFilter(bool scanRequestWhitelistOnly, bool connectWhitelistOnly) {
  BLE_CHECK_IMPL();
  impl.scanRequestWhitelistOnly = scanRequestWhitelistOnly;
  impl.connectWhitelistOnly = connectWhitelistOnly;
}

void BLEAdvertising::reset() {
  BLE_CHECK_IMPL();
  if (impl.isAdvertising) {
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
#if BLE5_SUPPORTED
  // Clear extended per-instance state (and the controller's sets, best-effort)
  // so the process-wide static Impl starts clean across BLE.end()/begin().
  (void)esp_ble_gap_ext_adv_set_clear();
  for (uint8_t i = 0; i < Impl::kMaxExtInstances; i++) {
    impl.extInstances[i] = Impl::ExtInstance{};
  }
#endif
}

// --------------------------------------------------------------------------
// Custom advertisement / scan response data
// --------------------------------------------------------------------------

void BLEAdvertising::setAdvertisementData(const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL();
  impl.rawAdvData.assign(data.data(), data.data() + data.length());
  impl.customAdvData = true;
}

void BLEAdvertising::setScanResponseData(const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL();
  impl.rawScanRespData.assign(data.data(), data.data() + data.length());
  impl.customScanRespData = true;
}

// --------------------------------------------------------------------------
// start()
// --------------------------------------------------------------------------

// durationMs is currently unused on this Bluedroid path; advertising relies on start/stop events only.
BTStatus BLEAdvertising::start(uint32_t durationMs) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);

  BLELockGuard lock(impl.mtx);

#if BLE5_SUPPORTED
  // On BLE5-capable Bluedroid the public "legacy" advertising runs over the
  // extended API with a legacy PDU on the reserved instance.
  return impl.startLegacyViaExtAdv(durationMs);
#else
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
      return result;
    }
    result = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
    if (!result) {
      log_e("Timeout waiting for raw adv data set");
      return result;
    }

    // Raw scan response if provided
    if (impl.customScanRespData) {
      impl.advSync.take();
      err = esp_ble_gap_config_scan_rsp_data_raw(impl.rawScanRespData.data(), impl.rawScanRespData.size());
      if (err != ESP_OK) {
        log_e("esp_ble_gap_config_scan_rsp_data_raw: %s", esp_err_to_name(err));
        result = BTStatus::Fail;
        return result;
      }
      result = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
      if (!result) {
        log_e("Timeout waiting for raw scan resp data set");
        return result;
      }
    }
  } else {
    // Serialize the AD payload ourselves and hand raw bytes to the controller,
    // instead of letting Bluedroid rebuild it via esp_ble_adv_data_t (which
    // force-expands every service UUID to 128 bits). This produces the compact
    // 16/32-bit UUID encoding and the exact same byte layout as the NimBLE and
    // Bluedroid extended-advertising paths (CSS Part A §1.2: the Local Name is
    // carried in the scan response when scannable, otherwise in the primary AD).
    const bool scannable =
      (impl.advType == BLEAdvType::ConnectableScannable || impl.advType == BLEAdvType::ScannableUndirected);
    String advName = impl.name.length() > 0 ? impl.name : BLE.getDeviceName();
    const bool nameInScanRsp = impl.scanResp && scannable;

    BLEAdvertisementData advData = BLEAdvDataBuilder::buildLegacyAdvData(
      advName, impl.serviceUUIDs, impl.appearance, impl.includeTxPower, BLE.getPower(), nameInScanRsp, impl.minPreferred, impl.maxPreferred
    );
    impl.advSync.take();
    esp_err_t err = esp_ble_gap_config_adv_data_raw(const_cast<uint8_t *>(advData.data()), advData.length());
    if (err != ESP_OK) {
      log_e("esp_ble_gap_config_adv_data_raw: %s", esp_err_to_name(err));
      result = BTStatus::Fail;
      return result;
    }
    result = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
    if (!result) {
      log_e("Timeout waiting for adv data set");
      return result;
    }

    // Scan response is only valid for scannable PDUs.
    if (nameInScanRsp) {
      BLEAdvertisementData scanRespData = BLEAdvDataBuilder::buildLegacyScanRespData(advName);
      impl.advSync.take();
      err = esp_ble_gap_config_scan_rsp_data_raw(const_cast<uint8_t *>(scanRespData.data()), scanRespData.length());
      if (err != ESP_OK) {
        log_e("esp_ble_gap_config_scan_rsp_data_raw (scan resp): %s", esp_err_to_name(err));
        result = BTStatus::Fail;
        return result;
      }
      result = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
      if (!result) {
        log_e("Timeout waiting for scan resp data set");
        return result;
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
      return result;
    }
    result = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
    if (!result) {
      log_e("Failed waiting for adv start (status=%d)", (int)impl.advSync.status());
      return result;
    }
  }

  log_d("Advertising started");
  return result;
#endif /* BLE5_SUPPORTED */
}

// --------------------------------------------------------------------------
// stop()
// --------------------------------------------------------------------------

BTStatus BLEAdvertising::stop() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);

  BLELockGuard lock(impl.mtx);

  if (!impl.isAdvertising) {
    return BTStatus::OK;
  }

#if BLE5_SUPPORTED
  uint8_t stopInstance = Impl::kLegacyInstance;
  impl.advSync.take();
  esp_err_t err = esp_ble_gap_ext_adv_stop(1, &stopInstance);
  if (err != ESP_OK) {
    log_e("esp_ble_gap_ext_adv_stop: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  BTStatus s = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
  if (!s) {
    log_e("Timeout waiting for ext adv stop");
    return s;
  }
  log_d("Advertising stopped");
  return BTStatus::OK;
#else
  impl.advSync.take();
  esp_err_t err = esp_ble_gap_stop_advertising();
  if (err != ESP_OK) {
    log_e("esp_ble_gap_stop_advertising: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  BTStatus s = impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
  if (!s) {
    log_e("Timeout waiting for adv stop");
    return s;
  }

  log_d("Advertising stopped");
  return BTStatus::OK;
#endif /* BLE5_SUPPORTED */
}

// --------------------------------------------------------------------------
// onComplete
// --------------------------------------------------------------------------

void BLEAdvertising::onComplete(CompleteHandler h) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onCompleteCb = std::move(h);
}

void BLEAdvertising::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onCompleteCb = nullptr;
}

// --------------------------------------------------------------------------
// BLE5 extended advertising (TX) -- Bluedroid
//
// Compiled only on BLE5-capable Bluedroid silicon (BLE5_SUPPORTED). Periodic
// advertising TX is compiled when BLE_PERIODIC_ADV_TX_SUPPORTED (BLE5 +
// CONFIG_BT_BLE_50_PERIODIC_ADV_EN).
// --------------------------------------------------------------------------

#if BLE5_SUPPORTED

// Extended (non-legacy) PDU property mask, assembled from the shared neutral
// property flags. An extended PDU cannot be both connectable and scannable
// (Vol 6, Part B, §4.4.2.4), which advTypeToExtAdvProps already enforces.
static esp_ble_ext_adv_type_mask_t extAdvTypeMask(BLEAdvType type) {
  const BLEExtAdvProps props = advTypeToExtAdvProps(type, /*legacyPdu=*/false);
  esp_ble_ext_adv_type_mask_t mask = ESP_BLE_GAP_SET_EXT_ADV_PROP_NONCONN_NONSCANNABLE_UNDIRECTED;
  if (props.connectable) {
    mask |= ESP_BLE_GAP_SET_EXT_ADV_PROP_CONNECTABLE;
  }
  if (props.scannable) {
    mask |= ESP_BLE_GAP_SET_EXT_ADV_PROP_SCANNABLE;
  }
  if (props.highDutyDirected) {
    mask |= ESP_BLE_GAP_SET_EXT_ADV_PROP_HD_DIRECTED;
  } else if (props.directed) {
    mask |= ESP_BLE_GAP_SET_EXT_ADV_PROP_DIRECTED;
  }
  return mask;
}

// Legacy-PDU property mask (used by the reserved legacy set so legacy-only peers
// still discover us). Derived from the same neutral flags with legacy semantics.
static esp_ble_ext_adv_type_mask_t legacyExtAdvTypeMask(BLEAdvType type) {
  const BLEExtAdvProps props = advTypeToExtAdvProps(type, /*legacyPdu=*/true);
  if (props.directed) {
    return props.highDutyDirected ? ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_HD_DIR : ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_LD_DIR;
  }
  if (props.connectable) {  // legacy connectable undirected (ADV_IND) is always scannable
    return ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_IND;
  }
  if (props.scannable) {
    return ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_SCAN;
  }
  return ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_NONCONN;
}

BLEAdvertising::Impl::ExtInstance *BLEAdvertising::Impl::extInstanceAt(uint8_t instance) {
  // The top instance is reserved for the legacy start()/stop() path.
  if (instance >= kLegacyInstance) {
    return nullptr;
  }
  ExtInstance *extInst = &extInstances[instance];
  if (!extInst->slotInitialized) {
    extInst->slotInitialized = true;
    extInst->params.type = ESP_BLE_GAP_SET_EXT_ADV_PROP_CONNECTABLE;
    extInst->params.interval_min = 0x30;  // 30 ms in 0.625 ms units
    extInst->params.interval_max = 0x60;  // 60 ms
    extInst->params.channel_map = ADV_CHNL_ALL;
    extInst->params.own_addr_type = static_cast<esp_ble_addr_type_t>(BLE.getOwnAddressType());
    extInst->params.filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
    extInst->params.primary_phy = ESP_BLE_GAP_PRI_PHY_1M;
    extInst->params.secondary_phy = ESP_BLE_GAP_PHY_1M;
    extInst->params.tx_power = EXT_ADV_TX_PWR_NO_PREFERENCE;
    extInst->params.sid = 0;
#if BLE_PERIODIC_ADV_SUPPORTED
    // Default periodic interval 80 ms (units of 1.25 ms); properties=0 omits TX power.
    extInst->periodicParams.interval_min = 64;
    extInst->periodicParams.interval_max = 64;
    extInst->periodicParams.properties = 0;
#endif
  }
  return extInst;
}

BTStatus BLEAdvertising::Impl::ensureExtConfigured(uint8_t instance) {
  auto *extInst = extInstanceAt(instance);
  if (!extInst) {
    return BTStatus::InvalidParam;
  }
  if (extInst->extAdvRegistered) {
    return BTStatus::OK;
  }
  advSync.take();
  esp_err_t err = esp_ble_gap_ext_adv_set_params(instance, &extInst->params);
  if (err != ESP_OK) {
    log_e("esp_ble_gap_ext_adv_set_params inst %u: %s", instance, esp_err_to_name(err));
    return BTStatus::Fail;
  }
  BTStatus st = advSync.wait(ADV_SYNC_TIMEOUT_MS);
  if (st) {
    extInst->extAdvRegistered = true;
  }
  return st;
}

BTStatus BLEAdvertising::Impl::startLegacyViaExtAdv(uint32_t durationMs) {
  esp_ble_gap_ext_adv_params_t params = {};
  params.type = legacyExtAdvTypeMask(advType);
  params.interval_min = minInterval;
  params.interval_max = maxInterval;
  params.channel_map = ADV_CHNL_ALL;
  params.own_addr_type = static_cast<esp_ble_addr_type_t>(BLE.getOwnAddressType());
  params.peer_addr_type = BLE_ADDR_TYPE_PUBLIC;
  params.filter_policy = mapFilterPolicy(scanRequestWhitelistOnly, connectWhitelistOnly);
  params.primary_phy = ESP_BLE_GAP_PRI_PHY_1M;   // legacy PDUs are 1M only
  params.secondary_phy = ESP_BLE_GAP_PHY_1M;
  params.sid = 0;
  params.tx_power = EXT_ADV_TX_PWR_NO_PREFERENCE;
  params.scan_req_notif = false;

  advSync.take();
  esp_err_t err = esp_ble_gap_ext_adv_set_params(kLegacyInstance, &params);
  if (err != ESP_OK) {
    log_e("legacy ext_adv_set_params: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  BTStatus st = advSync.wait(ADV_SYNC_TIMEOUT_MS);
  if (!st) {
    return st;
  }

  const bool scannable = (advType == BLEAdvType::ConnectableScannable || advType == BLEAdvType::ScannableUndirected);
  String advName = name.length() > 0 ? name : BLE.getDeviceName();
  const bool nameInScanRsp = scanResp && scannable;

  {
    // Point directly at the source bytes (a member vector for custom data, or a
    // locally built payload) instead of copying into a temporary; the payload
    // stays alive across the config call and its completion wait.
    BLEAdvertisementData built;
    const uint8_t *advPtr = rawAdvData.data();
    size_t advLen = rawAdvData.size();
    if (!customAdvData) {
      built = BLEAdvDataBuilder::buildLegacyAdvData(
        advName, serviceUUIDs, appearance, includeTxPower, BLE.getPower(), nameInScanRsp, minPreferred, maxPreferred
      );
      advPtr = built.data();
      advLen = built.length();
    }
    advSync.take();
    err = esp_ble_gap_config_ext_adv_data_raw(kLegacyInstance, advLen, advPtr);
    if (err != ESP_OK) {
      log_e("legacy config_ext_adv_data_raw: %s", esp_err_to_name(err));
      return BTStatus::Fail;
    }
    st = advSync.wait(ADV_SYNC_TIMEOUT_MS);
    if (!st) {
      return st;
    }
  }

  if (scannable && scanResp) {
    BLEAdvertisementData built;
    const uint8_t *rspPtr = rawScanRespData.data();
    size_t rspLen = rawScanRespData.size();
    if (!customScanRespData) {
      built = BLEAdvDataBuilder::buildLegacyScanRespData(advName);
      rspPtr = built.data();
      rspLen = built.length();
    }
    advSync.take();
    err = esp_ble_gap_config_ext_scan_rsp_data_raw(kLegacyInstance, rspLen, rspPtr);
    if (err != ESP_OK) {
      log_e("legacy config_ext_scan_rsp_data_raw: %s", esp_err_to_name(err));
      return BTStatus::Fail;
    }
    st = advSync.wait(ADV_SYNC_TIMEOUT_MS);
    if (!st) {
      return st;
    }
  }

  esp_ble_gap_ext_adv_t startSet = {};
  startSet.instance = kLegacyInstance;
  startSet.duration = (durationMs == 0) ? 0 : static_cast<int>(durationMs / 10);
  startSet.max_events = 0;
  advSync.take();
  err = esp_ble_gap_ext_adv_start(1, &startSet);
  if (err != ESP_OK) {
    log_e("legacy ext_adv_start: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return advSync.wait(ADV_SYNC_TIMEOUT_MS);
}

void BLEAdvertising::setExtType(uint8_t instance, BLEAdvType type) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtType: instance %u out of range", instance);
    return;
  }
  extInst->params.type = extAdvTypeMask(type);
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtPhy(uint8_t instance, BLEPhy primaryPhy, BLEPhy secondaryPhy) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtPhy: instance %u out of range", instance);
    return;
  }
  // BLEPhy enum values match ESP_BLE_GAP_PHY_* codes (1M=1, 2M=2, Coded=3).
  extInst->params.primary_phy = static_cast<esp_ble_gap_pri_phy_t>(primaryPhy);
  extInst->params.secondary_phy = static_cast<esp_ble_gap_phy_t>(secondaryPhy);
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtTxPower(uint8_t instance, int8_t txPower) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtTxPower: instance %u out of range", instance);
    return;
  }
  extInst->params.tx_power = txPower;
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtInterval(uint8_t instance, uint16_t minInterval, uint16_t maxInterval) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtInterval: instance %u out of range", instance);
    return;
  }
  extInst->params.interval_min = minInterval;
  extInst->params.interval_max = maxInterval;
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtChannelMap(uint8_t instance, uint8_t channelMap) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtChannelMap: instance %u out of range", instance);
    return;
  }
  extInst->params.channel_map = static_cast<esp_ble_adv_channel_t>(channelMap);
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtSID(uint8_t instance, uint8_t sid) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtSID: instance %u out of range", instance);
    return;
  }
  extInst->params.sid = sid;
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtAnonymous(uint8_t instance, bool anonymous) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtAnonymous: instance %u out of range", instance);
    return;
  }
  if (anonymous) {
    extInst->params.type |= ESP_BLE_GAP_SET_EXT_ADV_PROP_ANON_ADV;
  } else {
    extInst->params.type &= ~ESP_BLE_GAP_SET_EXT_ADV_PROP_ANON_ADV;
  }
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtIncludeTxPower(uint8_t instance, bool include) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtIncludeTxPower: instance %u out of range", instance);
    return;
  }
  if (include) {
    extInst->params.type |= ESP_BLE_GAP_SET_EXT_ADV_PROP_INCLUDE_TX_PWR;
  } else {
    extInst->params.type &= ~ESP_BLE_GAP_SET_EXT_ADV_PROP_INCLUDE_TX_PWR;
  }
  extInst->extAdvRegistered = false;
}

void BLEAdvertising::setExtScanReqNotify(uint8_t instance, bool enable) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setExtScanReqNotify: instance %u out of range", instance);
    return;
  }
  extInst->params.scan_req_notif = enable;
  extInst->extAdvRegistered = false;
}

BTStatus BLEAdvertising::setExtAdvertisementData(uint8_t instance, const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BTStatus st = impl.ensureExtConfigured(instance);
  if (!st) {
    return st;
  }
  impl.advSync.take();
  esp_err_t err = esp_ble_gap_config_ext_adv_data_raw(instance, data.length(), data.data());
  if (err != ESP_OK) {
    log_e("config_ext_adv_data_raw inst %u: %s", instance, esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
}

BTStatus BLEAdvertising::setExtScanResponseData(uint8_t instance, const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  // A scan-response payload can only be staged on an already-configured set, so
  // apply the instance's accumulated params first.
  BTStatus st = impl.ensureExtConfigured(instance);
  if (!st) {
    return st;
  }
  impl.advSync.take();
  esp_err_t err = esp_ble_gap_config_ext_scan_rsp_data_raw(instance, data.length(), data.data());
  if (err != ESP_OK) {
    log_e("config_ext_scan_rsp_data_raw inst %u: %s", instance, esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
}

BTStatus BLEAdvertising::setExtInstanceAddress(uint8_t instance, const BTAddress &addr) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  BTStatus st = impl.ensureExtConfigured(instance);
  if (!st) {
    return st;
  }
  esp_bd_addr_t bd;
  addr.toEspBdAddr(bd);
  esp_err_t err = esp_ble_gap_ext_adv_set_rand_addr(instance, bd);
  if (err != ESP_OK) {
    log_e("ext_adv_set_rand_addr inst %u: %s", instance, esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

BTStatus BLEAdvertising::startExtended(uint8_t instance, uint32_t durationMs, uint8_t maxEvents) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  // Ensure params are applied even if the caller started without setting data.
  BTStatus st = impl.ensureExtConfigured(instance);
  if (!st) {
    return st;
  }
  esp_ble_gap_ext_adv_t startSet = {};
  startSet.instance = instance;
  startSet.duration = (durationMs == 0) ? 0 : static_cast<int>(durationMs / 10);
  startSet.max_events = maxEvents;
  impl.advSync.take();
  esp_err_t err = esp_ble_gap_ext_adv_start(1, &startSet);
  if (err != ESP_OK) {
    log_e("ext_adv_start inst %u: %s", instance, esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
}

BTStatus BLEAdvertising::stopExtended(uint8_t instance) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  uint8_t inst = instance;
  impl.advSync.take();
  esp_err_t err = esp_ble_gap_ext_adv_stop(1, &inst);
  if (err != ESP_OK) {
    log_e("ext_adv_stop inst %u: %s", instance, esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
}

BTStatus BLEAdvertising::removeExtended(uint8_t instance) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  esp_err_t err = esp_ble_gap_ext_adv_set_remove(instance);
  if (err != ESP_OK) {
    log_e("ext_adv_set_remove inst %u: %s", instance, esp_err_to_name(err));
    return BTStatus::Fail;
  }
  if (instance < Impl::kMaxExtInstances) {
    impl.extInstances[instance] = Impl::ExtInstance{};
  }
  return BTStatus::OK;
}

BTStatus BLEAdvertising::clearExtended() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  esp_err_t err = esp_ble_gap_ext_adv_set_clear();
  if (err != ESP_OK) {
    log_e("ext_adv_set_clear: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  for (uint8_t i = 0; i < Impl::kMaxExtInstances; i++) {
    impl.extInstances[i] = Impl::ExtInstance{};
  }
  return BTStatus::OK;
}

#if BLE_PERIODIC_ADV_SUPPORTED

BTStatus BLEAdvertising::Impl::ensurePeriodicConfigured(uint8_t instance) {
  auto *extInst = extInstanceAt(instance);
  if (!extInst) {
    return BTStatus::InvalidParam;
  }
  if (extInst->periodicAdvRegistered) {
    return BTStatus::OK;
  }
  advSync.take();
  esp_err_t err = esp_ble_gap_periodic_adv_set_params(instance, &extInst->periodicParams);
  if (err != ESP_OK) {
    log_e("esp_ble_gap_periodic_adv_set_params inst %u: %s", instance, esp_err_to_name(err));
    advSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  BTStatus st = advSync.wait(ADV_SYNC_TIMEOUT_MS);
  if (st) {
    extInst->periodicAdvRegistered = true;
  }
  return st;
}

void BLEAdvertising::setPeriodicAdvInterval(uint8_t instance, uint16_t minInterval, uint16_t maxInterval) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setPeriodicAdvInterval: instance %u out of range", instance);
    return;
  }
  extInst->periodicParams.interval_min = minInterval;
  extInst->periodicParams.interval_max = maxInterval;
  extInst->periodicAdvRegistered = false;
}

void BLEAdvertising::setPeriodicAdvTxPower(uint8_t instance, bool include) {
  BLE_CHECK_IMPL();
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    log_e("setPeriodicAdvTxPower: instance %u out of range", instance);
    return;
  }
  // BT Core Spec: Include TxPower is bit 6 of the periodic advertising properties.
  if (include) {
    extInst->periodicParams.properties |= 0x40;
  } else {
    extInst->periodicParams.properties &= static_cast<uint16_t>(~0x40);
  }
  extInst->periodicAdvRegistered = false;
}

BTStatus BLEAdvertising::setPeriodicAdvData(uint8_t instance, const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    return BTStatus::InvalidParam;
  }
  BTStatus st = impl.ensureExtConfigured(instance);
  if (!st) {
    return st;
  }
  st = impl.ensurePeriodicConfigured(instance);
  if (!st) {
    return st;
  }
  impl.advSync.take();
#if defined(CONFIG_BT_BLE_FEAT_PERIODIC_ADV_ENH)
  esp_err_t err = esp_ble_gap_config_periodic_adv_data_raw(instance, data.length(), data.data(), false);
#else
  esp_err_t err = esp_ble_gap_config_periodic_adv_data_raw(instance, data.length(), data.data());
#endif
  if (err != ESP_OK) {
    log_e("esp_ble_gap_config_periodic_adv_data_raw inst %u: %s", instance, esp_err_to_name(err));
    impl.advSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  return impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
}

BTStatus BLEAdvertising::startPeriodicAdv(uint8_t instance) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  auto *extInst = impl.extInstanceAt(instance);
  if (!extInst) {
    return BTStatus::InvalidParam;
  }
  BTStatus st = impl.ensureExtConfigured(instance);
  if (!st) {
    return st;
  }
  st = impl.ensurePeriodicConfigured(instance);
  if (!st) {
    return st;
  }
  impl.advSync.take();
#if defined(CONFIG_BT_BLE_FEAT_PERIODIC_ADV_ENH)
  esp_err_t err = esp_ble_gap_periodic_adv_start(instance, false);
#else
  esp_err_t err = esp_ble_gap_periodic_adv_start(instance);
#endif
  if (err != ESP_OK) {
    log_e("esp_ble_gap_periodic_adv_start inst %u: %s", instance, esp_err_to_name(err));
    impl.advSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  return impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
}

BTStatus BLEAdvertising::stopPeriodicAdv(uint8_t instance) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  impl.advSync.take();
  esp_err_t err = esp_ble_gap_periodic_adv_stop(instance);
  if (err != ESP_OK) {
    log_e("esp_ble_gap_periodic_adv_stop inst %u: %s", instance, esp_err_to_name(err));
    impl.advSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  return impl.advSync.wait(ADV_SYNC_TIMEOUT_MS);
}

#endif /* BLE_PERIODIC_ADV_SUPPORTED */

#endif /* BLE5_SUPPORTED */

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

#endif /* BLE_ADVERTISING_SUPPORTED */

// BLE5 extended + periodic advertising TX are implemented above under BLE5_SUPPORTED /
// BLE_PERIODIC_ADV_SUPPORTED. When those guards are off, the shared NotSupported
// fallbacks in BLEAdvertising.cpp provide the public entry points.

#endif /* BLE_BLUEDROID */
