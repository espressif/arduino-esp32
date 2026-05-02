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
 * @file NimBLEAdvertising.cpp
 * @brief NimBLE implementation of BLE GAP advertising and advertising completion callbacks.
 *
 * Spec references:
 *  - BT Core Spec v5.x, Vol 3, Part C (GAP) — advertising and discovery procedures.
 *  - BT Core Spec v5.x, Vol 6, Part B, §2.3.1 — LE advertising PDU types
 *    (ADV_IND, ADV_DIRECT_IND, ADV_NONCONN_IND, ADV_SCAN_IND, SCAN_RSP).
 *  - BT Core Spec v5.x, Vol 6, Part B, §4.4.2 — advertising interval timing:
 *    unit = 0.625 ms; minimum = 0x0020 (20 ms); maximum = 0x4000 (10.24 s).
 *  - CSS (Supplement to Core Spec), Part A — AD type codes and structures.
 *    §1.3: Flags (0x01); §1.2: Local Name (0x08/0x09).
 *  - BT Core Spec v5.x, Vol 3, Part C, §11 — AD structure format and 31-byte limit
 *    for legacy advertising PDUs.
 */

#include "impl/BLEGuards.h"
#if BLE_NIMBLE

#include "BLE.h"
#include "NimBLEAdvertising.h"

#include "impl/BLEImplHelpers.h"
#include "esp32-hal-log.h"

#if BLE_ADVERTISING_SUPPORTED

#include <host/ble_hs.h>
#include <host/ble_gap.h>

/**
 * @brief NimBLE GAP callback for advertising: completion, connect, and server forwarding.
 * @param event GAP event from NimBLE.
 * @param arg User pointer to BLEAdvertising::Impl.
 * @return 0 for handled; may forward to the server for connect events.
 */
int BLEAdvertising::Impl::gapEventCallback(struct ble_gap_event *event, void *arg) {
  auto *impl = static_cast<BLEAdvertising::Impl *>(arg);
  if (event->type == BLE_GAP_EVENT_ADV_COMPLETE) {
    impl->advertising = false;
    BLEAdvertising::CompleteHandler cb;
    {
      BLELockGuard lock(impl->mtx);
      cb = impl->onCompleteCb;
    }
    if (cb) {
      cb(0);
    }
    return 0;
  }

  if (event->type == BLE_GAP_EVENT_CONNECT) {
    impl->advertising = false;
  }

  BLEServer server = BLE.createServer();
  if (server) {
    return server.handleGapEvent(event);
  }
  return 0;
}

// --------------------------------------------------------------------------
// BLEAdvertising public API (stack-specific)
// --------------------------------------------------------------------------

/**
 * @brief Sets the device name used when building default advertisement fields.
 * @param name Name string to encode in AD data.
 */
void BLEAdvertising::setName(const String &name) {
  BLE_CHECK_IMPL();
  impl.deviceName = name;
}

/**
 * @brief Enables or disables use of a scan response payload (when not using only custom data paths).
 * @param enable True to allow scan response data to be set.
 */
void BLEAdvertising::setScanResponse(bool enable) {
  BLE_CHECK_IMPL();
  impl.scanResponseEnabled = enable;
}

/**
 * @brief Sets the high-level advertising PDU type to apply when starting advertising.
 * @param type Connectable, scannable, directed, etc.
 */
void BLEAdvertising::setType(BLEAdvType type) {
  BLE_CHECK_IMPL();
  impl.advType = type;
}

/**
 * @brief Sets advertising interval range in milliseconds (converted to 0.625 ms units internally).
 * @param minMs Minimum interval in ms.
 * @param maxMs Maximum interval in ms.
 * @note BT Core Spec v5.x, Vol 6, Part B, §4.4.2.2:
 *       Controller timing units are 0.625 ms; range 0x0020 (20 ms) – 0x4000 (10.24 s).
 *       Conversion: units = (ms × 1000) / 625.
 */
void BLEAdvertising::setInterval(uint16_t minMs, uint16_t maxMs) {
  BLE_CHECK_IMPL();
  impl.minInterval = (minMs * 1000) / 625;
  impl.maxInterval = (maxMs * 1000) / 625;
}

/**
 * @brief Sets the preferred connection interval minimum (as used in AD fields when applicable).
 * @param v Preferred minimum connection interval.
 */
void BLEAdvertising::setMinPreferred(uint16_t v) {
  BLE_CHECK_IMPL();
  impl.minPreferred = v;
}

