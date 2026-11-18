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
 * @file
 * @brief Weak hooks from the core BLE library into the separate BLEAudio library.
 *
 * The core must not include or link the LE Audio engine: a sketch that only
 * uses `<BLE.h>` does not compile `BLEAudio`. These hooks are declared weak, so
 * the core code can call them unconditionally and the linker resolves them to
 * null when the BLEAudio library is not part of the build. BLEAudio defines
 * `bleAudioPreEndHook` in `BLEAudio.cpp` and the GATT pair
 * (`bleAudioGattActive` / `bleAudioGattFrozen`) in both
 * `BLEAudio.nimble.cpp` and `BLEAudio.bluedroid.cpp`; only NimBLE needs the
 * GATT event forwarder (`bleAudioForwardHostGattEvent` in
 * `BLEAudio.nimble.cpp`), Bluedroid feeds the engine through its own GATTS/GATTC apps.
 */

#include "core/BLEGuards.h"

#if BLE_AUDIO_SUPPORTED

extern "C" {

struct ble_gap_event;

/**
 * @brief Tear down an active audio controller before the host goes down.
 * @return true when no audio controller is live or its teardown succeeded.
 */
bool bleAudioPreEndHook() __attribute__((weak));

/**
 * @brief Forward a NimBLE GATT event to the audio engine.
 *
 * NimBLE has no global GATT callback; the core connection handlers forward
 * MTU/subscribe/notification events here. Null when audio is not linked.
 */
void bleAudioForwardHostGattEvent(struct ble_gap_event *event) __attribute__((weak));

/** @brief Whether the audio engine currently owns the NimBLE GATT commit. */
bool bleAudioGattActive() __attribute__((weak));

/** @brief Whether the audio engine has already committed the GATT table (freeze). Enforced on both NimBLE and Bluedroid: classic services cannot restage while frozen. */
bool bleAudioGattFrozen() __attribute__((weak));

}  // extern "C"

#endif /* BLE_AUDIO_SUPPORTED */
