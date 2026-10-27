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
 * @brief BAP engine unit: stream pool, PACS, unicast server/client, broadcast
 *        source/sink and scan delegator.
 *
 * Streams live in a fixed pool sized from Kconfig. Each slot embeds an
 * `esp_ble_audio_cap_stream_t` (whose first member is the BAP stream), so the
 * same slot serves plain BAP procedures and CAP procedures in
 * `BLEAudioEngineCap.c`. Stream events and SDUs go straight to C++ through
 * one callback table keyed by the slot's `owner` (the C++ stream Impl); the
 * control-plane events of each role go through the envelope bridge
 * (bleAudioEngineEmit).
 *
 * Every function runs on the host task or with the host lock held by the
 * caller, like the rest of the engine. Integer results are 0 or an
 * `esp_err_t` / negative errno from the stack.
 */

#include "audio/BLEAudioEngine.h"
#if BLE_AUDIO_SUPPORTED

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Install this unit's engine hooks; call after bleAudioEngineInit(). */
int bleAudioBapAttach(void);

/* ── Stream pool ────────────────────────────────────────────────────────── */

/** Role a stream slot belongs to; each kind has its own Kconfig-sized quota. */
typedef enum {
  BLE_AUDIO_STREAM_UNICAST_SERVER = 0,  /*!< Bound to an ASE a remote client configures. */
  BLE_AUDIO_STREAM_UNICAST_CLIENT,      /*!< Configured by us onto a remote ASE (BAP or CAP). */
  BLE_AUDIO_STREAM_BROADCAST_SOURCE,    /*!< One BIS we transmit. */
  BLE_AUDIO_STREAM_BROADCAST_SINK,      /*!< One BIS we receive. */
} ble_audio_stream_kind_t;

/** Endpoint (ASE) direction, same values as `ESP_BLE_AUDIO_DIR_*`. */
#define BLE_AUDIO_DIR_SINK   1
#define BLE_AUDIO_DIR_SOURCE 2

/** Opaque stream slot; defined in BLEAudioEngineBap.c. */
typedef struct ble_audio_slot ble_audio_slot_t;

/** SDU status, same meaning as `BLEAudioSduInfo::Status`. */
#define BLE_AUDIO_SDU_VALID   0
#define BLE_AUDIO_SDU_INVALID 1  /*!< Received with errors; data may be partial. */
#define BLE_AUDIO_SDU_LOST    2  /*!< Nothing received for this interval; data is empty. */

/** Per-SDU receive metadata. */
typedef struct {
  uint32_t ts;     /*!< Controller timestamp (us), valid when @ref ts_valid. */
  uint16_t seq;    /*!< Packet sequence number. */
  uint8_t status;  /*!< BLE_AUDIO_SDU_*. */
  bool ts_valid;
} ble_audio_recv_info_t;

/**
 * Stream callbacks; `owner` is the value passed to bleAudioStreamAlloc().
 * @ref recv and @ref sent run once per SDU interval and must stay short.
 */
typedef struct {
  void (*configured)(void *owner);               /*!< Codec configured (or reconfigured). */
  void (*started)(void *owner);                  /*!< Audio flows. */
  void (*stopped)(void *owner, uint8_t reason);  /*!< Audio stopped; @p reason is the HCI reason. */
  void (*released)(void *owner);                 /*!< Endpoint back to idle; the slot is unbound. */
  void (*recv)(void *owner, const ble_audio_recv_info_t *info, const uint8_t *data, uint16_t len);
  void (*sent)(void *owner);                     /*!< The controller consumed one Tx SDU. */
} ble_audio_stream_cbs_t;

/** @brief Install the stream callback table (once, before any stream exists). */
void bleAudioStreamSetCallbacks(const ble_audio_stream_cbs_t *cbs);

/**
 * @brief Take a free slot of @p kind for @p owner.
 * @return NULL when every slot of that kind is taken (quota sized by Kconfig, see BLEAudioEngineBap.c).
 */
ble_audio_slot_t *bleAudioStreamAlloc(ble_audio_stream_kind_t kind, void *owner);
/** @brief Return @p slot to the pool (NULL is ignored). The stream must no longer be in use by the stack. */
void bleAudioStreamFree(ble_audio_slot_t *slot);

