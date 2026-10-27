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
 * @brief Unified GATT registration coordinator (NimBLE).
 *
 * NimBLE cannot support two independent GATT owners: two `ble_gatts_start()`
 * calls wipe each other (`ble_att_svr_start` reallocates the attribute pool),
 * and `ble_svc_gap_init` / `ble_svc_gatt_init` are non-idempotent. So there
 * must be exactly ONE commit for the whole attribute table, into which every
 * contributor (the classic `BLEServer` and the LE Audio engine) stages its
 * services first.
 *
 * This coordinator owns that single lifecycle. It has two modes:
 *
 *  - **Standalone** (no audio): the coordinator itself performs the classic
 *    `reset -> svc_gap_init -> svc_gatt_init -> add_svcs -> ble_gatts_start`
 *    sequence, exactly as the previous `BLEServer::start()` did. Behaviour for
 *    non-audio sketches is unchanged.
 *
 *  - **Audio** (engine present): the LE Audio engine does `svc_gap_init` /
 *    `svc_gatt_init` (in `esp_ble_audio_common_init`) and the single
 *    `ble_gatts_start` (in `esp_ble_audio_common_start`). Here the coordinator
 *    must NOT reset or start; `BLEServer::start()` only *stages* its services
 *    (`ble_gatts_add_svcs`) and the engine's `common_start` commits everything
 *    at once via `BLEAudio::start()`.
 *
 * The coordinator does not carry a separate "audio mode" flag: `audioModeActive()`
 * is derived from the LE Audio engine's own lifecycle (`bleAudioEngineIsInitialized()`),
 * so audio mode is exactly the window between `audio.begin()` and `audio.end()`.
 * This keeps the mode single-sourced in the engine and leaves the engine itself
 * free of any GATT-coordinator (i.e. NimBLE-only) knowledge.
 *
 * Required ordering in audio mode (documented in AUDIO.md):
 *   BLE.begin() -> audio.begin() (common_init) -> create/stage BLEServer
 *   services -> audio.start() (single commit).
 *
 * NimBLE-only: on Bluedroid, incremental multi-app GATTS registration already
 * lets audio coexist through a separate GATTS app, so the Bluedroid server
 * path stays thin and does not use this coordinator.
 */

#include "core/BLEGuards.h"
#if BLE_NIMBLE

#include "BTStatus.h"
#include "server/BLEServer.h"

namespace BLEGattDatabase {

/**
 * @brief Whether the LE Audio engine currently owns the GATT commit.
 *
 * Derived from `bleAudioEngineIsInitialized()` when the LE Audio engine is
 * compiled in; always false otherwise. When true, `stageServer()` must be used
 * (the engine performs the single commit); when false, `commitServerStandalone()`
 * performs the full rebuild.
 */
bool audioModeActive();

/**
 * @brief Stage a server's services without committing (audio mode).
 *
 * Performs `ble_gatts_add_svcs` only -- no reset, no init, no start. The
 * accumulated table is committed later by the engine's single
 * `ble_gatts_start` (triggered from BLEAudio::start()).
 *
 * @param impl Server state whose @c services are staged; marks them started.
 * @return BTStatus::OK on success, or BTStatus::Fail on a registration error.
 */
BTStatus stageServer(BLEServer::Impl &impl);

/**
 * @brief Fully register a server's GATT database standalone (no audio).
 *
 * Runs the classic `reset -> svc_gap_init -> svc_gatt_init -> add_svcs ->
 * ble_gatts_start` sequence, sets the GAP device name, and marks the server
 * and its services started. This is the behaviour the previous
 * `nimbleRebuildGattDatabase()` provided.
 *
 * @param impl Server state to register.
 * @return BTStatus::OK on success, or BTStatus::Fail on a registration/start error.
 */
BTStatus commitServerStandalone(BLEServer::Impl &impl);

}  // namespace BLEGattDatabase

#endif /* BLE_NIMBLE */
