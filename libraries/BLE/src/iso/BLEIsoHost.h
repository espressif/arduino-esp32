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
 * @file BLEIsoHost.h
 * @brief IDF host hand-over between the ISO transport and the LE Audio engine.
 *
 * The IDF host (`bt_le_host_init`) and its single GAP app callback can only be
 * claimed once. Either the ISO transport claims it (`esp_ble_iso_common_init`,
 * from the `BLEIso` controller's `begin()`) or the LE Audio engine does
 * (`esp_ble_audio_common_init`, which also brings ISO up). When audio owns the
 * host it calls bleIsoAttachShared() and forwards its GAP events through
 * bleIsoOnGapEvent(), so ISO is never initialized twice.
 *
 * Internal: implemented in `BLEIso.cpp`, called by the C audio engine
 * (`BLEAudioEngine.c`, which cannot include C++ headers because the
 * `esp_ble_audio_*` headers only parse as C) and by the NimBLE GAP forwarder.
 */

#include "core/BLEGuards.h"
#if BLE_ISO_SUPPORTED

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Whether the ISO transport itself claimed the host (controller `begin()` without audio). */
bool bleIsoOwnsHost(void);

/** @brief Whether the IDF host is up (claimed by ISO or shared by audio). */
bool bleIsoHostUp(void);

/**
 * @brief Called by the LE Audio engine after it claims or releases the host.
 * @param attached true after `esp_ble_audio_common_init`, false after deinit
 *        (which also released the ISO layer, so every ISO channel is dropped).
 */
void bleIsoAttachShared(bool attached);

/** @brief GAP app event from the audio engine's callback (`esp_ble_iso_gap_app_event_t *`). */
void bleIsoOnGapEvent(void *event);

#ifdef __cplusplus
}  // extern "C"

/**
 * C++-only ISO transport internals (called from the BLE library's C++ TUs).
 * The four functions above stay global `extern "C"` because the C LE Audio
 * engine calls them; everything else lives here. The NimBLE GAP forwarder
 * (`forwardHostGapEvent`, in `iso/BLEIso.nimble.h`) completes the set.
 */
namespace BLEIsoInternal {

/**
 * @brief Map a public connection handle onto the HCI ACL handle expected by `esp_ble_iso_chan_connect`.
 *
 * Identity on NimBLE; resolves `BLEClient` GATTC connection IDs on Bluedroid.
 */
uint16_t resolveAclHandle(uint16_t connHandle);

/**
 * @brief Whether @p connHandle names a live ACL (false for 0xFFFF and for
 * dead or never-connected links).
 *
 * Checked before a CIG is created so a stale handle fails closed instead of
 * reaching the controller (where a recycled handle could address the wrong
 * peer).
 */
bool aclHandleLive(uint16_t connHandle);

/** @brief End the retained ISO controller, if any; silent when never minted. */
void preEnd();

/** @brief Whether the retained ISO controller, if any, is still active. */
bool isActive();

}  // namespace BLEIsoInternal

#endif /* __cplusplus */

#endif /* BLE_ISO_SUPPORTED */
