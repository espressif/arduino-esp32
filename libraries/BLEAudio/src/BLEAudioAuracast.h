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
 * @brief Internal: extended advertising payload shared by every Auracast
 *        broadcaster (BroadcastSource and the CAP Initiator broadcast), so the
 *        two assemblies cannot drift apart (they did: different Broadcast Name
 *        limits and quality-bit derivations).
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include "advertising/BLEAdvertisementData.h"
#include "BTStatus.h"

namespace BLEAudioAuracast {

/// Broadcast Name limit (Assigned Numbers 0x30, 4..32 octets).
constexpr uint8_t kMaxNameLen = 32;
/// 80 ms periodic advertising interval; also sent to acceptors, so min = max.
constexpr uint16_t kPaInterval = 0x40;
/// AD type of the BASE service data structure in the periodic payload.
constexpr uint8_t kAdTypeServiceData16 = 0x16;

/**
 * @brief Assemble the shared extended advertising data of an Auracast source:
 *        Flags, Appearance, Broadcast Audio Announcement (0x1852) carrying the
 *        24-bit Broadcast_ID, optional PBP Public Broadcast Announcement
 *        (0x1856), Broadcast Name and Complete Local Name.
 *
 * @param ext           Data object to fill (caller sizes it, usually EXTENDED_MAX_PAYLOAD).
 * @param broadcastId   24-bit Broadcast_ID.
 * @param advName       Broadcast Name / Complete Local Name payload (Broadcast Name
 *                      and PBA Program Info are UTF-8-truncated to 32 octets per PBP §3.1.1).
 * @param withPba       Add the PBP Public Broadcast Announcement.
 * @param audioContext  Streaming Audio Contexts LTV of the PBA (ignored without withPba).
 * @param pbaFeatures   PBP features byte (ignored without withPba).
 */
void buildExt(BLEAdvertisementData &ext, uint32_t broadcastId, const String &advName, bool withPba, uint16_t audioContext, uint8_t pbaFeatures);

}  // namespace BLEAudioAuracast

#endif  // BLE_AUDIO_SUPPORTED