/**
 * @brief Sets the preferred connection interval maximum (as used in AD fields when applicable).
 * @param v Preferred maximum connection interval.
 */
void BLEAdvertising::setMaxPreferred(uint16_t v) {
  BLE_CHECK_IMPL();
  impl.maxPreferred = v;
}

/**
 * @brief Whether to include TX power in built-in advertisement/scan response fields.
 * @param include True to include the current radio TX power.
 */
void BLEAdvertising::setTxPower(bool include) {
  BLE_CHECK_IMPL();
  impl.includeTxPower = include;
}

/**
 * @brief Sets the GAP appearance value for default advertisement data.
 * @param appearance 16-bit appearance constant.
 */
void BLEAdvertising::setAppearance(uint16_t appearance) {
  BLE_CHECK_IMPL();
  impl.appearance = appearance;
}

/**
 * @brief Sets the white-list / filter policy bits for scan and connection requests.
 * @param scanWl If true, restrict scan requests to the whitelist.
 * @param connWl If true, restrict connection requests to the whitelist.
 */
void BLEAdvertising::setScanFilter(bool scanWl, bool connWl) {
  BLE_CHECK_IMPL();
  impl.filterPolicy = 0;
  if (scanWl) {
    impl.filterPolicy |= 0x01;
  }
  if (connWl) {
    impl.filterPolicy |= 0x02;
  }
}

/**
 * @brief Restores all advertising options and buffers to a default empty state.
 */
void BLEAdvertising::reset() {
  BLE_CHECK_IMPL();
  if (impl.advertising) {
    stop();
  }
  impl.serviceUUIDs.clear();
  impl.deviceName = "";
  impl.useCustomAdvData = false;
  impl.useCustomScanRsp = false;
  impl.customAdvData.clear();
  impl.customScanRspData.clear();
  impl.appearance = 0;
  impl.includeTxPower = false;
  impl.minInterval = 0x20;
  impl.maxInterval = 0x40;
  impl.minPreferred = 0;
  impl.maxPreferred = 0;
  impl.advType = BLEAdvType::ConnectableScannable;
  impl.scanResponseEnabled = true;
  impl.filterPolicy = 0;
}

/**
 * @brief Uses caller-supplied raw advertisement bytes instead of fields built from UUIDs/name.
 * @param data Encoded AD payload.
 */
void BLEAdvertising::setAdvertisementData(const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL();
  impl.customAdvData = data;
  impl.useCustomAdvData = true;
}

/**
 * @brief Sets custom raw scan response bytes; implies custom scan response mode.
 * @param data Encoded scan response payload.
 */
void BLEAdvertising::setScanResponseData(const BLEAdvertisementData &data) {
  BLE_CHECK_IMPL();
  impl.customScanRspData = data;
  impl.useCustomScanRsp = true;
}

/**
 * @brief Builds AD/scan response, then starts GAP advertising for a duration or indefinitely.
 * @param durationMs Duration in ms (0 = forever, scaled to stack units in the call).
 * @return Outcome of stack calls and data setup.
 */
