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
 * @brief Bluedroid backend methods of `BLEAudio::Impl`.
 */

#include "core/BLEGuards.h"
#if BLE_BLUEDROID && BLE_AUDIO_SUPPORTED

#include "BLE.h"
#include "BLE.bluedroid.h"
#include "BLEAudio.bluedroid.h"
#include "engine/BLEAudioEngine.bluedroid.h"
#include "esp32-hal-log.h"

void BLEAudio::Impl::backendOnControllerCreated(BLEClass::Impl &bleImpl) {
  bleImpl.storeHook(bleImpl.audioGapHook, BLEAudioEngine::forwardHostGapEvent);
}

BTStatus BLEAudio::Impl::backendConnect(const BTAddress &address) {
  log_e("Audio: Bluedroid connect to %s reported NotSupported", address.toString().c_str());
  return BTStatus::NotSupported;
}

#endif /* BLE_BLUEDROID && BLE_AUDIO_SUPPORTED */
