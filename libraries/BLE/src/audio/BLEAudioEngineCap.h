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
 * @brief CAP engine unit: CAP acceptor/initiator/commander/handover and CSIP.
 *
 * Client procedures (initiator, commander, set coordinator) act on the peers
 * they discovered; the unit caches each peer's connection object from the
 * discovery callbacks, so callers only pass connection handles. Every
 * procedure reports completion through the event bridge with its error code.
 * Functions of a role that is disabled in Kconfig return ESP_ERR_NOT_SUPPORTED.
 */

#include "audio/BLEAudioEngineBap.h"
#if BLE_AUDIO_SUPPORTED

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_AUDIO_SIRK_LEN 16 /*!< Set Identity Resolving Key length. */
#define BLE_AUDIO_RSI_LEN  6  /*!< Resolvable Set Identifier length (advertised in AD type 0x2E). */

/* ── CSIS server (CAP acceptor's included instance, or standalone member) ── */

/**
 * Event numbers inside BLE_AUDIO_GRP_CAP_ACCEPTOR (CAS-included instance) and
 * BLE_AUDIO_GRP_CSIP_MEMBER (standalone instance); build the type with
 * BLE_AUDIO_EVT(group, n).
 */
enum {
  BLE_AUDIO_CSIS_EVT_LOCK = 0,     /* ble_audio_csis_lock_t; conn = client or NONE (local/timeout) */
  BLE_AUDIO_CSIS_EVT_SIRK_REQ = 1, /* ble_audio_csis_sirk_req_t */
};

/** SIRK read response, same order as ESP_BLE_AUDIO_CSIP_READ_SIRK_REQ_RSP_*. */
enum { BLE_AUDIO_SIRK_ACCEPT = 0, BLE_AUDIO_SIRK_ACCEPT_ENC, BLE_AUDIO_SIRK_REJECT, BLE_AUDIO_SIRK_OOB_ONLY };

/** Set lock state (local instance or a remote member). */
typedef struct {
  bool locked;
} ble_audio_csis_lock_t;

/**
 * A client is reading the SIRK. Dispatched synchronously: the handler picks
 * the response by writing one of the BLE_AUDIO_SIRK_* values to @c *reply.
 */
typedef struct {
  uint8_t *reply; /*!< Preset to BLE_AUDIO_SIRK_ACCEPT; the handler may overwrite it. */
} ble_audio_csis_sirk_req_t;

/** Registration parameters of a CSIS instance. */
typedef struct {
  uint8_t sirk[BLE_AUDIO_SIRK_LEN]; /*!< Shared by every member of the set. */
  uint8_t set_size;                 /*!< Number of members in the set. */
  uint8_t rank;                     /*!< This member's rank, 1..set_size, unique in the set. */
  bool lockable;                    /*!< Expose the Set Member Lock characteristic. */
  const uint8_t *name; /*!< Coordinated Set Name (CSIS 1.1), may be NULL. */
  uint8_t name_len;
} ble_audio_csis_cfg_t;

/*
 * Every bleAudioCsis* function takes @p cas to pick the instance: true for the
 * CSIS included by the CAP acceptor's CAS, false for the standalone one.
 */

/**
 * @brief Register the CSIS instance and record it for `common_start`.
 * @param cas true: CAS + included CSIS (CAP acceptor); false: standalone CSIS.
 */
int bleAudioCsisRegister(bool cas, const ble_audio_csis_cfg_t *cfg);
/** @brief Replace the SIRK of a registered instance. */
int bleAudioCsisSetSirk(bool cas, const uint8_t sirk[BLE_AUDIO_SIRK_LEN]);
/** @brief Update set size and rank of a registered instance. */
int bleAudioCsisSetSizeRank(bool cas, uint8_t size, uint8_t rank);
/** @brief Update the Coordinated Set Name (needs CSIS 1.1 support in the stack). */
int bleAudioCsisSetName(bool cas, const uint8_t *name, uint8_t len);
/** @brief Generate a fresh RSI from the instance's SIRK, for the advertising data. */
int bleAudioCsisGenerateRsi(bool cas, uint8_t rsi[BLE_AUDIO_RSI_LEN]);
/**
 * @brief Lock or release the set locally.
 * @param force Release even when a client holds the lock.
 */
