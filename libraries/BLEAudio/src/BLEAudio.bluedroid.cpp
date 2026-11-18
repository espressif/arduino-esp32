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
 * @file BLEAudio.bluedroid.cpp
 * @brief Single Bluedroid backend unit for `BLEAudio`:
 *  1. Host GAP event forwarder (`audioGapHook`).
 *  2. `BLEAudio::Impl` Bluedroid methods.
 *  3. Pure-C host glue hooks (`bleAudioHost*`, `bleAudioEngine*`) consumed by `src/engine/`.
 *  4. Periodic Advertising Sync Transfer (PAST) hooks & feature stubs.
 */

#include "core/BLEGuards.h"
#if BLE_BLUEDROID && BLE_AUDIO_SUPPORTED

#include "BLE.h"
#include "BLE.bluedroid.h"
#include "BLEAudio.bluedroid.h"
#include "BTAddress.h"
#include "core/BLEAddressUtil.h"
#include "engine/BLEAudioEngine.h"
#include "scan/BLEScan.bluedroid.h"
#include "esp32-hal-log.h"

#include <cstring>
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>

extern "C" uint8_t esp_ble_audio_bluedroid_get_gattc_if(void);

// ==========================================================================
// 1. Host GAP event forwarder
// ==========================================================================

namespace {

void bluedroidForwardAudioGapEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  if (param == nullptr) {
    return;
  }
#if BLE_PAST_SUPPORTED
  if (event == ESP_GAP_BLE_PERIODIC_ADV_SET_INFO_TRANS_COMPLETE_EVT) {
    if (param->period_adv_set_info_trans.status != ESP_BT_STATUS_SUCCESS) {
      log_w("audio link: PAST set-info transfer failed status=%d", param->period_adv_set_info_trans.status);
    }
    return;
  }
  if (event == ESP_GAP_BLE_PERIODIC_ADV_SYNC_TRANS_COMPLETE_EVT) {
    if (param->period_adv_sync_trans.status != ESP_BT_STATUS_SUCCESS) {
      log_w("audio link: PAST sync transfer failed status=%d", param->period_adv_sync_trans.status);
    }
    return;
  }
#endif
  if (event != ESP_GAP_BLE_AUTH_CMPL_EVT) {
    return;
  }
  log_d(
    "audio link: pairing complete success=%d reason=0x%02x auth_mode=%d", (int)param->ble_security.auth_cmpl.success,
    (unsigned)param->ble_security.auth_cmpl.fail_reason, (int)param->ble_security.auth_cmpl.auth_mode
  );
  if (bleAudioEngineIsInitialized()) {
    bleAudioEngineGapPostEvent(static_cast<uint16_t>(event), param);
  }
}

}  // namespace

// ==========================================================================
// 2. BLEAudio::Impl Bluedroid methods
// ==========================================================================

void BLEAudio::Impl::backendOnControllerCreated(BLEClass::Impl &bleImpl) {
  bleImpl.storeHook(bleImpl.audioGapHook, bluedroidForwardAudioGapEvent);
}

BTStatus BLEAudio::Impl::backendConnect(const BTAddress &address) {
  // Bluedroid central links open directly via bleAudioHostOpenLink().
  log_e("Audio: Bluedroid connect to %s reported NotSupported", address.toString().c_str());
  return BTStatus::NotSupported;
}

// NimBLE tears down its owned BLEClient links here; Bluedroid central links
// are owned by the engine GATTC interface instead, so drop every tracked ACL
// through the engine (same fire-and-forget semantics: the disconnect
// completes asynchronously while end() proceeds to deinit).
void BLEAudio::Impl::backendPreEndTeardown() {
  bleAudioEngineDisconnectAll();
}

// The freeze is an engine state, not a NimBLE one: an audio-owned GATT table
// stays committed until audio.end() on both hosts, so the classic server
// observes the same staging window everywhere.
extern "C" bool bleAudioGattActive() {
  return bleAudioEngineIsInitialized();
}

extern "C" bool bleAudioGattFrozen() {
  return bleAudioEngineIsStarted();
}

// ==========================================================================
// 3. Engine C host hooks (engine/BLEAudioEngine.h)
// ==========================================================================

extern "C" int bleAudioHostSecureLink(uint16_t, const uint8_t host_addr[6], uint8_t) {
  if (host_addr == nullptr) {
    return ESP_ERR_INVALID_ARG;
  }
  esp_bd_addr_t bda = {};
  memcpy(bda, host_addr, sizeof(bda));
  return static_cast<int>(esp_ble_set_encryption(bda, ESP_BLE_SEC_ENCRYPT_NO_MITM));
}

extern "C" int bleAudioHostExchangeMtu(uint16_t) {
  // Bluedroid exchanges the ATT MTU automatically when the engine's GATTC opens the link.
  return 0;
}

extern "C" int bleAudioHostOpenLink(uint8_t addr_type, const uint8_t addr[6]) {
  esp_gatt_if_t gattc_if = esp_ble_audio_bluedroid_get_gattc_if();
  if (gattc_if == ESP_GATT_IF_NONE) {
    log_e("engine GATTC interface not registered; cannot open the link");
    return ESP_ERR_INVALID_STATE;
  }
  esp_bd_addr_t bda;
  bleAudioAddrFromGap(bda, addr);
  esp_err_t err = esp_ble_gattc_aux_open(gattc_if, bda, static_cast<esp_ble_addr_type_t>(addr_type), true);
  if (err != ESP_OK) {
    log_e("esp_ble_gattc_aux_open failed: %d", err);
  }
  return static_cast<int>(err);
}

