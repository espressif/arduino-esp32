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
 * @file BLEIso.nimble.cpp
 * @brief NimBLE GAP event forwarding into the shared IDF ISO/Audio host.
 *
 * API contract is documented on the declaration in `BLEIso.nimble.h`; the
 * definition below carries implementation notes only.
 */

#include "core/BLEGuards.h"

#if BLE_NIMBLE && BLE_ISO_SUPPORTED

#include "iso/BLEIso.nimble.h"
#include "iso/BLEIsoHost.h"

#include "esp_ble_iso_common_api.h"
#include <host/ble_gap.h>

namespace BLEIso {

void forwardHostGapEvent(struct ble_gap_event *event) {
  if (event == nullptr || !bleIsoHostUp()) {
    return;
  }

  switch (event->type) {
    // ACL lifecycle: audio link bring-up (encryption, discovery) and per-link cleanup.
    case BLE_GAP_EVENT_CONNECT:
    case BLE_GAP_EVENT_DISCONNECT:
    case BLE_GAP_EVENT_ENC_CHANGE:
#if BLE5_SUPPORTED
    // Extended scan reports feed broadcast source discovery; periodic sync
    // events carry the BASE and BIGInfo that the BIS receiver and broadcast
    // sink wait for, including syncs received by PAST.
    case BLE_GAP_EVENT_EXT_DISC:
    case BLE_GAP_EVENT_PERIODIC_SYNC:
    case BLE_GAP_EVENT_PERIODIC_REPORT:
    case BLE_GAP_EVENT_PERIODIC_SYNC_LOST:
#ifdef BLE_GAP_EVENT_PERIODIC_TRANSFER
    case BLE_GAP_EVENT_PERIODIC_TRANSFER:
#endif
#ifdef BLE_GAP_EVENT_PERIODIC_TRANSFER_V2
    case BLE_GAP_EVENT_PERIODIC_TRANSFER_V2:
#endif
#endif
      esp_ble_iso_gap_app_post_event(event->type, event);
      break;
    default: break;
  }
}

}  // namespace BLEIso

#endif /* BLE_NIMBLE && BLE_ISO_SUPPORTED */
