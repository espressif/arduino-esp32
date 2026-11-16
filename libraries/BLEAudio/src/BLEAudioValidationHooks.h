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

#include "core/BLEGuards.h"

#ifndef BLE_AUDIO_VALIDATION_HOOKS
#define BLE_AUDIO_VALIDATION_HOOKS 0
#endif

#if BLE_AUDIO_VALIDATION_HOOKS && BLE_AUDIO_SUPPORTED

#include <stdbool.h>
#include <stdint.h>

/**
 * @file
 * @brief Test-only injectors for `tests/validation/ble_audio/`. Not a public API.
 *
 * Compiled in only when @c BLE_AUDIO_VALIDATION_HOOKS is set. Production
 * builds compile this header to nothing.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Arm the engine's pending-connect target without opening a real ACL.
 * @param addr Engine-order (LSB-first) address.
 * @return true when the target was armed.
 */
bool bleAudioValidationArmConnect(const uint8_t addr[6], uint8_t addr_type);

/** @return true when @p addr is the armed pending-connect target. */
bool bleAudioValidationAcceptPeer(const uint8_t addr[6], uint8_t addr_type);

/** Cancel the armed pending-connect target. */
void bleAudioValidationCancelConnect(void);

/** Make the next CAP handover report an asynchronous failure after 50 ms. */
void bleAudioValidationFailNextHandover(void);

/**
 * Drop (true) or deliver (false, default) the recorder's controller-release
 * notifications, simulating a stalled ISO controller without touching the
 * link. While blocked, the pacing logic resyncs and counts txStalls instead
 * of spinning silently.
 */
void bleAudioValidationBlockReleases(bool block);

#ifdef __cplusplus
}
#endif

#endif /* BLE_AUDIO_VALIDATION_HOOKS && BLE_AUDIO_SUPPORTED */
