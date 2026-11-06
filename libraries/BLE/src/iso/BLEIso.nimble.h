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
 * @file BLEIso.nimble.h
 * @brief NimBLE GAP event forwarding into the shared IDF ISO/Audio host.
 *
 * NimBLE has no global GAP callback, so the IDF host (claimed by `BLEIso` or
 * by `BLEAudio`) only sees the events the application forwards. ISO and audio
 * share one GAP sink, so every GAP event is forwarded exactly once, from here.
 * GATT events are forwarded separately by `BLEAudioEngine.nimble.h`.
 */

#include "core/BLEGuards.h"

#if BLE_NIMBLE && BLE_ISO_SUPPORTED

struct ble_gap_event;

namespace BLEIso {

/**
 * @brief Forward a NimBLE GAP event (connect/disconnect/encryption, extended
 *        scan reports, periodic sync lifecycle) into the IDF host.
 *
 * No-op when neither ISO nor audio owns the host. Call it from every NimBLE
 * GAP handler (server, client and scanner).
 */
void forwardHostGapEvent(struct ble_gap_event *event);

}  // namespace BLEIso

#endif /* BLE_NIMBLE && BLE_ISO_SUPPORTED */
