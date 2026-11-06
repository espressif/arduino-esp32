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
 * @brief BAP Broadcast Assistant engine unit (BASS client).
 *
 * The assistant drives a remote Scan Delegator (a broadcast sink) over one
 * ACL: it discovers the remote BASS, reports broadcast sources found by the
 * local scanner while a remote scan is active, and adds / modifies / removes
 * sources in the remote receive states. When a receive state asks for
 * SyncInfo and this device holds a PA sync to that source, the sync is handed
 * over with PAST (BLE_AUDIO_BA_OP_PAST result).
 *
 * Addresses are LSB-first on both hosts; address types are BT_ADDR_LE_*.
 */

#include "audio/BLEAudioEngine.h"
#if BLE_AUDIO_SUPPORTED

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_AUDIO_BA_MAX_SUBGROUPS      4           /*!< Subgroups carried per source (more are dropped). */
#define BLE_AUDIO_BA_NAME_MAX           32          /*!< Longest reported source name, without the NUL. */
#define BLE_AUDIO_BA_CODE_SIZE          16          /*!< Broadcast Code length. */
#define BLE_AUDIO_BA_BIS_NO_PREF        0xFFFFFFFFu /*!< bis_sync value: let the sink pick the BISes. */
#define BLE_AUDIO_BA_PA_INTERVAL_UNKNOWN 0xFFFF
#define BLE_AUDIO_BA_SRC_ID_NONE        0xFF        /*!< ble_audio_ba_result_t::src_id when not applicable. */

enum {
  BLE_AUDIO_EVT_BA_DISCOVERED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_BROADCAST_ASSISTANT, 0),     /* ble_audio_ba_discovered_t */
  BLE_AUDIO_EVT_BA_SOURCE_FOUND = BLE_AUDIO_EVT(BLE_AUDIO_GRP_BROADCAST_ASSISTANT, 1),   /* ble_audio_ba_source_t */
  BLE_AUDIO_EVT_BA_RECV_STATE = BLE_AUDIO_EVT(BLE_AUDIO_GRP_BROADCAST_ASSISTANT, 2),     /* ble_audio_ba_recv_state_t */
  BLE_AUDIO_EVT_BA_RECV_STATE_REMOVED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_BROADCAST_ASSISTANT, 3), /* ble_audio_ba_result_t (src_id) */
  BLE_AUDIO_EVT_BA_RESULT = BLE_AUDIO_EVT(BLE_AUDIO_GRP_BROADCAST_ASSISTANT, 4),         /* ble_audio_ba_result_t; err */
};

/** Operation completed by BLE_AUDIO_EVT_BA_RESULT. */
typedef enum {
  BLE_AUDIO_BA_OP_SCAN_START = 0,
  BLE_AUDIO_BA_OP_SCAN_STOP,
  BLE_AUDIO_BA_OP_ADD_SOURCE,
  BLE_AUDIO_BA_OP_MODIFY_SOURCE,
  BLE_AUDIO_BA_OP_REMOVE_SOURCE,
  BLE_AUDIO_BA_OP_BROADCAST_CODE,
  BLE_AUDIO_BA_OP_PAST, /*!< err -ENOENT: no local PA sync; -ENOTSUP: host built without PAST. */
} ble_audio_ba_op_t;

/** Payload of BLE_AUDIO_EVT_BA_DISCOVERED. */
typedef struct {
  uint8_t recv_states; /*!< Receive State characteristics on the remote BASS. */
} ble_audio_ba_discovered_t;

/** A broadcast source seen by the local scanner (each Broadcast_ID once per scan). */
typedef struct {
  uint8_t addr_type;
  uint8_t addr[6];
  uint8_t sid;          /*!< Advertising SID. */
  int8_t rssi;
  uint32_t broadcast_id;
  uint16_t pa_interval; /*!< 1.25 ms units, or BLE_AUDIO_BA_PA_INTERVAL_UNKNOWN. */
  char name[BLE_AUDIO_BA_NAME_MAX + 1]; /*!< Broadcast Name, else Complete Local Name, else empty. */
} ble_audio_ba_source_t;