extern "C" void bleAudioAddrFromGap(uint8_t out[6], const uint8_t in[6]) {
  for (int i = 0; i < 6; i++) {
    out[i] = in[5 - i];
  }
}

extern "C" int bleAudioEngineDisconnect(uint16_t conn_handle) {
  BTAddress peer;
  uint8_t lsb[6];
  if (bleAudioEngineLinkAddress(conn_handle, lsb)) {
    peer = BTAddress(lsb);
  } else if (!bluedroidFindPeerAddress(conn_handle, peer)) {
    return static_cast<int>(BTStatus::NotConnected);
  }

  esp_bd_addr_t bda;
  BLEAddressUtil::toNative(peer, bda);
  esp_err_t err = esp_ble_gap_disconnect(bda);
  if (err == ESP_OK) {
    return 0;
  }
  if (err == ESP_ERR_INVALID_STATE) {
    return static_cast<int>(BTStatus::NotConnected);
  }
  log_e("audio link: disconnect handle=%u: %s", (unsigned)conn_handle, esp_err_to_name(err));
  return static_cast<int>(BTStatus::Fail);
}

extern "C" int bleAudioHostPrepareInit(void) {
  return 0;
}

extern "C" int bleAudioHostPrepareGatt(void) {
  return 0;
}

extern "C" void bleAudioHostFinishGatt(bool) {}

extern "C" void bleAudioHostResetGatt(void) {}

// ==========================================================================
// 4. Periodic Advertising Sync Transfer (PAST) hooks & feature stubs
// ==========================================================================

#if BLE_SCANNING_SUPPORTED

extern "C" int bleAudioEngineReceivePeriodicSync(uint16_t conn_handle, uint8_t peer_addr_type, const uint8_t peer_addr[6], uint16_t skip, uint16_t timeout_ms) {
  if (!peer_addr) {
    return static_cast<int>(BTStatus::InvalidParam);
  }
  (void)BLE.getScan();
  BTAddress address(peer_addr, static_cast<BTAddress::Type>(peer_addr_type));
  return static_cast<int>(BLEScan::Impl::receivePeriodicSyncFromPeer(conn_handle, address, skip, timeout_ms));
}

#else /* !BLE_SCANNING_SUPPORTED */

extern "C" int bleAudioEngineReceivePeriodicSync(uint16_t, uint8_t, const uint8_t peer_addr[6], uint16_t, uint16_t) {
  return peer_addr ? static_cast<int>(BTStatus::NotSupported) : static_cast<int>(BTStatus::InvalidParam);
}

#endif /* BLE_SCANNING_SUPPORTED */

#if BLE_SCANNING_SUPPORTED && BLE_PAST_SUPPORTED

extern "C" int bleAudioEngineStopPeriodicSyncReceive(uint16_t conn_handle) {
  (void)BLE.getScan();
  BTAddress peer;
  uint8_t lsb[6];
  if (bleAudioEngineLinkAddress(conn_handle, lsb)) {
    peer = BTAddress(lsb);
  } else if (!bluedroidFindPeerAddress(conn_handle, peer)) {
    return static_cast<int>(BTStatus::NotConnected);
  }
  return static_cast<int>(BLEScan::Impl::setPeriodicSyncTransferParams(conn_handle, peer, ESP_BLE_GAP_PAST_MODE_NO_SYNC_EVT, 0, 100));
}

#else /* !(BLE_SCANNING_SUPPORTED && BLE_PAST_SUPPORTED) */

extern "C" int bleAudioEngineStopPeriodicSyncReceive(uint16_t) {
  return static_cast<int>(BTStatus::NotSupported);
}

#endif /* BLE_SCANNING_SUPPORTED && BLE_PAST_SUPPORTED */

#if BLE_PAST_SUPPORTED

extern "C" int bleAudioEngineSendPast(uint8_t adv, uint16_t conn, uint16_t service_data, const uint8_t peer_addr[6], uint8_t peer_addr_type) {
  (void)conn;
  if (peer_addr == nullptr) {
    return ESP_ERR_INVALID_ARG;
  }
  BTAddress peer(peer_addr, static_cast<BTAddress::Type>(peer_addr_type));
  uint8_t native[6];
  BLEAddressUtil::toNative(peer, native);
  esp_err_t err = esp_ble_gap_periodic_adv_set_info_trans(native, service_data, adv);
  return err == ESP_OK ? 0 : static_cast<int>(err);
}

extern "C" int bleAudioEngineSendPastSync(uint16_t sync_handle, uint16_t conn, uint16_t service_data, const uint8_t peer_addr[6], uint8_t peer_addr_type) {
  (void)conn;
  if (peer_addr == nullptr) {
    return ESP_ERR_INVALID_ARG;
  }
  BTAddress peer(peer_addr, static_cast<BTAddress::Type>(peer_addr_type));
  uint8_t native[6];
  BLEAddressUtil::toNative(peer, native);
  esp_err_t err = esp_ble_gap_periodic_adv_sync_trans(native, service_data, sync_handle);
  return err == ESP_OK ? 0 : static_cast<int>(err);
}

#else /* !BLE_PAST_SUPPORTED */

extern "C" int bleAudioEngineSendPast(uint8_t, uint16_t, uint16_t, const uint8_t[6], uint8_t) {
  return ESP_ERR_NOT_SUPPORTED;
}

extern "C" int bleAudioEngineSendPastSync(uint16_t, uint16_t, uint16_t, const uint8_t[6], uint8_t) {
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* BLE_PAST_SUPPORTED */

#endif /* BLE_BLUEDROID && BLE_AUDIO_SUPPORTED */