int bleAudioCsisLock(bool cas, bool lock, bool force);
/** @return true while the instance is locked (by anyone); false if not registered. */
bool bleAudioCsisIsLocked(bool cas);

/* ── CSIP set coordinator ───────────────────────────────────────────────── */

enum {
  BLE_AUDIO_EVT_CSIP_DISCOVERED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CSIP_COORDINATOR, 0),   /* ble_audio_csip_set_t */
  BLE_AUDIO_EVT_CSIP_LOCKED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CSIP_COORDINATOR, 1),       /* lock procedure done */
  BLE_AUDIO_EVT_CSIP_RELEASED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CSIP_COORDINATOR, 2),     /* release procedure done */
  BLE_AUDIO_EVT_CSIP_LOCK_CHANGED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CSIP_COORDINATOR, 3), /* ble_audio_csis_lock_t */
};

/** First set of the peer (set_count == 0: no CSIS). */
typedef struct {
  uint8_t set_count;                /*!< CSIS instances on the peer. */
  uint8_t set_size;                 /*!< Members in the first set. */
  uint8_t rank;                     /*!< The peer's rank in the first set. */
  bool lockable;
  uint8_t sirk[BLE_AUDIO_SIRK_LEN]; /*!< SIRK of the first set, as held by the stack. */
} ble_audio_csip_set_t;

/** @brief Register the set coordinator callbacks. */
int bleAudioCsipCoordInit(void);
/** @brief Discover CSIS on @p conn_handle and remember it as a member (CSIP_DISCOVERED). */
int bleAudioCsipCoordDiscover(uint16_t conn_handle);
/** @brief Lock (or release) every discovered member whose set has @p sirk (NULL = any). */
int bleAudioCsipCoordLock(const uint8_t sirk[BLE_AUDIO_SIRK_LEN], bool lock);
/** @brief Whether an advertising payload carries an RSI that resolves with @p sirk. */
bool bleAudioCsipIsSetMember(const uint8_t sirk[BLE_AUDIO_SIRK_LEN], const uint8_t *adv, uint16_t len);

/* ── CAP initiator ──────────────────────────────────────────────────────── */

enum {
  BLE_AUDIO_EVT_CAP_DISCOVERED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CAP_INITIATOR, 0), /* ble_audio_cap_discovered_t; err = CAS result */
  BLE_AUDIO_EVT_CAP_UC_STARTED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CAP_INITIATOR, 1),
  BLE_AUDIO_EVT_CAP_UC_UPDATED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CAP_INITIATOR, 2),
  BLE_AUDIO_EVT_CAP_UC_STOPPED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CAP_INITIATOR, 3),
  BLE_AUDIO_EVT_CAP_BC_STARTED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CAP_INITIATOR, 4),
  BLE_AUDIO_EVT_CAP_BC_STOPPED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CAP_INITIATOR, 5), /* err = HCI reason */
  /** An acceptor wants our periodic advertising over PAST (it is on @c conn_handle). */
  BLE_AUDIO_EVT_CAP_PAST_REQ = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CAP_INITIATOR, 6), /* ble_audio_cap_past_req_t */
};

/** Payload of BLE_AUDIO_EVT_CAP_DISCOVERED. */
typedef struct {
  ble_audio_uc_discovered_t eps; /*!< All zero when the peer has no ASCS. */
  bool coordinated;              /*!< CAS includes a CSIS instance. */
} ble_audio_cap_discovered_t;

/** Payload of BLE_AUDIO_EVT_CAP_PAST_REQ. */
typedef struct {
  uint8_t src_id;    /*!< Goes in the PAST service data high octet. */
  uint8_t addr_type; /*!< Peer address (LSB first), for hosts that address PAST by peer. */
  uint8_t addr[6];
} ble_audio_cap_past_req_t;

