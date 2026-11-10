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

#include "BLE.h"
#include "BLE.bluedroid.h"
#include "BLEAudioImpl.h"
#include "engine/BLEAudioEngine.h"
#include "engine/BLEAudioEngine.bluedroid.h"
#include "core/BLEAudioHooks.h"
#include "core/BLEMutex.h"
#include "esp32-hal-log.h"

namespace {
BLEAudio &audioControllerSlot() {
  static BLEAudio slot;
  return slot;
}
}  // namespace

extern "C" bool bleAudioPreEndHook() {
  auto &audio = audioControllerSlot();
  if (!audio) {
    return true;
  }
  return static_cast<bool>(audio.end());
}

BLEAudio BLEClass::getAudioController() {
  BLELockGuard lifecycle(_impl ? _impl->lifecycleMtx : nullptr);
  if (!isInitialized() || _impl->ending) {
    log_e("getAudioController: BLE not initialized");
    return BLEAudio();
  }
  auto &slot = audioControllerSlot();
  if (!slot) {
    slot = BLEAudio(std::make_shared<BLEAudio::Impl>());
    _impl->storeHook(_impl->audioGapHook, BLEAudioEngine::forwardHostGapEvent);
  }
  return BLEAudio(slot);
}

#endif /* BLE_BLUEDROID && BLE_AUDIO_SUPPORTED */