/** One remote Receive State. */
typedef struct {
  uint8_t src_id;        /*!< Source_ID assigned by the remote. */
  uint8_t addr_type;
  uint8_t addr[6];
  uint8_t sid;
  uint32_t broadcast_id;
  uint8_t pa_sync_state; /*!< BASS PA_Sync_State (0 not synced .. 4 no PAST). */
  uint8_t encrypt_state; /*!< BASS BIG_Encryption (0 none, 1 code required, 2 decrypting, 3 bad code). */
  uint8_t bad_code[BLE_AUDIO_BA_CODE_SIZE]; /*!< Valid when encrypt_state is 3. */
  uint8_t num_subgroups; /*!< Capped at BLE_AUDIO_BA_MAX_SUBGROUPS. */
  uint32_t bis_sync[BLE_AUDIO_BA_MAX_SUBGROUPS]; /*!< BISes the sink is synced to, per subgroup. */
} ble_audio_ba_recv_state_t;

/** Payload of BLE_AUDIO_EVT_BA_RESULT and BLE_AUDIO_EVT_BA_RECV_STATE_REMOVED. */
typedef struct {
  uint8_t op;     /*!< ble_audio_ba_op_t */
  uint8_t src_id; /*!< PAST / removed source, else BLE_AUDIO_BA_SRC_ID_NONE. */
} ble_audio_ba_result_t;

/** Add Source operation parameters. */
typedef struct {
  uint8_t addr_type; /*!< Identity types are folded to public / random. */
  uint8_t addr[6];
  uint8_t sid;
  uint32_t broadcast_id;
  bool pa_sync;          /*!< Ask the sink to sync to the periodic advertising. */
  uint16_t pa_interval;  /*!< Helps the sink schedule the sync; BLE_AUDIO_BA_PA_INTERVAL_UNKNOWN if not known. */
  uint8_t num_subgroups; /*!< 1..BLE_AUDIO_BA_MAX_SUBGROUPS */
  uint32_t bis_sync[BLE_AUDIO_BA_MAX_SUBGROUPS]; /*!< BIT(index-1) per BIS, or BLE_AUDIO_BA_BIS_NO_PREF. */
  const uint8_t *meta; /*!< Metadata applied to every subgroup, or NULL. */
  uint8_t meta_len;
} ble_audio_ba_add_src_t;

/** Modify Source operation parameters (no subgroup metadata is sent). */
typedef struct {
  uint8_t src_id;
  bool pa_sync;
  uint16_t pa_interval;
  uint8_t num_subgroups; /*!< 0..BLE_AUDIO_BA_MAX_SUBGROUPS. */
  uint32_t bis_sync[BLE_AUDIO_BA_MAX_SUBGROUPS];
} ble_audio_ba_mod_src_t;

/** @brief Register the assistant callbacks (idempotent; before or after start). */
int bleAudioBaInit(void);

/** @brief Discover the remote BASS (BLE_AUDIO_EVT_BA_DISCOVERED, then one RECV_STATE per non-empty state). */
int bleAudioBaDiscover(uint16_t conn_handle);

/**
 * @brief Tell the remote that we scan on its behalf and start reporting sources.
 * @param stack_scan true lets the stack start the scan; false expects the
 *        application scanner to be running (its reports reach the engine GAP).
 */
int bleAudioBaScanStart(uint16_t conn_handle, bool stack_scan);
/** @brief Tell the remote we stopped scanning; SOURCE_FOUND reports stop immediately. */
int bleAudioBaScanStop(uint16_t conn_handle);

/*
 * The operations below write the remote Broadcast Audio Scan Control Point.
 * Each completes with BLE_AUDIO_EVT_BA_RESULT; the remote then reports the
 * resulting receive state (RECV_STATE or RECV_STATE_REMOVED).
 */

/** @brief Add a broadcast source to the remote. */
int bleAudioBaAddSource(uint16_t conn_handle, const ble_audio_ba_add_src_t *src);
/** @brief Change PA / BIS sync of a source the remote already holds. */
int bleAudioBaModifySource(uint16_t conn_handle, const ble_audio_ba_mod_src_t *src);
/** @brief Remove a source (the remote must no longer be synced to it). */
int bleAudioBaRemoveSource(uint16_t conn_handle, uint8_t src_id);
/** @brief Send the Broadcast Code for an encrypted source. */
int bleAudioBaSetBroadcastCode(uint16_t conn_handle, uint8_t src_id, const uint8_t code[BLE_AUDIO_BA_CODE_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
