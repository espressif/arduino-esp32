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
 * @brief Profiles engine unit: TMAP / GMAP identity services and PBP announcements.
 *
 * TMAS and GMAS join the single GATT commit, so their register calls must run
 * before bleAudioEngineStart(). Discovery needs a connected peer whose GATT
 * discovery finished (BLE_AUDIO_EVT_GATT_DISCOVERED). Role and feature values
 * are the spec bitfields (TMAP Role, GMAP Role, UGG/UGT/BGS/BGR Features).
 */

#include "audio/BLEAudioEngine.h"
#if BLE_AUDIO_SUPPORTED

#ifdef __cplusplus
extern "C" {
#endif

/* ── TMAP ───────────────────────────────────────────────────────────────── */

enum {
  BLE_AUDIO_EVT_TMAP_DISCOVERED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_TMAP, 0), /* ble_audio_tmap_discovered_t */
};

typedef struct {
  uint16_t roles; /*!< Peer TMAP roles, 0 on failure. */
} ble_audio_tmap_discovered_t;

/** @brief Add the TMAS instance with @p roles (non-zero). */
int bleAudioTmapRegister(uint16_t roles);
/** @brief Read the peer's TMAP roles (BLE_AUDIO_EVT_TMAP_DISCOVERED). */
int bleAudioTmapDiscover(uint16_t conn_handle);

/* ── GMAP ───────────────────────────────────────────────────────────────── */

enum {
  BLE_AUDIO_EVT_GMAP_DISCOVERED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_GMAP, 0), /* ble_audio_gmap_discovered_t */
};

typedef struct {
  uint8_t ugg; /*!< Unicast Game Gateway features. */
  uint8_t ugt; /*!< Unicast Game Terminal features. */
  uint8_t bgs; /*!< Broadcast Game Sender features. */
  uint8_t bgr; /*!< Broadcast Game Receiver features. */
} ble_audio_gmap_features_t;

typedef struct {
  uint8_t roles; /*!< Peer GMAP roles, 0 on failure. */
  ble_audio_gmap_features_t features;
} ble_audio_gmap_discovered_t;

/**
 * @brief Add the GMAS instance with @p roles and the features of those roles.
 *
 * The stack rejects (ESP_ERR_INVALID_ARG) a role whose CONFIG_BT_GMAP_*_SUPPORTED
 * is off and feature bits the ASE / stream counts cannot back (e.g. UGT needs
 * Sink or Source).
 */
int bleAudioGmapRegister(uint8_t roles, const ble_audio_gmap_features_t *features);
/** @brief Read the peer's GMAP roles and features (BLE_AUDIO_EVT_GMAP_DISCOVERED). */
int bleAudioGmapDiscover(uint16_t conn_handle);

/* ── PBP ────────────────────────────────────────────────────────────────── */

/** UUID16 + features + metadata length. */
#define BLE_AUDIO_PBA_HDR_LEN 4

/**
 * @brief Build a Public Broadcast Announcement service-data value.
 *
 * @p out receives the 0x1856 UUID (little-endian), the features, the metadata
 * length and the metadata: the payload of one Service Data 16-bit AD structure.
 * @return ESP_ERR_NOT_SUPPORTED without CONFIG_BT_PBP, ESP_ERR_NO_MEM when
 *         @p cap < BLE_AUDIO_PBA_HDR_LEN + @p meta_len.
 */
int bleAudioPbpBuild(uint8_t features, const uint8_t *meta, size_t meta_len, uint8_t *out, size_t cap, size_t *out_len);

/**
 * @brief Parse a Service Data 16-bit value (starting with its UUID) as a PBA.
 * @param meta Set to the metadata inside @p data (valid while @p data is).
 */
int bleAudioPbpParse(const uint8_t *data, size_t len, uint8_t *features, const uint8_t **meta, size_t *meta_len);

#ifdef __cplusplus
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
