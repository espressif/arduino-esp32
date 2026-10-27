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

#if BLE_NIMBLE && BLE_AUDIO_SUPPORTED

#include "audio/BLEAudioEngine.nimble.h"
#include "audio/BLEAudioEngine.h"

#include <host/ble_hs.h>

/**
 * @file BLEAudioEngine.nimble.cpp
 * @brief NimBLE host calls used by the engine's link bring-up, and GATT event forwarding.
 *
 * API contract is documented on the declarations in `BLEAudioEngine.nimble.h`
 * and `BLEAudioEngine.h`; the definitions below carry implementation notes only.
 */

// --------------------------------------------------------------------------
// Link bring-up helpers (called from BLEAudioEngine.c)
// --------------------------------------------------------------------------

/*
 * Implemented here because NimBLE host headers clash with the Zephyr headers of the C units.
 * A procedure already in progress (NimBLE starts one itself when the peer sends a Security
 * Request) counts as started: its completion arrives as the same ENC_CHANGE event.
 */
extern "C" int bleAudioNimbleSecure(uint16_t conn_handle) {
  int rc = ble_gap_security_initiate(conn_handle);
  return rc == BLE_HS_EALREADY ? 0 : rc;
}

/*
 * An exchange already sent on this link (in progress or completed, e.g. by the library's
 * GATT client) counts as success: every MTU event starts discovery.
 */
extern "C" int bleAudioNimbleExchangeMtu(uint16_t conn_handle) {
  int rc = ble_gattc_exchange_mtu(conn_handle, nullptr, nullptr);
  return rc == BLE_HS_EALREADY ? 0 : rc;
}

namespace BLEAudioEngine {

// --------------------------------------------------------------------------
// GATT event forwarding
// --------------------------------------------------------------------------

/*
 * Called from the BLEServer and BLEClient connection handlers, right after
 * the ISO GAP forwarding. The engine consumes the event synchronously, so the
 * NimBLE event can be passed as is.
 */
void forwardHostGattEvent(struct ble_gap_event *event) {
  if (event == nullptr || !bleAudioEngineIsInitialized()) {
    return;
  }
  switch (event->type) {
    case BLE_GAP_EVENT_MTU:
    case BLE_GAP_EVENT_NOTIFY_RX:
    case BLE_GAP_EVENT_NOTIFY_TX:
    case BLE_GAP_EVENT_SUBSCRIBE:  bleAudioEngineGattPostEvent((uint8_t)event->type, event); break;
    default:                       break;
  }
}

}  // namespace BLEAudioEngine

#endif /* BLE_NIMBLE && BLE_AUDIO_SUPPORTED */
