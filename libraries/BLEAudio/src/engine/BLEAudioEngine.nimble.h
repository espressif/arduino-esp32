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

#endif /* BLE_NIMBLE && BLE_AUDIO_SUPPORTED */