/**
 * @brief Send one SDU. The sequence number is kept per slot and advances only
 *        when the controller accepted the SDU.
 * @return 0, ESP_ERR_INVALID_STATE when not streaming or not Tx, or the stack error.
 */
int bleAudioStreamSend(ble_audio_slot_t *slot, const uint8_t *sdu, uint16_t len);

/** @brief True between the started and stopped callbacks. */
bool bleAudioStreamIsStreaming(const ble_audio_slot_t *slot);
/** @brief True when the local device transmits on this stream. */
bool bleAudioStreamIsTx(const ble_audio_slot_t *slot);
/** @brief ACL of a unicast stream; BLE_AUDIO_CONN_NONE for broadcast or unbound streams. */
uint16_t bleAudioStreamConnHandle(const ble_audio_slot_t *slot);
/** @brief Codec the stream is configured with; false before configuration. */
bool bleAudioStreamGetCodec(const ble_audio_slot_t *slot, ble_audio_codec_t *out);
/** @brief QoS the stream runs with; false before QoS configuration. */
bool bleAudioStreamGetQos(const ble_audio_slot_t *slot, ble_audio_qos_t *out);

/* For the other engine units (CAP procedures). */

/** @brief The slot's `esp_ble_audio_cap_stream_t*`, NULL for NULL. */
void *bleAudioStreamCap(ble_audio_slot_t *slot);
/** @brief Slot owning a BAP stream pointer (container_of); NULL if it is not from the pool. */
ble_audio_slot_t *bleAudioStreamFromBap(const void *bap);
/**
 * @brief Build the slot-owned codec config.
 * @param context Streaming context metadata, 0 = Unspecified.
 * @return The `esp_ble_audio_codec_cfg_t*` (valid for the slot's lifetime), NULL if the codec has no LC3 encoding.
 */
void *bleAudioStreamSetCodec(ble_audio_slot_t *slot, const ble_audio_codec_t *codec, uint16_t context);
/** @brief Build the slot-owned QoS config; returns `esp_ble_audio_bap_qos_cfg_t*`. */
void *bleAudioStreamSetQos(ble_audio_slot_t *slot, const ble_audio_qos_t *qos);
/** @brief Mark a CAP-driven unicast stream as bound to @p conn_handle / direction. */
void bleAudioStreamBind(ble_audio_slot_t *slot, uint16_t conn_handle, bool tx);

/* ── PACS ───────────────────────────────────────────────────────────────── */

/** One LC3 capability record (bitfields as in the PAC characteristic). */
typedef struct {
  uint16_t freq_mask;   /*!< ESP_BLE_AUDIO_CODEC_CAP_FREQ_* bits. */
  uint8_t dur_mask;     /*!< ESP_BLE_AUDIO_CODEC_CAP_DURATION_* bits. */
  uint8_t chan_counts;  /*!< BIT(n-1) per supported channel count. */
  uint16_t min_octets;  /*!< Smallest supported octets per codec frame. */
  uint16_t max_octets;  /*!< Largest supported octets per codec frame. */
  uint8_t max_frames;   /*!< Max codec frames per SDU. */
} ble_audio_pac_t;

/**
 * @brief Union of the LC3 presets set in @p preset_mask (bit = BLEAudioCodecPreset).
 * @param max_channels Channel counts 1..@p max_channels are advertised (0 is treated as 1).
 */
void bleAudioPacFromPresets(uint32_t preset_mask, uint8_t max_channels, ble_audio_pac_t *out);

/**
 * @brief Presets (bit = BLEAudioCodecPreset) whose rate, duration and octets fit @p pac.
 * @return The preset mask; 0 for NULL or a record no preset fits.
 */
uint32_t bleAudioPacPresets(const ble_audio_pac_t *pac);

/**
 * @brief Merge a capability, location and contexts into the record for @p dir.
 *
 * Roles call this while staging; bleAudioPacsCommit() registers the merged
 * records once, so a unicast server and a broadcast sink share one sink PAC.
 */
void bleAudioPacsAdd(uint8_t dir, const ble_audio_pac_t *pac, uint32_t location, uint16_t contexts);

/**
 * @brief `pacs_register` + `pacs_cap_register` + location/contexts, once per session.
 * @return 0 (also when nothing was staged or it already ran) or the stack error.
 */
