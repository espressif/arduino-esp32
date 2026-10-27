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
  if (event == ESP_GAP_BLE_AUTH_CMPL_EVT && param != nullptr && bleAudioEngineIsInitialized()) {
    bleAudioEngineGapPostEvent((uint16_t)event, param);
  }
}

}  // namespace BLEAudioEngine

#endif /* BLE_BLUEDROID && BLE_AUDIO_SUPPORTED */