/** Extended advertising set carrying the broadcast (see `esp_ble_iso_ext_adv_info_t`). */
typedef struct {
  uint8_t handle;       /*!< Host advertising instance/handle. */
  uint8_t sid;          /*!< Advertising SID. */
  uint8_t addr_type;
  uint8_t addr[6];      /*!< Local advertiser address, LSB first. */
  uint16_t pa_interval; /*!< Periodic advertising interval (1.25 ms units). */
  uint32_t broadcast_id; /*!< 24-bit Broadcast_ID advertised in the service data. */
} ble_audio_cap_adv_t;

/** @brief Register the CAP initiator callbacks (plus the unicast client callbacks its discovery uses). */
int bleAudioCapInitiatorInit(void);
/** @brief CAS, then (with handover) BASS, then ASCS sink and source discovery. */
int bleAudioCapDiscover(uint16_t conn_handle);
/**
 * @brief Create the unicast group and run the CAP unicast start procedure.
 *
 * @p reqs name endpoints discovered by bleAudioCapDiscover(); slots are
 * UNICAST_CLIENT streams. Opposite directions of one peer share a CIS.
 */
int bleAudioCapUnicastStart(const ble_audio_uc_stream_req_t *reqs, uint8_t count);
/** @brief New streaming context on every stream of the running group. */
int bleAudioCapUnicastUpdate(uint16_t context);
/** @brief Stop and release every stream, then delete the group (UC_STOPPED). */
int bleAudioCapUnicastStop(void);

/** @brief Same contract as bleAudioBsrcCreate(), on the CAP broadcast source. */
int bleAudioCapBroadcastCreate(ble_audio_slot_t *const slots[], const uint32_t *locations, uint8_t count, const ble_audio_codec_t *codec,
                               const ble_audio_qos_t *qos, uint16_t context, const uint8_t *code);
/** @brief Encoded BASE (starting with the 0x1851 UUID) for the periodic advertising data. */
int bleAudioCapBroadcastGetBase(uint8_t *out, uint16_t cap, uint16_t *out_len);
/** @brief Register @p adv with the stack (once) and attach the BIG to it. */
int bleAudioCapBroadcastStart(const ble_audio_cap_adv_t *adv);
/** @brief Change the streaming context in the BASE metadata of the running broadcast. */
int bleAudioCapBroadcastUpdate(uint16_t context);
/** @brief Terminate the BIG (BC_STOPPED); the source stays configured for Start or Delete. */
int bleAudioCapBroadcastStop(void);
/** @brief Delete the stopped source and unregister the advertising set. */
void bleAudioCapBroadcastDelete(void);

/* ── CAP handover ───────────────────────────────────────────────────────── */

enum {
  /** The broadcast source exists: put its BASE in the periodic advertising and start it. */
  BLE_AUDIO_EVT_CAP_HO_CREATED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CAP_HANDOVER, 0),
  BLE_AUDIO_EVT_CAP_HO_TO_BROADCAST = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CAP_HANDOVER, 1), /* handover done (or failed: err) */
  BLE_AUDIO_EVT_CAP_HO_TO_UNICAST = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CAP_HANDOVER, 2),   /* handover done (or failed: err) */
};

/** @brief Register the handover callbacks and the BASS client used to move acceptors between modes. */
int bleAudioCapHandoverInit(void);
/**
 * @brief Move the streaming sink streams of the unicast group to a broadcast.
 *
 * The extended advertising set must already run (without periodic data); the
 * BASE is published on BLE_AUDIO_EVT_CAP_HO_CREATED. The broadcast reuses the
 * unicast codec; @p qos (may be NULL) overrides RTN and transport latency.
 */
int bleAudioCapHandoverToBroadcast(const ble_audio_cap_adv_t *adv, const ble_audio_qos_t *qos, const uint8_t *code);
/** @brief Move the handed-over broadcast streams back to unicast (@p reqs name those same slots). */
int bleAudioCapHandoverToUnicast(const ble_audio_uc_stream_req_t *reqs, uint8_t count);

/* ── CAP commander ──────────────────────────────────────────────────────── */

