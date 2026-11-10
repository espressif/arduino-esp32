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

#pragma once

/**
 * @file BLEAudioEngine.nimble.h
 * @brief NimBLE GATT event forwarding into the LE Audio engine.
 *
 * NimBLE has no global GATT callback, so ASCS/PACS/profile clients only see
 * what the connection handlers forward. GAP events (connect, disconnect,
 * encryption, periodic sync) share one sink with the ISO layer and are
 * forwarded by `BLEIso::forwardHostGapEvent()`; forwarding them here as well
 * would deliver them twice.
 */

#include "core/BLEGuards.h"

#if BLE_NIMBLE && BLE_AUDIO_SUPPORTED

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NimBLE host calls used by the link bring-up in BLEAudioEngine.c. NimBLE
 * headers clash with the Zephyr headers of the C units, so they are
 * implemented in BLEAudioEngine.nimble.cpp.
 */

/** @brief Start pairing/encryption on a central link (`ble_gap_security_initiate`). */
int bleAudioNimbleSecure(uint16_t conn_handle);
/**
 * @brief Start the ATT MTU exchange (`ble_gattc_exchange_mtu`); discovery runs on the MTU event.
 * @return 0 also when an exchange is already in progress.
 */
int bleAudioNimbleExchangeMtu(uint16_t conn_handle);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
struct ble_gap_event;

namespace BLEAudioEngine {

/**
 * @brief Forward MTU / NOTIFY_RX / NOTIFY_TX / SUBSCRIBE to the engine.
 *
 * No-op while the engine is not initialized, so connection handlers call it
 * unconditionally.
 */
void forwardHostGattEvent(struct ble_gap_event *event);

}  // namespace BLEAudioEngine
#endif /* __cplusplus */

#endif /* BLE_NIMBLE && BLE_AUDIO_SUPPORTED */