int bleAudioPacsCommit(void);

/** @brief Change the available contexts after start (e.g. while busy); ESP_ERR_INVALID_STATE before commit. */
int bleAudioPacsSetAvailable(uint8_t dir, uint16_t contexts);

/* ── Unicast server ─────────────────────────────────────────────────────── */

/** QoS preferences the server returns with every Config Codec (ASCS "Codec Configured" state). */
typedef struct {
  uint8_t rtn;             /*!< Preferred retransmission number. */
  uint16_t latency_ms;     /*!< Max transport latency. */
  uint32_t pd_min_us;      /*!< Supported presentation delay range. */
  uint32_t pd_max_us;
  uint32_t pref_pd_min_us; /*!< Preferred presentation delay range (0 = no preference). */
  uint32_t pref_pd_max_us;
  uint8_t phy;             /*!< BLE_AUDIO_PHY_* bitmask, 0 = 2M. */
  bool unframed;           /*!< Unframed PDUs supported. */
} ble_audio_qos_pref_t;

/**
 * @brief Register ASCS with @p snk_cnt / @p src_cnt ASEs (clamped to Kconfig).
 *
 * Remote clients configure ASEs onto the server slots the C++ side allocated
 * (kind UNICAST_SERVER); a config request with no free slot is rejected.
 * @param pref QoS preferences, NULL for all-zero preferences.
 */
int bleAudioUsInit(uint8_t snk_cnt, uint8_t src_cnt, const ble_audio_qos_pref_t *pref);

/* ── Unicast client ─────────────────────────────────────────────────────── */

enum {
  BLE_AUDIO_EVT_UC_DISCOVERED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_UNICAST_CLIENT, 0), /* ble_audio_uc_discovered_t; err != 0 = no ASE found */
  BLE_AUDIO_EVT_UC_PAC = BLE_AUDIO_EVT(BLE_AUDIO_GRP_UNICAST_CLIENT, 1),        /* ble_audio_uc_pac_t, one per remote LC3 PAC record */
  BLE_AUDIO_EVT_UC_STARTED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_UNICAST_CLIENT, 2),    /* every requested stream is streaming */
  BLE_AUDIO_EVT_UC_STOPPED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_UNICAST_CLIENT, 3),    /* every stream released, group deleted */
  BLE_AUDIO_EVT_UC_ERROR = BLE_AUDIO_EVT(BLE_AUDIO_GRP_UNICAST_CLIENT, 4),      /* ble_audio_uc_error_t; a stop follows */
};

/** Discovery summary for one acceptor. */
typedef struct {
  uint8_t sink_eps;    /*!< Remote sink ASEs (local transmits). */
  uint8_t source_eps;  /*!< Remote source ASEs (local receives). */
  uint32_t sink_loc;   /*!< Sink Audio Locations. */
  uint32_t source_loc; /*!< Source Audio Locations. */
  uint16_t sink_ctx;   /*!< Available contexts. */
  uint16_t source_ctx;
} ble_audio_uc_discovered_t;

/** One remote LC3 capability record. */
typedef struct {
  uint8_t dir;         /*!< BLE_AUDIO_DIR_*. */
  ble_audio_pac_t pac;
} ble_audio_uc_pac_t;

/** ASCS operation that failed, for BLE_AUDIO_EVT_UC_ERROR. */
enum { BLE_AUDIO_UC_OP_CONFIG = 1, BLE_AUDIO_UC_OP_QOS, BLE_AUDIO_UC_OP_ENABLE, BLE_AUDIO_UC_OP_START, BLE_AUDIO_UC_OP_CONNECT, BLE_AUDIO_UC_OP_GROUP };

/** Payload of BLE_AUDIO_EVT_UC_ERROR. */
typedef struct {
  uint8_t op;       /*!< BLE_AUDIO_UC_OP_*. */
  uint8_t rsp_code; /*!< ASCS response code from the peer; 0 when the call failed locally. */
  uint8_t reason;   /*!< ASCS reason from the peer; 0 when the call failed locally. */
} ble_audio_uc_error_t;

