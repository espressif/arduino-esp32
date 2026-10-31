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

#include "core/BLEGuards.h"

#if BLE_BLUEDROID && BLE_AUDIO_SUPPORTED

#include "audio/BLEAudioEngine.bluedroid.h"
#include "audio/BLEAudioEngine.h"
#include "BLE.h"
#include "scan/BLEScan.bluedroid.h"
#include "BTAddress.h"
#include "esp32-hal-log.h"

/**
 * @file BLEAudioEngine.bluedroid.cpp
 * @brief Bluedroid SMP completion forwarding into the LE Audio engine.
 *
 * API contract is documented on the declarations in `BLEAudioEngine.bluedroid.h`;
 * the definitions below carry implementation notes only.
 */

namespace BLEAudioEngine {

/*
 * Called from the library's GAP callback for every event. The engine starts
 * GATT discovery on a link once its encryption completes.
 */
void forwardHostGapEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
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
  // Audio links are encrypted before discovery; without this the failure only
  // shows up later as a rejected PACS/ASCS read.
  log_d(
    "audio link: pairing complete success=%d reason=0x%02x auth_mode=%d", (int)param->ble_security.auth_cmpl.success,
    (unsigned)param->ble_security.auth_cmpl.fail_reason, (int)param->ble_security.auth_cmpl.auth_mode
  );
  if (bleAudioEngineIsInitialized()) {
    bleAudioEngineGapPostEvent((uint16_t)event, param);
  }
}

}  // namespace BLEAudioEngine

extern "C" int bleAudioEngineReceivePeriodicSync(uint16_t conn_handle, uint8_t peer_addr_type, const uint8_t peer_addr[6], uint16_t skip,
                                                   uint16_t timeout_ms) {
  if (!peer_addr) {
    return static_cast<int>(BTStatus::InvalidParam);
  }
#if BLE_SCANNING_SUPPORTED
  (void)BLE.getScan();
  BTAddress address(peer_addr, static_cast<BTAddress::Type>(peer_addr_type));
  return static_cast<int>(BLEScan::Impl::receivePeriodicSyncFromPeer(conn_handle, address, skip, timeout_ms));
#else
  (void)conn_handle;
  (void)peer_addr_type;
  (void)skip;
  (void)timeout_ms;
  return static_cast<int>(BTStatus::NotSupported);
#endif
}

// Bluedroid registers the classic server and LE Audio as separate GATTS apps,
// so no cross-owner GATT commit coordination is required.
extern "C" int bleAudioHostPrepareGatt(void) {
  return 0;
}

extern "C" void bleAudioHostFinishGatt(bool) {}

extern "C" void bleAudioHostResetGatt(void) {}

#endif /* BLE_BLUEDROID && BLE_AUDIO_SUPPORTED */