BTStatus BLEAdvertising::start(uint32_t durationMs) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.advertising) {
    return BTStatus::OK;
  }

  struct ble_gap_adv_params advParams = {};
  switch (impl.advType) {
    case BLEAdvType::Connectable:
    case BLEAdvType::ConnectableScannable:
      advParams.conn_mode = BLE_GAP_CONN_MODE_UND;
      advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;
      break;
    case BLEAdvType::ConnectableDirected:
    case BLEAdvType::DirectedLowDuty:
      advParams.conn_mode = BLE_GAP_CONN_MODE_DIR;
      advParams.disc_mode = BLE_GAP_DISC_MODE_NON;
      advParams.high_duty_cycle = 0;
      break;
    case BLEAdvType::DirectedHighDuty:
      advParams.conn_mode = BLE_GAP_CONN_MODE_DIR;
      advParams.disc_mode = BLE_GAP_DISC_MODE_NON;
      advParams.high_duty_cycle = 1;
      break;
    case BLEAdvType::ScannableUndirected:
      advParams.conn_mode = BLE_GAP_CONN_MODE_NON;
      advParams.disc_mode = BLE_GAP_DISC_MODE_GEN;
      break;
    case BLEAdvType::NonConnectable:
      advParams.conn_mode = BLE_GAP_CONN_MODE_NON;
      advParams.disc_mode = BLE_GAP_DISC_MODE_NON;
      break;
  }
  advParams.itvl_min = impl.minInterval;
  advParams.itvl_max = impl.maxInterval;
  advParams.filter_policy = impl.filterPolicy;

  // Resolve device name once; placed in scan response per BLE spec
  // (CSS Part A, §1.2) to keep the primary AD free for flags and UUIDs.
  // The scan response is only sent in reply to an active SCAN_REQ from a
  // scanning device (BT Core Spec v5.x, Vol 6, Part B, §4.4.3.2).
  String name = impl.deviceName.length() > 0 ? impl.deviceName : BLE.getDeviceName();

  // Build primary advertisement data: flags, UUIDs, appearance.
  // Set both LE General Discoverable Mode (bit 1) and BR/EDR Not Supported
  // (bit 2) in the Flags AD type (0x01) as required for LE-only devices.
  // CSS Part A, §1.3; BT Core Spec v5.x, Vol 3, Part C, §9.2.4 (discoverable mode).
  // The name is omitted here when scan response is enabled (spec recommends
  // Local Name in the scan response to leave room for service UUIDs in the
  // primary AD). When scan response is disabled, the name goes into the
  // primary AD instead.
  struct ble_hs_adv_fields fields = {};
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

  if (!impl.scanResponseEnabled && name.length() > 0) {
    fields.name = reinterpret_cast<const uint8_t *>(name.c_str());
    fields.name_len = name.length();
    fields.name_is_complete = 1;
  }

  if (impl.includeTxPower) {
    fields.tx_pwr_lvl = BLE.getPower();
    fields.tx_pwr_lvl_is_present = 1;
  }

  if (impl.appearance > 0) {
    fields.appearance = impl.appearance;
    fields.appearance_is_present = 1;
  }

  if (impl.useCustomAdvData) {
    int rc = ble_gap_adv_set_data(impl.customAdvData.data(), impl.customAdvData.length());
    if (rc != 0) {
      log_e("ble_gap_adv_set_data: rc=%d", rc);
      return BTStatus::Fail;
    }
  } else {
    // Build service UUIDs into fields (16, 32, and 128-bit)
    ble_uuid16_t uuid16s[10];
    ble_uuid32_t uuid32s[10];
    ble_uuid128_t uuid128s[4];
    int n16 = 0, n32 = 0, n128 = 0;
    for (auto &uuid : impl.serviceUUIDs) {
      switch (uuid.bitSize()) {
        case 16:
          if (n16 < 10) {
            uuid16s[n16].u.type = BLE_UUID_TYPE_16;
            uuid16s[n16].value = uuid.toUint16();
            n16++;
          }
          break;
        case 32:
          if (n32 < 10) {
            uuid32s[n32].u.type = BLE_UUID_TYPE_32;
            uuid32s[n32].value = uuid.toUint32();
            n32++;
          }
          break;
        default:
        {
          if (n128 < 4) {
            BLEUUID u128 = uuid.to128();
            uuid128s[n128].u.type = BLE_UUID_TYPE_128;
            const uint8_t *be = u128.data();
            for (int i = 0; i < 16; i++) {
              uuid128s[n128].value[15 - i] = be[i];
            }
            n128++;
          }
          break;
        }
      }
    }
    if (n16 > 0) {
      fields.uuids16 = uuid16s;
      fields.num_uuids16 = n16;
      fields.uuids16_is_complete = 1;
    }
    if (n32 > 0) {
      fields.uuids32 = uuid32s;
      fields.num_uuids32 = n32;
      fields.uuids32_is_complete = 1;
    }
    if (n128 > 0) {
      fields.uuids128 = uuid128s;
      fields.num_uuids128 = n128;
      fields.uuids128_is_complete = 1;
    }

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
      log_e("ble_gap_adv_set_fields: rc=%d", rc);
      return BTStatus::Fail;
    }
  }

  if (impl.scanResponseEnabled) {
    if (impl.useCustomScanRsp) {
      int rc = ble_gap_adv_rsp_set_data(impl.customScanRspData.data(), impl.customScanRspData.length());
      if (rc != 0) {
        log_e("ble_gap_adv_rsp_set_data: rc=%d", rc);
        return BTStatus::Fail;
      }
    } else {
      // Auto-build a scan response with the device name, matching
      // Bluedroid's default behavior when scanResp is enabled.
      struct ble_hs_adv_fields rspFields = {};
      if (name.length() > 0) {
        rspFields.name = reinterpret_cast<const uint8_t *>(name.c_str());
        rspFields.name_len = name.length();
        rspFields.name_is_complete = 1;
      }
      int rc = ble_gap_adv_rsp_set_fields(&rspFields);
      if (rc != 0) {
        log_e("ble_gap_adv_rsp_set_fields: rc=%d", rc);
        return BTStatus::Fail;
      }
    }
  } else {
    (void)ble_gap_adv_rsp_set_data(nullptr, 0);
  }

  int32_t duration = (durationMs == 0) ? BLE_HS_FOREVER : (durationMs / 10);
  int rc = ble_gap_adv_start(static_cast<uint8_t>(BLE.getOwnAddressType()), NULL, duration, &advParams, Impl::gapEventCallback, &impl);
  if (rc != 0) {
    log_e("ble_gap_adv_start: rc=%d", rc);
    return BTStatus::Fail;
  }

  log_i("Advertising: started (duration=%u ms, %u service UUID(s))", durationMs, (unsigned)impl.serviceUUIDs.size());
  impl.advertising = true;
  return BTStatus::OK;
}

