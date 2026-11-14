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
 * @file BLEAudioAuracast.cpp
 * @brief Shared extended advertising payload of the Auracast broadcasters.
 *
 * Includes BLE.h for the advertising UUID helpers; this is C++ glue on top of
 * the library's own BLEAdvertising, so it behaves the same on NimBLE and
 * Bluedroid.
 */

#include "BLEAudioAuracast.h"
#if BLE_AUDIO_SUPPORTED

#include "BLE.h"
#include "BLEAudioProfiles.h"

namespace {

constexpr uint16_t kBroadcastAudioAnnouncementUuid = 0x1852;   ///< Carries the 24-bit Broadcast ID.
constexpr uint16_t kPublicBroadcastAnnouncementUuid = 0x1856;  ///< PBP: features + metadata.
constexpr uint16_t kAppearanceBroadcastingDevice = 0x0885;
constexpr uint8_t kMetaStreamingContext = 0x02;  ///< LTV type: Streaming Audio Contexts.
constexpr uint8_t kMetaProgramInfo = 0x03;       ///< LTV type: Program Info (UTF-8).

}  // namespace

namespace BLEAudioAuracast {

BTStatus buildExt(BLEAdvertisementData &ext, uint32_t broadcastId, const String &advName, bool withPba, uint16_t audioContext, uint8_t pbaFeatures) {
  ext.setFlags(BLEAdvFlag::GeneralDisc | BLEAdvFlag::BrEdrNotSupported);
  ext.setAppearance(kAppearanceBroadcastingDevice);
  const uint8_t id[3] = {(uint8_t)broadcastId, (uint8_t)(broadcastId >> 8), (uint8_t)(broadcastId >> 16)};
  ext.setServiceData(BLEUUID(kBroadcastAudioAnnouncementUuid), id, sizeof(id));
#if BLE_AUDIO_PBP_SUPPORTED
  if (withPba) {
    // PBA metadata: Streaming Audio Contexts + Program Info (the name), so
    // Auracast UIs show the same context and title as a plain broadcast source.
    const size_t n = advName.length() < kMaxNameLen ? advName.length() : kMaxNameLen;
    uint8_t meta[4 + 2 + kMaxNameLen];
    size_t m = 0;
    meta[m++] = 3;
    meta[m++] = kMetaStreamingContext;
    meta[m++] = (uint8_t)audioContext;
    meta[m++] = (uint8_t)(audioContext >> 8);
    meta[m++] = (uint8_t)(1 + n);
    meta[m++] = kMetaProgramInfo;
    memcpy(meta + m, advName.c_str(), n);
    m += n;
    uint8_t pba[2 + 2 + sizeof(meta)];
    const size_t pbaLen = BLEAudioPublicBroadcast::buildAnnouncement(pbaFeatures, meta, m, pba, sizeof(pba));
    if (pbaLen > 2) {
      // buildAnnouncement() starts with the UUID, which setServiceData() adds itself.
      ext.setServiceData(BLEUUID(kPublicBroadcastAnnouncementUuid), pba + 2, pbaLen - 2);
    }
  }
#else
  (void)withPba;
  (void)audioContext;
  (void)pbaFeatures;
#endif
  // Scanners disagree on the title: Samsung Listen uses the Broadcast Name,
  // others the Complete Local Name, so publish both.
  ext.setBroadcastName(advName);
  ext.setName(advName);
  return BTStatus::OK;
}

}  // namespace BLEAudioAuracast

#endif  // BLE_AUDIO_SUPPORTED
