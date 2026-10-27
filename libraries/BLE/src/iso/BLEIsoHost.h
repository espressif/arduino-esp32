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
 * from `BLEIso::begin()`) or the LE Audio engine does
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

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Whether the ISO transport itself claimed the host (`BLEIso::begin()` without audio). */
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

/**
 * @brief Release ISO ownership unconditionally, for `BLE.end()`.
 *
 * `BLEIso::end()` keeps its state when a native teardown step fails, so a
 * standalone user can retry. The core cannot retry: it is about to destroy the
 * host, and a transport still claiming ownership then makes every later
 * `audio.begin()` fail with "standalone BLEIso session owns the host". This
 * runs `end()` and then drops the Arduino-side state regardless of the result;
 * the controller teardown that follows releases the native objects anyway.
 */
void bleIsoForceRelease(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_ISO_SUPPORTED */