/**
 * @brief Stops active advertising.
 *
 * The @c advertising flag is always cleared on return, even when
 * @c ble_gap_adv_stop() reports @c BLE_HS_EALREADY (advertising was not
 * running — e.g. after a @c BLE.end() / @c BLE.begin() stack restart).
 * Not clearing the flag on @c EALREADY would leave @c advertising == true,
 * causing a subsequent @c start() to return immediately as a no-op
 * (it guards with @c if (impl.advertising) return OK;).  This matches the
 * pattern used by @c BLEScan::stop() and Bluedroid's @c BLEAdvertising::stop().
 *
 * @return OK on success or when already stopped; Fail on unexpected errors.
 */
BTStatus BLEAdvertising::stop() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!impl.advertising) {
    return BTStatus::OK;
  }
  log_d("Advertising: stop");
  int rc = ble_gap_adv_stop();
  // Always clear the flag — the stack has stopped (or was already stopped).
  // BLE_HS_EALREADY means "not currently advertising" which is our target
  // state; treat it as success, not an error.
  impl.advertising = false;
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    log_e("Advertising: ble_gap_adv_stop rc=%d", rc);
    return BTStatus::Fail;
  }
  log_i("Advertising: stopped");
  return BTStatus::OK;
}

/**
 * @brief Registers a callback invoked when an advertising session completes.
 * @param handler Callback (may be null to clear).
 */
void BLEAdvertising::onComplete(CompleteHandler handler) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onCompleteCb = handler;
}

/**
 * @brief Clears the on-complete callback.
 */
void BLEAdvertising::resetCallbacks() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.onCompleteCb = nullptr;
}

// --------------------------------------------------------------------------
// BLEClass::getAdvertising() + advertising shortcuts
// --------------------------------------------------------------------------

/**
 * @brief Returns a BLEAdvertising object backed by a static shared implementation.
 * @return Valid object if BLE is initialized; otherwise empty.
 */
BLEAdvertising BLEClass::getAdvertising() {
  if (!isInitialized()) {
    log_e("getAdvertising: BLE not initialized");
    return BLEAdvertising();
  }
  static std::shared_ptr<BLEAdvertising::Impl> advImpl;
  if (!advImpl) {
    advImpl = std::make_shared<BLEAdvertising::Impl>();
  }
  return BLEAdvertising(advImpl);
}

/**
 * @brief Starts advertising on the process-wide default BLEAdvertising instance.
 * @return Result of Advertisement start on that instance.
 */
BTStatus BLEClass::startAdvertising() {
  return getAdvertising().start();
}

/**
 * @brief Stops advertising for the default advertising instance.
 * @return Result of stop().
 */
BTStatus BLEClass::stopAdvertising() {
  return getAdvertising().stop();
}

#else /* !BLE_ADVERTISING_SUPPORTED -- stubs */

// Stubs for BLE_ADVERTISING_SUPPORTED == 0: log; start/stop return NotSupported; reset* mostly log.

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

void BLEAdvertising::reset() {
  log_w("Advertising not supported");
}

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

void BLEAdvertising::onComplete(CompleteHandler) {
  log_w("Advertising not supported");
}

void BLEAdvertising::resetCallbacks() {
  log_w("Advertising not supported");
}

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

#endif /* BLE_NIMBLE */