/** One stream to set up with bleAudioUcStart(). */
typedef struct {
  ble_audio_slot_t *slot;  /*!< A UNICAST_CLIENT slot. */
  uint16_t conn_handle;    /*!< A peer already discovered with bleAudioUcDiscover(). */
  uint8_t dir;             /*!< Remote endpoint direction (BLE_AUDIO_DIR_*). */
  uint8_t ep_index;        /*!< Index among the discovered endpoints of @p dir. */
  ble_audio_codec_t codec;
  ble_audio_qos_t qos;
  uint16_t context;        /*!< Streaming context sent with Enable. */
} ble_audio_uc_stream_req_t;

/** @brief Register the unicast client callbacks (once per session). */
int bleAudioUcInit(void);
/** @brief Discover the peer's PACS and ASEs (sink, then source); ends with BLE_AUDIO_EVT_UC_DISCOVERED. */
int bleAudioUcDiscover(uint16_t conn_handle);
/**
 * @brief Configure, QoS-configure, enable and connect every requested stream.
 *
 * Runs the ASCS sequence one control-point operation at a time; each stream
 * reports through the stream callbacks and BLE_AUDIO_EVT_UC_STARTED fires once
 * all of them stream. Streams that share a peer and have opposite directions
 * share one bidirectional CIS. All requests are validated before any is
 * applied; ESP_ERR_INVALID_STATE while a previous setup is still active.
 */
int bleAudioUcStart(const ble_audio_uc_stream_req_t *reqs, uint8_t count);
/** @brief Release every client stream and delete the CIG (BLE_AUDIO_EVT_UC_STOPPED). */
int bleAudioUcStop(void);

/* ── Broadcast source ───────────────────────────────────────────────────── */

enum {
  BLE_AUDIO_EVT_BSRC_STARTED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_BROADCAST_SOURCE, 0),
  BLE_AUDIO_EVT_BSRC_STOPPED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_BROADCAST_SOURCE, 1), /* err = HCI reason */
};

/**
 * @brief Create a one-subgroup source with @p count BISes (one slot each).
 *
 * Only one source exists at a time. The codec and QoS are stored in
 * `slots[0]` and shared by every BIS.
 * @param locations Per-BIS Audio Location (NULL or 0 = no per-BIS allocation).
 * @param code      16-octet Broadcast Code, NULL for an unencrypted BIG.
 */
int bleAudioBsrcCreate(ble_audio_slot_t *const slots[], const uint32_t *locations, uint8_t count, const ble_audio_codec_t *codec,
                       const ble_audio_qos_t *qos, uint16_t context, const uint8_t *code);
/** @brief Encoded BASE (starting with the 0x1851 UUID) for the periodic advertising data. */
int bleAudioBsrcGetBase(uint8_t *out, uint16_t cap, uint16_t *out_len);
/** @brief Attach the BIG to an extended advertising set that already runs periodic advertising. */
int bleAudioBsrcStart(uint8_t adv_handle);
/** @brief Terminate the BIG (BLE_AUDIO_EVT_BSRC_STOPPED); the source can be started again. */
int bleAudioBsrcStop(void);
/** @brief Replace the Streaming Audio Context in the BASE metadata. */
int bleAudioBsrcUpdateContext(uint16_t context);
/** @brief Delete the source and detach the advertising set; ignored (with a warning) while the BIG is up. */
void bleAudioBsrcDelete(void);
/** @brief Opaque `esp_ble_audio_bap_broadcast_source_t*` for PBP/handover. */
void *bleAudioBsrcHandle(void);

/* ── Broadcast sink + scan delegator ────────────────────────────────────── */