enum {
  BLE_AUDIO_EVT_CAP_CMD_DISCOVERED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CAP_COMMANDER, 0), /* ble_audio_cap_cmd_discovered_t */
  BLE_AUDIO_EVT_CAP_CMD_DONE = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CAP_COMMANDER, 1),       /* ble_audio_cap_cmd_done_t */
  BLE_AUDIO_EVT_CAP_CMD_RECV_STATE = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CAP_COMMANDER, 2), /* ble_audio_cap_recv_state_t */
};

/** Commander procedure, reported in ble_audio_cap_cmd_done_t. */
typedef enum {
  BLE_AUDIO_CAP_OP_VOLUME = 0,
  BLE_AUDIO_CAP_OP_VOLUME_MUTE,
  BLE_AUDIO_CAP_OP_VOLUME_OFFSET,
  BLE_AUDIO_CAP_OP_MIC_MUTE,
  BLE_AUDIO_CAP_OP_MIC_GAIN,
  BLE_AUDIO_CAP_OP_RECEPTION_START,
  BLE_AUDIO_CAP_OP_RECEPTION_STOP,
  BLE_AUDIO_CAP_OP_BROADCAST_CODE,
} ble_audio_cap_op_t;

/** Payload of BLE_AUDIO_EVT_CAP_CMD_DISCOVERED. */
typedef struct {
  bool coordinated; /*!< CAS includes a CSIS instance. */
  bool bass;        /*!< BASS found (broadcast reception procedures available). */
} ble_audio_cap_cmd_discovered_t;

/** Payload of BLE_AUDIO_EVT_CAP_CMD_DONE; the event's err carries the procedure result. */
typedef struct {
  uint8_t op; /*!< ble_audio_cap_op_t */
} ble_audio_cap_cmd_done_t;

/** A peer's BASS Receive State, reported on read and on every notification. */
typedef struct {
  uint8_t src_id;    /*!< Source_ID the peer assigned. */
  uint8_t pa_state;  /*!< BASS PA sync state. */
  uint8_t big_enc;   /*!< BASS BIG encryption state. */
  uint32_t broadcast_id;
  uint32_t bis_sync; /*!< Union of the subgroups' BIS sync bits. */
} ble_audio_cap_recv_state_t;

/** Broadcast the commander asks acceptors to receive. */
typedef struct {
  uint8_t addr_type;
  uint8_t addr[6]; /*!< Broadcaster address, LSB first. */
  uint8_t sid;     /*!< Advertising SID of the broadcast. */
  uint16_t pa_interval; /*!< 0xFFFF = unknown. */
  uint32_t broadcast_id;
  uint32_t bis_sync;    /*!< BIT(index-1) per BIS; 0xFFFFFFFF = no preference. */
} ble_audio_cap_bcast_src_t;

/*
 * Commander procedures run on every peer passed to bleAudioCapCommanderDiscover()
 * that is still connected, and each completes with one CAP_CMD_DONE.
 */

/** @brief Register the commander callbacks. */
int bleAudioCapCommanderInit(void);
/**
 * @brief CAS, then the VCP and MICP controllers (compiled-in ones the link has
 *        not discovered yet), then BASS; the procedures below act on every
 *        discovered peer. CAP_CMD_DISCOVERED fires once the chain ends.
 */
int bleAudioCapCommanderDiscover(uint16_t conn_handle);
/** @brief Set the absolute volume (0-255) on every peer's VCS. */
int bleAudioCapCommanderVolume(uint8_t volume);
int bleAudioCapCommanderVolumeMute(bool mute);
/** @brief Set the offset of every peer's first VOCS instance. */
int bleAudioCapCommanderVolumeOffset(int16_t offset);
int bleAudioCapCommanderMicMute(bool mute);
/** @brief Set the gain of every peer's first microphone AICS instance. */
int bleAudioCapCommanderMicGain(int8_t gain);
/** @brief Ask every peer's BASS to sync to @p src (Add Source with PA sync). */
int bleAudioCapCommanderReceptionStart(const ble_audio_cap_bcast_src_t *src);
/** @brief Stop the reception started by the last ReceptionStart / handover. */
int bleAudioCapCommanderReceptionStop(void);
/** @brief Write the Broadcast Code to every peer that has a receive state for our source. */
int bleAudioCapCommanderDistributeCode(const uint8_t code[BLE_AUDIO_BCODE_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
