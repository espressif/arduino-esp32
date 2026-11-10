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
 * @file BLEAudioEngine.bluedroid.h
 * @brief Bluedroid GAP event forwarding into the LE Audio engine.
 *
 * The engine's Bluedroid adapter registers its own BTA GATTS/GATTC apps and
 * receives connection, GATT, scan and periodic-sync events through BTA, so it
 * coexists with the library's GATT server and client. Only the SMP completion
 * has no BTA path and must be forwarded from the application GAP callback.
 */

#include "core/BLEGuards.h"

#if BLE_BLUEDROID && BLE_AUDIO_SUPPORTED

#include <esp_gap_ble_api.h>

namespace BLEAudioEngine {

/** @brief Forward ESP_GAP_BLE_AUTH_CMPL_EVT; no-op for other events or when audio is off. */
void forwardHostGapEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

}  // namespace BLEAudioEngine

#endif /* BLE_BLUEDROID && BLE_AUDIO_SUPPORTED */