enum {
  BLE_AUDIO_EVT_BSINK_FOUND = BLE_AUDIO_EVT(BLE_AUDIO_GRP_BROADCAST_SINK, 0),       /* ble_audio_bsink_found_t */
  BLE_AUDIO_EVT_BSINK_PA_SYNCED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_BROADCAST_SINK, 1),   /* err = HCI status */
  BLE_AUDIO_EVT_BSINK_PA_LOST = BLE_AUDIO_EVT(BLE_AUDIO_GRP_BROADCAST_SINK, 2),     /* err = HCI reason */
  BLE_AUDIO_EVT_BSINK_BASE = BLE_AUDIO_EVT(BLE_AUDIO_GRP_BROADCAST_SINK, 3),        /* ble_audio_bsink_base_t */
  BLE_AUDIO_EVT_BSINK_STARTED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_BROADCAST_SINK, 4),
  BLE_AUDIO_EVT_BSINK_STOPPED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_BROADCAST_SINK, 5),     /* err = HCI reason */
  BLE_AUDIO_EVT_BSINK_SYNC_FAILED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_BROADCAST_SINK, 6), /* err: -ENOENT no BIS, -EACCES no code, else stack error */
  BLE_AUDIO_EVT_BSINK_PA_REQ = BLE_AUDIO_EVT(BLE_AUDIO_GRP_BROADCAST_SINK, 7),      /* ble_audio_bsink_pa_req_t */
  BLE_AUDIO_EVT_BSINK_PA_TERM_REQ = BLE_AUDIO_EVT(BLE_AUDIO_GRP_BROADCAST_SINK, 8), /* assistant asks to drop the PA sync */
  BLE_AUDIO_EVT_BSINK_CODE = BLE_AUDIO_EVT(BLE_AUDIO_GRP_BROADCAST_SINK, 9),        /* code received from an assistant */
};

#define BLE_AUDIO_BCODE_SIZE     16  /*!< Broadcast Code length. */
#define BLE_AUDIO_BCAST_NAME_MAX 32  /*!< Longest Broadcast Name kept (longer names are truncated). */

/** A Broadcast Source seen while scanning. */
typedef struct {
  uint8_t addr_type;
  uint8_t addr[6];         /*!< LSB-first on both hosts (see bleAudioAddrFromGap()). */
  uint8_t sid;             /*!< Advertising SID, needed for the PA sync. */
  int8_t rssi;
  uint32_t broadcast_id;   /*!< 24-bit Broadcast_ID. */
  uint16_t pba_features;   /*!< Public Broadcast Announcement features, 0 if absent. */
  bool has_pba;
  char name[BLE_AUDIO_BCAST_NAME_MAX + 1];  /*!< Broadcast Name, else Complete Local Name, else empty. */
} ble_audio_bsink_found_t;

/** Summary of a received BASE. */
typedef struct {
  uint32_t bis_mask;       /*!< BIT(index-1) for every BIS in the BASE. */
  uint8_t subgroups;
  uint32_t pd_us;          /*!< Presentation delay. */
  ble_audio_codec_t codec; /*!< First subgroup. */
} ble_audio_bsink_base_t;

/** A Broadcast Assistant asked us to sync; the C++ side creates the PA sync. */
typedef struct {
  uint8_t addr_type;
  uint8_t addr[6];       /*!< LSB-first (taken from the BASS receive state). */
  uint8_t sid;
  uint32_t broadcast_id;
  bool past;             /*!< Wait for PAST on the ACL instead of scanning. */
  uint16_t pa_interval;  /*!< PA interval from the assistant (1.25 ms units, 0xFFFF = unknown). */
  int *reply;            /*!< Handler writes 0 to accept, <0 to reject. */
} ble_audio_bsink_pa_req_t;

/**
 * @brief Enable the sink (and the scan delegator when @p delegator).
 *
 * The caller must also add a sink PAC with bleAudioPacsAdd() before commit;
 * the stack only reports BIS indexes of subgroups whose codec matches a PAC.
 */
int bleAudioBsinkInit(bool delegator);
/** @brief Streams to sync (1 = mono, 2 = stereo); the chosen BISes bind to them in order. */
int bleAudioBsinkSetStreams(ble_audio_slot_t *const slots[], uint8_t count);
/** @brief Report announcements seen while scanning (BLE_AUDIO_EVT_BSINK_FOUND). */
void bleAudioBsinkSetScanning(bool on);
/** @brief Expect a PA sync to @p broadcast_id; the next PA_SYNC event creates the sink. */
void bleAudioBsinkArm(uint32_t broadcast_id);
/** @param code 16 octets, or NULL to clear. */
void bleAudioBsinkSetCode(const uint8_t *code);
/** @brief Preferred BIS indexes (BIT(index-1)); 0 = the lowest ones in the BASE. */
void bleAudioBsinkSetBisMask(uint32_t mask);
/** @brief Leave the BIG (the sink is deleted once the stack reports it stopped). */
int bleAudioBsinkStop(void);
/** @brief PA sync handle in use, 0xFFFF when not synced. */
uint16_t bleAudioBsinkSyncHandle(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
