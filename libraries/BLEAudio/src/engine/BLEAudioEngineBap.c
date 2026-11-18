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
 * @file BLEAudioEngineBap.c
 * @brief BAP engine unit: stream pool, PACS, unicast server/client,
 *        broadcast source/sink and scan delegator.
 *
 * Sections, in order:
 *  - Codec/QoS translation between the plain C mirrors and the engine's LTV
 *    codec configuration (`esp_ble_audio_codec_cfg_t`).
 *  - Stream pool: fixed array of slots sized from Kconfig, one BAP stream
 *    ops table shared by all of them, and the direct C++ callbacks.
 *  - PACS: capability records staged by the roles and registered once.
 *  - Unicast server: ASCS registration and the config/QoS/enable requests
 *    a remote client sends to our ASEs.
 *  - Unicast client: discovery and a small state machine that runs the
 *    ASCS sequence one control-point operation at a time.
 *  - Broadcast source and sink, including the scan delegator (BASS server).
 *
 * All callbacks run on the Bluetooth host task; the functions called from
 * C++ run on the caller's task. The few slot fields both sides touch
 * (streaming/seq/bound/tx/in_use/conn_handle) are _Atomic so `write()` on the
 * codec task and the host-task streaming transitions cannot race; everything
 * else is configuration-before-start or host-task-only.
 *
 * API contract is documented in `BLEAudioEngineBap.h`.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include "engine/BLEAudioEngineBap.h"

#include "esp_ble_audio_bap_api.h"
#include "esp_ble_audio_cap_api.h"
#include "esp_ble_audio_pacs_api.h"
#include "esp_ble_audio_codec_api.h"
#include "esp_ble_audio_common_api.h"
#include "esp_ble_audio_defs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>
#include <errno.h>
#include <stdatomic.h>

static const char *TAG = "BLEAudioBap";

#define CONN_NONE    BLE_AUDIO_CONN_NONE
#define SYNC_NONE    0xFFFF
#define CFG_DATA_MAX BLE_AUDIO_CODEC_DATA_MAX /* Codec config LTVs, see the header. */
#define META_CTX_LEN 4                        /* One Streaming/Preferred Audio Contexts LTV. */

/*
 * With CAP compiled in, streams register their ops through CAP so the same
 * slot serves both plain BAP procedures and CAP procedures.
 */
#if defined(CONFIG_BT_CAP_ACCEPTOR) || defined(CONFIG_BT_CAP_INITIATOR)
#define USE_CAP_OPS 1
#else
#define USE_CAP_OPS 0
#endif

/* ── Little-endian helpers ──────────────────────────────────────────────── */

static inline uint16_t audio_get_le16(const uint8_t *p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}

static inline uint32_t audio_get_le24(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

/* ── Codec / QoS translation ────────────────────────────────────────────── */

/**
 * @brief Encode @p c as LC3 Codec Specific Configuration LTVs into @p d.
 *
 * Channel allocation is omitted for mono and the frame block count when it
 * is 1, as BAP allows (absent means the default).
 *
 * @param d Output, at least CFG_DATA_MAX bytes.
 * @return Encoded length, or 0 when the rate or duration has no LC3 code.
 */
static size_t build_cfg_data(uint8_t *d, const ble_audio_codec_t *c) {
  esp_ble_audio_codec_cfg_freq_t freq;
  esp_ble_audio_codec_cfg_frame_dur_t dur;
  if (esp_ble_audio_codec_cfg_freq_hz_to_freq(c->sample_rate_hz, &freq) != ESP_OK
      || esp_ble_audio_codec_cfg_frame_dur_us_to_frame_dur(c->frame_dur_us, &dur) != ESP_OK) {
    return 0;
  }
  size_t n = 0;
  d[n++] = 2;
  d[n++] = ESP_BLE_AUDIO_CODEC_CFG_FREQ;
  d[n++] = (uint8_t)freq;
  d[n++] = 2;
  d[n++] = ESP_BLE_AUDIO_CODEC_CFG_DURATION;
  d[n++] = (uint8_t)dur;
  if (c->chan_alloc) {
    d[n++] = 5;
    d[n++] = ESP_BLE_AUDIO_CODEC_CFG_CHAN_ALLOC;
    ble_audio_put_le32(&d[n], c->chan_alloc);
    n += 4;
  }
  d[n++] = 3;
  d[n++] = ESP_BLE_AUDIO_CODEC_CFG_FRAME_LEN;
  ble_audio_put_le16(&d[n], c->octets_per_frame);
  n += 2;
  if (c->frames_per_sdu > 1) {
    d[n++] = 2;
    d[n++] = ESP_BLE_AUDIO_CODEC_CFG_FRAME_BLKS_PER_SDU;
    d[n++] = c->frames_per_sdu;
  }
  return n;
}

/** @brief Encode one contexts metadata LTV (@p type = Streaming or Preferred Audio Contexts). */
static size_t build_ctx_meta(uint8_t *m, uint8_t type, uint16_t ctx) {
  m[0] = 3;
  m[1] = type;
  ble_audio_put_le16(&m[2], ctx);
  return META_CTX_LEN;
}

/**
 * @brief Decode an engine codec configuration into @p o.
 *
 * Rate, duration and octets are mandatory; a missing allocation reads as
 * mono and a missing block count as 1 (the BAP defaults).
 *
 * @return false when @p cfg is NULL or a mandatory LTV is missing.
 */
static bool parse_cfg(const esp_ble_audio_codec_cfg_t *cfg, ble_audio_codec_t *o) {
  esp_ble_audio_codec_cfg_freq_t freq;
  esp_ble_audio_codec_cfg_frame_dur_t dur;
  uint32_t hz = 0, us = 0;
  esp_ble_audio_location_t loc = 0;
  uint16_t octets = 0;
  uint8_t blocks = 1;

  if (cfg == NULL || esp_ble_audio_codec_cfg_get_freq(cfg, &freq) != ESP_OK || esp_ble_audio_codec_cfg_freq_to_freq_hz(freq, &hz) != ESP_OK
      || esp_ble_audio_codec_cfg_get_frame_dur(cfg, &dur) != ESP_OK || esp_ble_audio_codec_cfg_frame_dur_to_frame_dur_us(dur, &us) != ESP_OK
      || esp_ble_audio_codec_cfg_get_octets_per_frame(cfg, &octets) != ESP_OK) {
    return false;
  }
  (void)esp_ble_audio_codec_cfg_get_chan_allocation(cfg, &loc, true);
  (void)esp_ble_audio_codec_cfg_get_frame_blocks_per_sdu(cfg, &blocks, true);
  o->sample_rate_hz = hz;
  o->frame_dur_us = (uint16_t)us;
  o->octets_per_frame = octets;
  o->frames_per_sdu = blocks ? blocks : 1;
  o->chan_alloc = (uint32_t)loc;
  return true;
}

/** @brief C mirror -> engine QoS (a zero PHY defaults to 2M). */
static void qos_to_cfg(const ble_audio_qos_t *q, esp_ble_audio_bap_qos_cfg_t *o) {
  memset(o, 0, sizeof(*o));
  o->pd = q->pd_us;
  o->framing = q->framed ? ESP_BLE_AUDIO_BAP_QOS_CFG_FRAMING_FRAMED : ESP_BLE_AUDIO_BAP_QOS_CFG_FRAMING_UNFRAMED;
  o->phy = q->phy ? q->phy : BLE_AUDIO_PHY_2M;
  o->rtn = q->rtn;
  o->sdu = q->max_sdu;
  o->latency = q->latency_ms;
  o->interval = q->sdu_interval_us;
}

/** @brief Engine QoS -> C mirror. */
static void qos_from_cfg(const esp_ble_audio_bap_qos_cfg_t *q, ble_audio_qos_t *o) {
  o->sdu_interval_us = q->interval;
  o->max_sdu = q->sdu;
  o->rtn = q->rtn;
  o->latency_ms = q->latency;
  o->pd_us = q->pd;
  o->phy = q->phy;
  o->framed = (q->framing == ESP_BLE_AUDIO_BAP_QOS_CFG_FRAMING_FRAMED);
}

/* ── Stream pool ────────────────────────────────────────────────────────── */

/*
 * One slot per stream the packaged Kconfig allows, per kind. Each kind has
 * its own quota (s_kind_limit) inside the shared array, so a unicast server
 * and a broadcast sink can coexist without starving each other.
 */
#if BLE_AUDIO_UNICAST_SERVER_SUPPORTED
#define POOL_US (CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT + CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT)
#else
#define POOL_US 0
#endif
#if BLE_AUDIO_UNICAST_CLIENT_SUPPORTED
#define POOL_UC CONFIG_BT_BAP_UNICAST_CLIENT_GROUP_STREAM_COUNT
#else
#define POOL_UC 0
#endif
#if BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED
#define POOL_BSRC CONFIG_BT_BAP_BROADCAST_SRC_STREAM_COUNT
#else
#define POOL_BSRC 0
#endif
#if BLE_AUDIO_BROADCAST_SINK_SUPPORTED
#define POOL_BSNK CONFIG_BT_BAP_BROADCAST_SNK_STREAM_COUNT
#else
#define POOL_BSNK 0
#endif
#define POOL_SIZE  (POOL_US + POOL_UC + POOL_BSRC + POOL_BSNK)
#define POOL_ALLOC (POOL_SIZE > 0 ? POOL_SIZE : 1)

/**
 * Unicast client progress of one stream through the ASCS sequence. A *_PENDING
 * stage means the control-point operation was sent and its stream callback is
 * awaited; the sequencer (uc_advance) moves at most one stream at a time.
 */
enum {
  UC_IDLE = 0,       /* Not part of a client setup (or CAP-driven). */
  UC_REQUESTED,      /* Listed by bleAudioUcStart(), nothing sent yet. */
  UC_CONFIG_PENDING, /* Config Codec sent. */
  UC_CONFIGURED,
  UC_QOS_PENDING, /* Config QoS sent (one per connection, covers all its streams). */
  UC_QOS_SET,
  UC_ENABLE_PENDING, /* Enable sent. */
  UC_ENABLED,
  UC_CONNECTING, /* CIS create requested. */
  UC_STREAMING,
};

/** One stream of the pool. */
struct ble_audio_slot {
  esp_ble_audio_cap_stream_t cap; /* MUST stay first: its first member is the BAP stream (see slot_of). */
  _Atomic(void *) owner;          /* C++ stream Impl, passed back in every s_cbs call. */
  _Atomic(uint8_t) kind;          /* ble_audio_stream_kind_t. */
  _Atomic bool in_use;            /* Pool ownership (C++ handle or zombie, see bleAudioStreamFree). */
  _Atomic bool bound;             /* Unicast: bound to a remote ASE (server) or endpoint (client). */
  _Atomic bool tx;                /* The local device sends on this stream. */
  _Atomic bool streaming;
  _Atomic uint8_t uc_stage;                 /* Unicast client: UC_* stage (caller task validates, host task advances). */
  _Atomic uint16_t conn_handle;             /* Unicast ACL, CONN_NONE otherwise. */
  _Atomic uint16_t seq;                     /* Next Tx sequence number; advances only on accepted sends. */
  _Atomic(esp_ble_audio_bap_ep_t *) uc_ep;  /* Unicast client: remote endpoint. */
  _Atomic(struct ble_audio_slot *) partner; /* Opposite direction sharing the same CIS. */
  esp_ble_audio_codec_cfg_t codec_cfg;      /* Slot-owned: the engine keeps pointers to these. */
  /* qos_cfg is written only by setup paths (SetCodec/SetQos at configure
   * time, slot reset at alloc/deinit) and read by Send only while streaming,
   * so no lock guards it: re-running SetQos on a streaming slot would race
   * the codec-task read. Snapshots (below) are the cross-task API. */
  esp_ble_audio_bap_qos_cfg_t qos_cfg;
  ble_audio_codec_t snap_codec; /* Decoded snapshot for cross-core codecConfig(). */
  ble_audio_qos_t snap_qos;     /* Decoded snapshot for cross-core qos(). */
  bool has_codec;
  bool has_qos;
  uint8_t cfg_data[CFG_DATA_MAX]; /* Backing store of codec_cfg.data. */
  uint8_t cfg_meta[META_CTX_LEN]; /* Backing store of codec_cfg.meta. */
  uint8_t bis_data[6];            /* Broadcast source: per-BIS channel allocation LTV. */
};

static ble_audio_slot_t s_pool[POOL_ALLOC];
static portMUX_TYPE s_slot_mux = portMUX_INITIALIZER_UNLOCKED;                  /* Guards snap_codec / snap_qos. */
static const uint8_t s_kind_limit[] = {POOL_US, POOL_UC, POOL_BSRC, POOL_BSNK}; /* Indexed by kind. */
static ble_audio_stream_cbs_t s_cbs;                                            /* C++ stream callbacks (bleAudioStreamSetCallbacks). */
#if BLE_AUDIO_BROADCAST_SINK_SUPPORTED
static _Atomic bool s_bsnk_joined; /* BSINK_STARTED already emitted for the current BIG. */
#endif
#if BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED
static _Atomic bool s_bsrc_up; /* BSRC_STARTED already emitted for the current BIG. */
static void bsrc_report_started(void);
extern void bleAudioNotifyBsrcState(void);
extern void bleAudioArmBsrcStateWait(void);
extern bool bleAudioWaitBsrcState(uint32_t timeoutMs);
#endif

#define BAP(slot) (&(slot)->cap.bap_stream)

/** @brief container_of: the BAP stream is the first member of the first member of the slot. */
static inline ble_audio_slot_t *slot_of(esp_ble_audio_bap_stream_t *stream) {
  return (ble_audio_slot_t *)stream;
}

static void uc_on_stream_event(ble_audio_slot_t *slot, uint8_t stage);
static void uc_check_released(void);
static void uc_check_released_locked(void);
static void uc_advance(void);
static void uc_advance_locked(void);
static void uc_stop_locked(void);
static void uc_bump(void);

/*
 * BAP stream ops, shared by every slot. Each one updates the slot, feeds the
 * unicast client sequencer when the slot belongs to it, then calls C++.
 */

static void slot_refresh_snapshots(ble_audio_slot_t *s) {
  ble_audio_codec_t c = {0};
  ble_audio_qos_t q = {0};
  const bool codec_ok = parse_cfg(BAP(s)->codec_cfg, &c);
  const bool qos_ok = BAP(s)->qos != NULL;
  if (qos_ok) {
    qos_from_cfg(BAP(s)->qos, &q);
  }
  portENTER_CRITICAL(&s_slot_mux);
  if (codec_ok) {
    s->snap_codec = c;
    s->has_codec = true;
  }
  if (qos_ok) {
    s->snap_qos = q;
    s->has_qos = true;
  }
  portEXIT_CRITICAL(&s_slot_mux);
}

static void op_configured(esp_ble_audio_bap_stream_t *stream, const esp_ble_audio_bap_qos_cfg_pref_t *pref) {
  (void)pref;
  ble_audio_slot_t *s = slot_of(stream);
  slot_refresh_snapshots(s);
  if (s->kind == BLE_AUDIO_STREAM_UNICAST_CLIENT) {
    uc_on_stream_event(s, UC_CONFIGURED);
  }
  if (s_cbs.configured) {
    s_cbs.configured(s->owner);
  }
}

static void op_qos_set(esp_ble_audio_bap_stream_t *stream) {
  ble_audio_slot_t *s = slot_of(stream);
  slot_refresh_snapshots(s);
  if (s->kind == BLE_AUDIO_STREAM_UNICAST_CLIENT) {
    uc_on_stream_event(s, UC_QOS_SET);
  }
}

static void op_enabled(esp_ble_audio_bap_stream_t *stream) {
  ble_audio_slot_t *s = slot_of(stream);
  if (s->kind == BLE_AUDIO_STREAM_UNICAST_SERVER && !s->tx) {
    /* The receiver starts: a server starts its own sink ASEs once enabled. */
    esp_err_t err = esp_ble_audio_bap_stream_start(stream);
    if (err != ESP_OK) {
      /* A hung Enabling ASE is worse than a released one: the peer waits for
       * Receiver Start Ready that will never come. Release best-effort so the
       * peer sees Released instead of silence. */
      ESP_LOGE(TAG, "unicast server: Receiver Start Ready on sink ASE (conn %u) failed: %d, releasing", s->conn_handle, err);
      if (esp_ble_audio_bap_stream_release(stream) != ESP_OK) {
        ESP_LOGE(TAG, "unicast server: release after failed start also failed (conn %u)", s->conn_handle);
      }
    }
  } else if (s->kind == BLE_AUDIO_STREAM_UNICAST_CLIENT) {
    uc_on_stream_event(s, UC_ENABLED);
  }
}

static void op_connected(esp_ble_audio_bap_stream_t *stream) {
  ble_audio_slot_t *s = slot_of(stream);
  if (s->kind == BLE_AUDIO_STREAM_UNICAST_CLIENT && s->uc_stage != UC_IDLE && !s->tx) {
    /* The client is the receiver of a remote source ASE; CAP-driven streams
     * (uc_stage idle) are started by the CAP procedure itself. */
    esp_err_t err = esp_ble_audio_bap_stream_start(stream);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "unicast client: Receiver Start Ready on source ASE (conn %u) failed: %d", s->conn_handle, err);
    }
  }
}

/* Streaming: the Tx sequence number restarts at 0 for every new stream instance. */
static void op_started(esp_ble_audio_bap_stream_t *stream) {
  ble_audio_slot_t *s = slot_of(stream);
  slot_refresh_snapshots(s);
  s->seq = 0;
  s->streaming = true;
#if BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED
  if (s->kind == BLE_AUDIO_STREAM_BROADCAST_SOURCE && !atomic_load(&s_bsrc_up)) {
    bool all_bis_started = true;
    for (int i = 0; i < POOL_SIZE; i++) {
      const ble_audio_slot_t *candidate = &s_pool[i];
      if (candidate->in_use && candidate->kind == BLE_AUDIO_STREAM_BROADCAST_SOURCE && !candidate->streaming) {
        all_bis_started = false;
        break;
      }
    }
    if (all_bis_started) {
      /* Some Bluedroid BIG paths notify every BIS stream but omit the
       * source-level started callback. The stream states are authoritative. */
      bsrc_report_started();
    }
  }
  if (s->kind == BLE_AUDIO_STREAM_BROADCAST_SOURCE) {
    bleAudioNotifyBsrcState();
  }
#endif
  if (s->kind == BLE_AUDIO_STREAM_UNICAST_CLIENT) {
    uc_on_stream_event(s, UC_STREAMING);
  }
  if (s_cbs.started) {
    s_cbs.started(s->owner);
  }
}

static void op_stopped(esp_ble_audio_bap_stream_t *stream, uint8_t reason) {
  ble_audio_slot_t *s = slot_of(stream);
  s->streaming = false;
  if (s_cbs.stopped) {
    s_cbs.stopped(s->owner, reason);
  }
  bleAudioNotifyBapIdle();
}

static void op_disconnected(esp_ble_audio_bap_stream_t *stream, uint8_t reason) {
  (void)reason;
  ble_audio_slot_t *s = slot_of(stream);
  if (s->kind == BLE_AUDIO_STREAM_UNICAST_CLIENT) {
    uc_check_released();
  }
  bleAudioNotifyBapIdle();
}

/*
 * ASE released (idle again): a unicast slot is unbound so the server can hand
 * it to the next ASE config and the client can finish its stop procedure.
 * The slot itself stays owned by its C++ stream.
 */
static void op_released(esp_ble_audio_bap_stream_t *stream) {
  ble_audio_slot_t *s = slot_of(stream);
  s->streaming = false;
  if (s->kind == BLE_AUDIO_STREAM_UNICAST_SERVER || s->kind == BLE_AUDIO_STREAM_UNICAST_CLIENT) {
    s->bound = false;
    s->conn_handle = CONN_NONE;
  }
  /* A released endpoint carries no configuration: drop the snapshots for every
   * kind so codecConfig()/qos() cannot report a codec that was never accepted
   * or has since been released (a re-created source re-publishes via Set*). */
  portENTER_CRITICAL(&s_slot_mux);
  s->has_codec = false;
  s->has_qos = false;
  portEXIT_CRITICAL(&s_slot_mux);
  if (s_cbs.released) {
    s_cbs.released(s->owner);
  }
  if (s->kind == BLE_AUDIO_STREAM_UNICAST_CLIENT) {
    s->uc_stage = UC_IDLE;
    s->uc_ep = NULL;
    /* Unpair both ways: leaving the mate's pointer at this now-idle slot
     * would skip it in group creation and stall its CIS connect forever. The
     * CAS only clears a pointer that still names this slot, so a concurrent
     * re-pairing on another task is never clobbered. */
    ble_audio_slot_t *mate = atomic_load(&s->partner);
    atomic_store(&s->partner, NULL);
    if (mate != NULL) {
      ble_audio_slot_t *expected = s;
      atomic_compare_exchange_strong(&mate->partner, &expected, NULL);
    }
    uc_bump(); /* A pass holding the guard must re-scan: this slot just went idle. */
    uc_check_released();
  }
  if (s->owner == NULL) {
    /* Zombie (freed C++ handle, see bleAudioStreamFree): the stack is done
     * with the slot, hand it back to the pool. */
    atomic_store(&s->in_use, false);
  }
  bleAudioNotifyBapIdle();
}

/* Hot path (once per SDU interval): map the ISO flags onto BLE_AUDIO_SDU_* and forward. */
static void op_recv(esp_ble_audio_bap_stream_t *stream, const esp_ble_iso_recv_info_t *info, const uint8_t *data, uint16_t len) {
  if (!s_cbs.recv) {
    return;
  }
  ble_audio_slot_t *s = slot_of(stream);
  ble_audio_recv_info_t ri = {0};
  if (info) {
    ri.ts = info->ts;
    ri.seq = info->seq_num;
    ri.ts_valid = (info->flags & ESP_BLE_ISO_FLAGS_TS) != 0;
    /* BT_ISO flags (esp_ble_iso_common_api.h mirrors Zephyr): every interval
     * is either delivered (VALID, possibly with ERROR) or LOST. Anything else
     * (no VALID without LOST) is not a defined benign state, so conceal it. */
    if (info->flags & ESP_BLE_ISO_FLAGS_LOST) {
      ri.status = BLE_AUDIO_SDU_LOST;
    } else if ((info->flags & ESP_BLE_ISO_FLAGS_ERROR) || !(info->flags & ESP_BLE_ISO_FLAGS_VALID)) {
      ri.status = BLE_AUDIO_SDU_INVALID;
    }
  }
  s_cbs.recv(s->owner, &ri, data, len);
}

static void op_sent(esp_ble_audio_bap_stream_t *stream, void *user_data) {
  (void)user_data;
  if (s_cbs.sent) {
    s_cbs.sent(slot_of(stream)->owner);
  }
}

static esp_ble_audio_bap_stream_ops_t s_ops = {
  .configured = op_configured,
  .qos_set = op_qos_set,
  .enabled = op_enabled,
  .released = op_released,
  .started = op_started,
  .stopped = op_stopped,
  .recv = op_recv,
  .sent = op_sent,
  .connected = op_connected,
  .disconnected = op_disconnected,
};

void bleAudioStreamSetCallbacks(const ble_audio_stream_cbs_t *cbs) {
  if (cbs) {
    s_cbs = *cbs;
  } else {
    memset(&s_cbs, 0, sizeof(s_cbs));
  }
}

/** @brief Reset @p s for a new owner; a broadcast source slot is Tx by construction. */
static void slot_init(ble_audio_slot_t *s, ble_audio_stream_kind_t kind, void *owner) {
  /* No memset over the slot: in_use/bound/tx/streaming/conn_handle/seq are
   * _Atomic and read lock-free from other tasks (codec task, app task). A
   * memset would tear a concurrent atomic load (C11 data race); every field
   * is stored explicitly instead. The stack-owned cap struct has no atomics
   * and is still zeroed in one go. New fields must be added to both halves. */
  memset(&s->cap, 0, sizeof(s->cap));
  atomic_store(&s->in_use, true);
  s->kind = (uint8_t)kind;
  s->owner = owner;
  atomic_store(&s->bound, false);
  atomic_store(&s->tx, kind == BLE_AUDIO_STREAM_BROADCAST_SOURCE);
  atomic_store(&s->streaming, false);
  atomic_store(&s->uc_stage, UC_IDLE);
  atomic_store(&s->conn_handle, CONN_NONE);
  atomic_store(&s->seq, 0);
  s->uc_ep = NULL;
  s->partner = NULL;
  memset(&s->codec_cfg, 0, sizeof(s->codec_cfg));
  memset(&s->qos_cfg, 0, sizeof(s->qos_cfg));
  portENTER_CRITICAL(&s_slot_mux);
  memset(&s->snap_codec, 0, sizeof(s->snap_codec));
  memset(&s->snap_qos, 0, sizeof(s->snap_qos));
  s->has_codec = false;
  s->has_qos = false;
  portEXIT_CRITICAL(&s_slot_mux);
  memset(s->cfg_data, 0, sizeof(s->cfg_data));
  memset(s->cfg_meta, 0, sizeof(s->cfg_meta));
  memset(s->bis_data, 0, sizeof(s->bis_data));
#if USE_CAP_OPS
  esp_ble_audio_cap_stream_ops_register(&s->cap, &s_ops);
#else
  esp_ble_audio_bap_stream_cb_register(BAP(s), &s_ops);
#endif
}

/** @brief Stack-free reset for the deinit path (the host mutex is already gone). */
static void slot_reset_nostack(ble_audio_slot_t *s) {
  uint8_t kind = s->kind;
  void *owner = s->owner;
  memset(&s->cap, 0, sizeof(s->cap));
  atomic_store(&s->in_use, true);
  s->kind = kind;
  s->owner = owner;
  atomic_store(&s->bound, false);
  atomic_store(&s->tx, kind == BLE_AUDIO_STREAM_BROADCAST_SOURCE);
  atomic_store(&s->streaming, false);
  atomic_store(&s->uc_stage, UC_IDLE);
  atomic_store(&s->conn_handle, CONN_NONE);
  atomic_store(&s->seq, 0);
  s->uc_ep = NULL;
  s->partner = NULL;
  memset(&s->codec_cfg, 0, sizeof(s->codec_cfg));
  memset(&s->qos_cfg, 0, sizeof(s->qos_cfg));
  portENTER_CRITICAL(&s_slot_mux);
  memset(&s->snap_codec, 0, sizeof(s->snap_codec));
  memset(&s->snap_qos, 0, sizeof(s->snap_qos));
  s->has_codec = false;
  s->has_qos = false;
  portEXIT_CRITICAL(&s_slot_mux);
  memset(s->cfg_data, 0, sizeof(s->cfg_data));
  memset(s->cfg_meta, 0, sizeof(s->cfg_meta));
  memset(s->bis_data, 0, sizeof(s->bis_data));
}

ble_audio_slot_t *bleAudioStreamAlloc(ble_audio_stream_kind_t kind, void *owner) {
  if ((unsigned)kind >= sizeof(s_kind_limit)) {
    return NULL;
  }
  uint8_t used = 0;
  for (int i = 0; i < POOL_SIZE; i++) {
    if (s_pool[i].in_use && s_pool[i].kind == kind) {
      used++;
    }
  }
  if (used >= s_kind_limit[kind]) {
    ESP_LOGE(TAG, "stream pool: all %u slots of kind %d in use (Kconfig limit)", s_kind_limit[kind], (int)kind);
    return NULL;
  }
  for (int i = 0; i < POOL_SIZE; i++) {
    ble_audio_slot_t *s = &s_pool[i];
    bool expected = false;
    if (atomic_compare_exchange_strong(&s->in_use, &expected, true)) {
      slot_init(s, kind, owner);
      return s;
    }
  }
  return NULL;
}

/* Called from the C++ Impl destructor. A slot the stack still references
 * (streaming, bound, or mid-setup) cannot be handed out again: it becomes a
 * zombie (owned, no callbacks) until the stack releases it, so a later event
 * can neither reach a destroyed Impl nor be delivered to a new owner. */
void bleAudioStreamFree(ble_audio_slot_t *slot) {
  if (slot == NULL) {
    return;
  }
#if BLE_AUDIO_UNICAST_CLIENT_SUPPORTED
  const uint8_t stage = atomic_load(&slot->uc_stage);
  const bool uc_busy = slot->kind == BLE_AUDIO_STREAM_UNICAST_CLIENT && stage != UC_IDLE;
#else
  const bool uc_busy = false;
#endif
  if (slot->streaming || slot->bound || uc_busy
#if BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED
      || (slot->kind == BLE_AUDIO_STREAM_BROADCAST_SOURCE && atomic_load(&s_bsrc_up))
#endif
  ) {
    slot->owner = NULL; /* Zombie: state updates continue, C++ callbacks are dropped. */
    return;
  }
  atomic_store(&slot->in_use, false);
  slot->owner = NULL;
}

/* Hot path: no logging; the caller reports the status. */
int bleAudioStreamSend(ble_audio_slot_t *slot, const uint8_t *sdu, uint16_t len) {
  if (slot == NULL || !slot->streaming || !slot->tx) {
    return ESP_ERR_INVALID_STATE;
  }
  /* Bound the SDU ourselves: an oversized send reaches the controller and is
   * then rejected per packet (or corrupts the stream timing). A zero sdu means
   * the QoS was never populated; let the engine decide in that case. */
  if (slot->qos_cfg.sdu && len > slot->qos_cfg.sdu) {
    return ESP_ERR_INVALID_SIZE;
  }
  esp_err_t err = esp_ble_audio_bap_stream_send(BAP(slot), sdu, len, slot->seq);
  if (err == ESP_OK) {
    slot->seq++;
  }
  return (int)err;
}

bool bleAudioStreamIsStreaming(const ble_audio_slot_t *slot) {
  return slot && slot->streaming;
}

bool bleAudioBapIsoBusy(void) {
  for (int i = 0; i < POOL_ALLOC; i++) {
    ble_audio_slot_t *s = &s_pool[i];
    if (!s->in_use) {
      continue;
    }
    if (s->streaming) {
      return true;
    }
    struct bt_iso_chan *iso = BAP(s)->iso;
    if (iso != NULL && iso->state != BT_ISO_STATE_DISCONNECTED) {
      return true;
    }
  }
  return false;
}

void bleAudioBapStopLeftover(void) {
#if BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED
  (void)bleAudioBsrcStop();
#endif
#if BLE_AUDIO_BROADCAST_SINK_SUPPORTED
  (void)bleAudioBsinkStop();
#endif
#if BLE_AUDIO_UNICAST_CLIENT_SUPPORTED
  (void)bleAudioUcStop();
#endif
}

bool bleAudioStreamIsBound(const ble_audio_slot_t *slot) {
  return slot && slot->bound;
}

uint8_t bleAudioStreamKind(const ble_audio_slot_t *slot) {
  return slot ? slot->kind : 0xFF;
}

bool bleAudioStreamIsTx(const ble_audio_slot_t *slot) {
  return slot && slot->tx;
}

uint16_t bleAudioStreamConnHandle(const ble_audio_slot_t *slot) {
  return slot ? slot->conn_handle : CONN_NONE;
}

bool bleAudioStreamGetCodec(const ble_audio_slot_t *slot, ble_audio_codec_t *out) {
  if (slot == NULL || out == NULL) {
    return false;
  }
  bool ok;
  portENTER_CRITICAL(&s_slot_mux);
  ok = slot->has_codec;
  if (ok) {
    *out = slot->snap_codec;
  }
  portEXIT_CRITICAL(&s_slot_mux);
  return ok;
}

bool bleAudioStreamGetQos(const ble_audio_slot_t *slot, ble_audio_qos_t *out) {
  if (slot == NULL || out == NULL) {
    return false;
  }
  bool ok;
  portENTER_CRITICAL(&s_slot_mux);
  ok = slot->has_qos;
  if (ok) {
    *out = slot->snap_qos;
  }
  portEXIT_CRITICAL(&s_slot_mux);
  return ok;
}

void *bleAudioStreamCap(ble_audio_slot_t *slot) {
  return slot ? &slot->cap : NULL;
}

/* Range check so a BAP stream that is not ours (e.g. from another stack user) maps to NULL. */
ble_audio_slot_t *bleAudioStreamFromBap(const void *bap) {
  const ble_audio_slot_t *s = (const ble_audio_slot_t *)bap;
  if (s < &s_pool[0] || s >= &s_pool[POOL_SIZE]) {
    return NULL;
  }
  return (ble_audio_slot_t *)s;
}

/*
 * The config and its LTV buffers live in the slot because the engine keeps a
 * pointer to them for the stream's whole lifetime. Low latency / 2M targets
 * match the BAP low-latency presets used throughout.
 */
void *bleAudioStreamSetCodec(ble_audio_slot_t *slot, const ble_audio_codec_t *codec, uint16_t context) {
  if (slot == NULL || codec == NULL) {
    return NULL;
  }
  esp_ble_audio_codec_cfg_t *cfg = &slot->codec_cfg;
  memset(cfg, 0, sizeof(*cfg));
  cfg->id = ESP_BLE_ISO_CODING_FORMAT_LC3;
  cfg->target_latency = ESP_BLE_AUDIO_CODEC_CFG_TARGET_LATENCY_LOW;
  cfg->target_phy = ESP_BLE_AUDIO_CODEC_CFG_TARGET_PHY_2M;
  cfg->data_len = build_cfg_data(slot->cfg_data, codec);
  cfg->data = slot->cfg_data;
  if (cfg->data_len == 0) {
    ESP_LOGE(TAG, "codec %lu Hz / %u us has no LC3 configuration", (unsigned long)codec->sample_rate_hz, codec->frame_dur_us);
    return NULL;
  }
  cfg->meta_len = build_ctx_meta(slot->cfg_meta, ESP_BLE_AUDIO_METADATA_TYPE_STREAM_CONTEXT, context ? context : ESP_BLE_AUDIO_CONTEXT_TYPE_UNSPECIFIED);
  cfg->meta = slot->cfg_meta;
  portENTER_CRITICAL(&s_slot_mux);
  slot->snap_codec = *codec;
  slot->snap_codec.frames_per_sdu = codec->frames_per_sdu ? codec->frames_per_sdu : 1;
  slot->has_codec = true;
  portEXIT_CRITICAL(&s_slot_mux);
  return cfg;
}

void *bleAudioStreamSetQos(ble_audio_slot_t *slot, const ble_audio_qos_t *qos) {
  if (slot == NULL || qos == NULL) {
    return NULL;
  }
  qos_to_cfg(qos, &slot->qos_cfg);
  portENTER_CRITICAL(&s_slot_mux);
  slot->snap_qos = *qos;
  slot->snap_qos.phy = qos->phy ? qos->phy : BLE_AUDIO_PHY_2M;
  slot->has_qos = true;
  portEXIT_CRITICAL(&s_slot_mux);
  return &slot->qos_cfg;
}

void bleAudioStreamBind(ble_audio_slot_t *slot, uint16_t conn_handle, bool tx) {
  if (slot) {
    atomic_store(&slot->bound, true);
    atomic_store(&slot->conn_handle, conn_handle);
    atomic_store(&slot->tx, tx);
  }
}

/** @brief Release a CAP-driven unicast stream binding (slot stays owned). */
void bleAudioStreamUnbind(ble_audio_slot_t *slot) {
  if (slot) {
    atomic_store(&slot->bound, false);
    atomic_store(&slot->conn_handle, CONN_NONE);
  }
}

/* ── PACS ───────────────────────────────────────────────────────────────── */

/*
 * One merged LC3 record per direction. Roles stage into it while applying
 * (bleAudioPacsAdd) and bleAudioPacsCommit registers it once, so a unicast
 * server and a broadcast sink publish a single sink PAC covering both.
 */
typedef struct {
  bool used;
  ble_audio_pac_t pac;        /* Union of the staged capabilities. */
  uint32_t location;          /* Union of the staged Audio Locations. */
  uint16_t contexts;          /* Union of the staged contexts (supported = initially available). */
  uint8_t data[CFG_DATA_MAX]; /* Backing store of codec.data (the engine keeps the pointer). */
  uint8_t meta[META_CTX_LEN]; /* Backing store of codec.meta. */
  esp_ble_audio_codec_cap_t codec;
  esp_ble_audio_pacs_cap_t cap;
} pac_rec_t;

static pac_rec_t s_pac[2];           /* [0] sink, [1] source */
static uint16_t s_pacs_supported[2]; /* First published supported contexts, per direction. */
static bool s_pacs_committed;

/* Rate / duration / octets of each BLEAudioCodecPreset (index order). */
static const struct {
  uint16_t freq_bit;
  uint8_t dur_bit;
  uint8_t octets;
} s_preset_caps[BLE_AUDIO_PRESET_COUNT] = {
  {ESP_BLE_AUDIO_CODEC_CAP_FREQ_8KHZ, ESP_BLE_AUDIO_CODEC_CAP_DURATION_7_5, 26},
  {ESP_BLE_AUDIO_CODEC_CAP_FREQ_8KHZ, ESP_BLE_AUDIO_CODEC_CAP_DURATION_10, 30},
  {ESP_BLE_AUDIO_CODEC_CAP_FREQ_16KHZ, ESP_BLE_AUDIO_CODEC_CAP_DURATION_7_5, 30},
  {ESP_BLE_AUDIO_CODEC_CAP_FREQ_16KHZ, ESP_BLE_AUDIO_CODEC_CAP_DURATION_10, 40},
  {ESP_BLE_AUDIO_CODEC_CAP_FREQ_24KHZ, ESP_BLE_AUDIO_CODEC_CAP_DURATION_7_5, 45},
  {ESP_BLE_AUDIO_CODEC_CAP_FREQ_24KHZ, ESP_BLE_AUDIO_CODEC_CAP_DURATION_10, 60},
  {ESP_BLE_AUDIO_CODEC_CAP_FREQ_32KHZ, ESP_BLE_AUDIO_CODEC_CAP_DURATION_7_5, 60},
  {ESP_BLE_AUDIO_CODEC_CAP_FREQ_32KHZ, ESP_BLE_AUDIO_CODEC_CAP_DURATION_10, 80},
  {ESP_BLE_AUDIO_CODEC_CAP_FREQ_44KHZ, ESP_BLE_AUDIO_CODEC_CAP_DURATION_7_5, 97},
  {ESP_BLE_AUDIO_CODEC_CAP_FREQ_44KHZ, ESP_BLE_AUDIO_CODEC_CAP_DURATION_10, 130},
  {ESP_BLE_AUDIO_CODEC_CAP_FREQ_48KHZ, ESP_BLE_AUDIO_CODEC_CAP_DURATION_7_5, 75},
  {ESP_BLE_AUDIO_CODEC_CAP_FREQ_48KHZ, ESP_BLE_AUDIO_CODEC_CAP_DURATION_10, 100},
  {ESP_BLE_AUDIO_CODEC_CAP_FREQ_48KHZ, ESP_BLE_AUDIO_CODEC_CAP_DURATION_7_5, 90},
  {ESP_BLE_AUDIO_CODEC_CAP_FREQ_48KHZ, ESP_BLE_AUDIO_CODEC_CAP_DURATION_10, 120},
  {ESP_BLE_AUDIO_CODEC_CAP_FREQ_48KHZ, ESP_BLE_AUDIO_CODEC_CAP_DURATION_7_5, 117},
  {ESP_BLE_AUDIO_CODEC_CAP_FREQ_48KHZ, ESP_BLE_AUDIO_CODEC_CAP_DURATION_10, 155},
};

/*
 * The record is the union of the presets (every rate, duration and the octet
 * range), which is what PACS can express; a client may therefore pick any
 * combination inside it, not only the exact presets. Frame count equals the
 * channel count so a stereo stream can carry one frame per channel.
 */
void bleAudioPacFromPresets(uint32_t preset_mask, uint8_t max_channels, ble_audio_pac_t *out) {
  memset(out, 0, sizeof(*out));
  out->min_octets = 0xFFFF;
  for (int i = 0; i < BLE_AUDIO_PRESET_COUNT; i++) {
    if (!(preset_mask & (1UL << i))) {
      continue;
    }
    out->freq_mask |= s_preset_caps[i].freq_bit;
    out->dur_mask |= s_preset_caps[i].dur_bit;
    if (s_preset_caps[i].octets < out->min_octets) {
      out->min_octets = s_preset_caps[i].octets;
    }
    if (s_preset_caps[i].octets > out->max_octets) {
      out->max_octets = s_preset_caps[i].octets;
    }
  }
  if (out->max_octets == 0) {
    out->min_octets = 0;
  }
  if (max_channels == 0) {
    max_channels = 1;
  }
  /* chan_counts is BIT(n-1) per supported count: clamp to the Audio Location
   * width (1U << 32 is UB and anything above 8 has no defined location bit). */
  if (max_channels > 8) {
    ESP_LOGW(TAG, "PACS capability: clamping channel count %u to 8", max_channels);
    max_channels = 8;
  }
  out->chan_counts = (uint8_t)((1U << max_channels) - 1U);
  out->max_frames = max_channels;
}

/*
 * A preset fits when its rate and duration bits are set and its octet count
 * lies in the record's range. Records are per direction and a peer may publish
 * several, so callers OR the results of every record.
 */
uint32_t bleAudioPacPresets(const ble_audio_pac_t *pac) {
  uint32_t mask = 0;
  if (pac == NULL) {
    return 0;
  }
  for (int i = 0; i < BLE_AUDIO_PRESET_COUNT; i++) {
    if ((pac->freq_mask & s_preset_caps[i].freq_bit) && (pac->dur_mask & s_preset_caps[i].dur_bit) && s_preset_caps[i].octets >= pac->min_octets
        && s_preset_caps[i].octets <= pac->max_octets) {
      mask |= 1UL << i;
    }
  }
  return mask;
}

void bleAudioPacsAdd(uint8_t dir, const ble_audio_pac_t *pac, uint32_t location, uint16_t contexts) {
  if ((dir != BLE_AUDIO_DIR_SINK && dir != BLE_AUDIO_DIR_SOURCE) || pac == NULL) {
    return;
  }
  if (pac->freq_mask == 0 || pac->dur_mask == 0 || pac->max_octets == 0) {
    /* An empty preset mask (or a record no preset fits) would publish an
     * unusable LC3 PAC; refuse it loudly instead of advertising garbage. */
    ESP_LOGE(TAG, "PACS capability: refusing empty LC3 record (no preset fits)");
    return;
  }
  pac_rec_t *r = &s_pac[dir == BLE_AUDIO_DIR_SINK ? 0 : 1];
  if (!r->used) {
    r->used = true;
    r->pac = *pac;
  } else {
    r->pac.freq_mask |= pac->freq_mask;
    r->pac.dur_mask |= pac->dur_mask;
    r->pac.chan_counts |= pac->chan_counts;
    if (pac->min_octets < r->pac.min_octets) {
      r->pac.min_octets = pac->min_octets;
    }
    if (pac->max_octets > r->pac.max_octets) {
      r->pac.max_octets = pac->max_octets;
    }
    if (pac->max_frames > r->pac.max_frames) {
      r->pac.max_frames = pac->max_frames;
    }
  }
  r->location |= location;
  r->contexts |= contexts;
}

/** @brief Encode @p p as LC3 Codec Specific Capabilities LTVs (frame count only when > 1). */
static size_t build_cap_data(uint8_t *d, const ble_audio_pac_t *p) {
  size_t n = 0;
  d[n++] = 3;
  d[n++] = ESP_BLE_AUDIO_CODEC_CAP_TYPE_FREQ;
  ble_audio_put_le16(&d[n], p->freq_mask);
  n += 2;
  d[n++] = 2;
  d[n++] = ESP_BLE_AUDIO_CODEC_CAP_TYPE_DURATION;
  d[n++] = p->dur_mask;
  d[n++] = 2;
  d[n++] = ESP_BLE_AUDIO_CODEC_CAP_TYPE_CHAN_COUNT;
  d[n++] = p->chan_counts;
  d[n++] = 5;
  d[n++] = ESP_BLE_AUDIO_CODEC_CAP_TYPE_FRAME_LEN;
  ble_audio_put_le16(&d[n], p->min_octets);
  ble_audio_put_le16(&d[n + 2], p->max_octets);
  n += 4;
  if (p->max_frames > 1) {
    d[n++] = 2;
    d[n++] = ESP_BLE_AUDIO_CODEC_CAP_TYPE_FRAME_COUNT;
    d[n++] = p->max_frames;
  }
  return n;
}

/*
 * Unspecified is always added to the contexts so a client that does not pick
 * a specific context can still configure the direction. Staging is closed
 * after the first successful commit.
 */

int bleAudioPacsCommit(void) {
  if (s_pacs_committed || (!s_pac[0].used && !s_pac[1].used)) {
    return 0;
  }
  const esp_ble_audio_pacs_register_param_t reg = {
    .snk_pac = s_pac[0].used,
    .snk_loc = s_pac[0].used,
    .src_pac = s_pac[1].used,
    .src_loc = s_pac[1].used,
  };
  esp_err_t err = esp_ble_audio_pacs_register(&reg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_audio_pacs_register (sink=%d source=%d) failed: %d", s_pac[0].used, s_pac[1].used, err);
    return (int)err;
  }
  for (int i = 0; i < 2; i++) {
    pac_rec_t *r = &s_pac[i];
    if (!r->used) {
      continue;
    }
    esp_ble_audio_dir_t dir = (i == 0) ? ESP_BLE_AUDIO_DIR_SINK : ESP_BLE_AUDIO_DIR_SOURCE;
    uint16_t ctx = r->contexts | ESP_BLE_AUDIO_CONTEXT_TYPE_UNSPECIFIED;
    memset(&r->codec, 0, sizeof(r->codec));
    r->codec.id = ESP_BLE_ISO_CODING_FORMAT_LC3;
    r->codec.data_len = build_cap_data(r->data, &r->pac);
    r->codec.data = r->data;
    r->codec.meta_len = build_ctx_meta(r->meta, ESP_BLE_AUDIO_METADATA_TYPE_PREF_CONTEXT, ctx);
    r->codec.meta = r->meta;
    r->cap.codec_cap = &r->codec;
    err = esp_ble_audio_pacs_cap_register(dir, &r->cap);
    if (err == ESP_OK) {
      err = esp_ble_audio_pacs_set_location(dir, r->location);
    }
    if (err == ESP_OK) {
      err = esp_ble_audio_pacs_set_supported_contexts(dir, ctx);
      if (err == -EALREADY) {
        /* The lib publishes the supported-contexts value of a PAC record once
         * per boot: after the first successful set it answers later sets with
         * -EALREADY and keeps serving the first value, even when the value
         * differs from what the roles now stage (verified on hardware:
         * cap_register/set_location/set_available all re-accept a re-commit,
         * only set_supported does not). Failing the commit instead would also
         * abort the broadcast sink/delegator role bring-up, whose teardown
         * leaves the stack's broadcast BIG callbacks unregistered for every
         * later broadcast sink session of the same boot. Keep serving the
         * first value, but say so loudly: re-begin() with different contexts
         * is a documented boot-pinned divergence (see DESIGN.md), and the
         * validation suite asserts the published value after re-begin. */
        ESP_LOGW(TAG, "PACS %s supported contexts already published this boot, keeping it", i == 0 ? "sink" : "source");
        err = ESP_OK;
        /* Available must stay a subset of the pinned supported value; clamp
         * newly staged bits that the stack will never publish as supported. */
        ctx &= (uint16_t)(s_pacs_supported[i] | ESP_BLE_AUDIO_CONTEXT_TYPE_UNSPECIFIED);
      } else if (err == ESP_OK) {
        s_pacs_supported[i] = ctx;
      }
    }
    if (err == ESP_OK) {
      err = esp_ble_audio_pacs_set_available_contexts(dir, ctx);
    }
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "PACS %s record/location/contexts setup failed: %d", i == 0 ? "sink" : "source", err);
      return (int)err;
    }
  }
  s_pacs_committed = true;
  return 0;
}

int bleAudioPacsSetAvailable(uint8_t dir, uint16_t contexts) {
  if (!s_pacs_committed) {
    return ESP_ERR_INVALID_STATE;
  }
  const int idx = (dir == BLE_AUDIO_DIR_SINK) ? 0 : 1;
  if (s_pacs_supported[idx] != 0) {
    contexts &= (uint16_t)(s_pacs_supported[idx] | ESP_BLE_AUDIO_CONTEXT_TYPE_UNSPECIFIED);
  }
  return (int)esp_ble_audio_pacs_set_available_contexts((esp_ble_audio_dir_t)dir, contexts);
}

/* ── Unicast server ─────────────────────────────────────────────────────── */

#if BLE_AUDIO_UNICAST_SERVER_SUPPORTED

static esp_ble_audio_bap_qos_cfg_pref_t s_us_pref; /* QoS preferences returned with every config. */
static bool s_us_registered;

/*
 * ASCS server callbacks: a remote client drives our ASEs. The codec has
 * already been checked against our PAC by the stack, so the server accepts
 * every request that fits; the stream ops report the outcome to C++.
 */

/**
 * @brief Config Codec on an idle ASE: bind it to the first free server slot.
 *
 * A source ASE (the client receives) becomes a Tx slot here. Rejected with
 * "insufficient resources" when every slot is bound.
 */
static int us_config(
  esp_ble_conn_t *conn, const esp_ble_audio_bap_ep_t *ep, esp_ble_audio_dir_t dir, const esp_ble_audio_codec_cfg_t *codec_cfg,
  esp_ble_audio_bap_stream_t **stream, esp_ble_audio_bap_qos_cfg_pref_t *const pref, esp_ble_audio_bap_ascs_rsp_t *rsp
) {
  (void)ep;
  (void)codec_cfg;
  for (int i = 0; i < POOL_SIZE; i++) {
    ble_audio_slot_t *s = &s_pool[i];
    if (s->in_use && s->kind == BLE_AUDIO_STREAM_UNICAST_SERVER && !s->bound) {
      bleAudioStreamBind(s, conn->handle, dir == ESP_BLE_AUDIO_DIR_SOURCE);
      *stream = BAP(s);
      *pref = s_us_pref;
      return 0;
    }
  }
  ESP_LOGW(TAG, "unicast server: conn %u configured more ASEs than free streams, rejecting", conn->handle);
  *rsp = ESP_BLE_AUDIO_BAP_ASCS_RSP(ESP_BLE_AUDIO_BAP_ASCS_RSP_CODE_NO_MEM, ESP_BLE_AUDIO_BAP_ASCS_REASON_NONE);
  return -ENOMEM;
}

/** @brief Config Codec on an already configured ASE: keep the binding, return the same preferences. */
static int us_reconfig(
  esp_ble_audio_bap_stream_t *stream, esp_ble_audio_dir_t dir, const esp_ble_audio_codec_cfg_t *codec_cfg, esp_ble_audio_bap_qos_cfg_pref_t *const pref,
  esp_ble_audio_bap_ascs_rsp_t *rsp
) {
  (void)stream;
  (void)dir;
  (void)codec_cfg;
  (void)rsp;
  *pref = s_us_pref;
  return 0;
}

/* QoS, Enable, Metadata and the remaining operations are always accepted. */
static int us_qos(esp_ble_audio_bap_stream_t *stream, const esp_ble_audio_bap_qos_cfg_t *qos, esp_ble_audio_bap_ascs_rsp_t *rsp) {
  (void)stream;
  (void)qos;
  (void)rsp;
  return 0;
}

static int us_enable(esp_ble_audio_bap_stream_t *stream, const uint8_t meta[], size_t meta_len, esp_ble_audio_bap_ascs_rsp_t *rsp) {
  (void)stream;
  (void)meta;
  (void)meta_len;
  (void)rsp;
  return 0;
}

static int us_metadata(esp_ble_audio_bap_stream_t *stream, const uint8_t meta[], size_t meta_len, esp_ble_audio_bap_ascs_rsp_t *rsp) {
  return us_enable(stream, meta, meta_len, rsp);
}

static int us_accept(esp_ble_audio_bap_stream_t *stream, esp_ble_audio_bap_ascs_rsp_t *rsp) {
  (void)stream;
  (void)rsp;
  return 0;
}

static const esp_ble_audio_bap_unicast_server_cb_t s_us_cb = {
  .config = us_config,
  .reconfig = us_reconfig,
  .qos = us_qos,
  .enable = us_enable,
  .start = us_accept,
  .metadata = us_metadata,
  .disable = us_accept,
  .stop = us_accept,
  .release = us_accept,
};

/* ASE counts are clamped to the packaged Kconfig; registering twice is a no-op. */
int bleAudioUsInit(uint8_t snk_cnt, uint8_t src_cnt, const ble_audio_qos_pref_t *pref) {
  if (s_us_registered) {
    return 0;
  }
  if (pref) {
    s_us_pref.unframed_supported = pref->unframed;
    s_us_pref.phy = pref->phy ? pref->phy : BLE_AUDIO_PHY_2M;
    s_us_pref.rtn = pref->rtn;
    s_us_pref.latency = pref->latency_ms;
    s_us_pref.pd_min = pref->pd_min_us;
    s_us_pref.pd_max = pref->pd_max_us;
    s_us_pref.pref_pd_min = pref->pref_pd_min_us;
    s_us_pref.pref_pd_max = pref->pref_pd_max_us;
  }
  const esp_ble_audio_bap_unicast_server_register_param_t reg = {
    .snk_cnt = snk_cnt > CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT ? CONFIG_BT_ASCS_MAX_ASE_SNK_COUNT : snk_cnt,
    .src_cnt = src_cnt > CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT ? CONFIG_BT_ASCS_MAX_ASE_SRC_COUNT : src_cnt,
  };
  esp_err_t err = esp_ble_audio_bap_unicast_server_register(&reg);
  if (err == ESP_OK) {
    err = esp_ble_audio_bap_unicast_server_register_cb(&s_us_cb);
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "unicast server: ASCS registration (%u sink / %u source ASEs) failed: %d", reg.snk_cnt, reg.src_cnt, err);
    return (int)err;
  }
  s_us_registered = true;
  return 0;
}

#endif /* BLE_AUDIO_UNICAST_SERVER_SUPPORTED */

/* ── Unicast client ─────────────────────────────────────────────────────── */

#if BLE_AUDIO_UNICAST_CLIENT_SUPPORTED

/*
 * Setup runs as a sequencer over the slots (uc_advance): Config Codec for each
 * stream, one unicast group, Config QoS per peer, Enable per stream, then one
 * CIS connect per bidirectional pair. Every stream callback or ASCS response
 * re-enters the sequencer; any failure aborts the whole setup via uc_fail.
 */

#define UC_MAX_PEERS 2 /* Two acceptors: a left/right earbud pair (2P_2CIS). */

/** What discovery learned about one acceptor. */
typedef struct {
  uint16_t conn; /* CONN_NONE when the entry is free. */
  uint8_t n_snk;
  uint8_t n_src;
  esp_ble_audio_bap_ep_t *snk[CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SNK_COUNT]; /* Remote sink ASEs, in discovery order. */
  esp_ble_audio_bap_ep_t *src[CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SRC_COUNT]; /* Remote source ASEs. */
  ble_audio_uc_discovered_t info;                                          /* Accumulated locations/contexts, emitted with UC_DISCOVERED. */
  int sink_err;                                                            /* Sink discovery result, reported if no ASE is found at all. */
} uc_peer_t;

static struct {
  bool registered;
  _Atomic bool active;          /* A setup (or the streams it produced) is in progress. */
  _Atomic bool stopping;        /* bleAudioUcStop() is releasing; ASCS errors are ignored. */
  _Atomic bool started_emitted; /* UC_STARTED already sent for this setup. */
  esp_ble_audio_bap_unicast_group_t *group;
  uc_peer_t peers[UC_MAX_PEERS];
  /* Sequencer guard. Setup entry points run on the caller task while stream
   * callbacks run on the host task; without serialization two advancers can
   * pass the "no PENDING" scan together and issue two control-point ops,
   * violating the one-outstanding-op contract. The guard is a non-blocking
   * CAS mutex: a loser returns and the holder re-runs its pass while gen is
   * moving, so no wake-up is ever lost. uc_fail funnels through uc_advance
   * (via stop_requested) instead of stopping inline, so the guard never nests
   * across tasks. gen is bumped before every uc_advance() call made without
   * the guard held. */

  _Atomic bool guard;
  _Atomic uint32_t gen;
  _Atomic bool stop_requested; /* uc_fail asked for a stop; serviced by the advance prologue. */
} s_uc;

/* Peer-table lock. s_uc.peers[] is written by host-task discovery callbacks
 * and read by caller-task setup paths (and cleared on disconnect from either
 * side), so every lookup, claim, clear and field access runs under this
 * spinlock (same convention as s_slot_mux). It is a LEAF lock: never held
 * across esp_ble_audio_* calls (they can complete inline on the caller task
 * and re-enter through callbacks) and never across bleAudioEngineEmit (the
 * C++ dispatch runs synchronously and may call back into the engine).
 * Sections only scan the two entries, claim/clear them, or copy fields out. */
static portMUX_TYPE s_uc_peer_mux = portMUX_INITIALIZER_UNLOCKED;

/**
 * @brief Find the peer entry for @p conn, optionally claiming (and clearing) a free one.
 * @note Caller holds s_uc_peer_mux (see above): the returned pointer is only
 * valid while the lock is held. Copy what is needed, then release.
 */
static uc_peer_t *uc_peer(uint16_t conn, bool create) {
  uc_peer_t *free_peer = NULL;
  for (int i = 0; i < UC_MAX_PEERS; i++) {
    if (s_uc.peers[i].conn == conn) {
      return &s_uc.peers[i];
    }
    if (free_peer == NULL && s_uc.peers[i].conn == CONN_NONE) {
      free_peer = &s_uc.peers[i];
    }
  }
  if (create && free_peer) {
    memset(free_peer, 0, sizeof(*free_peer));
    free_peer->conn = conn;
  }
  return create ? free_peer : NULL;
}

/** @brief True for a client slot taking part in the current setup. */
static bool uc_slot_active(const ble_audio_slot_t *s) {
  return s->in_use && s->kind == BLE_AUDIO_STREAM_UNICAST_CLIENT && s->uc_stage != UC_IDLE;
}

static const char *uc_op_name(uint8_t op) {
  static const char *const names[] = {"?", "Config Codec", "Config QoS", "Enable", "Receiver Start Ready", "CIS connect", "group create"};
  return op < sizeof(names) / sizeof(names[0]) ? names[op] : names[0];
}

/**
 * @brief Abort the setup: log, emit UC_ERROR, then release every stream.
 *
 * Never stops inline: it arms stop_requested and funnels through uc_advance()
 * so the stop runs under the sequencer guard (uc_fail is called both from
 * inside a pass and from host callbacks that may race a pass).
 *
 * @param op        BLE_AUDIO_UC_OP_* that failed.
 * @param rsp       ASCS response code from the peer, 0 when the call failed locally.
 * @param reason    ASCS reason from the peer, 0 when the call failed locally.
 * @param local_err Local errno of a failed stack call, 0 for peer rejections.
 */
static void uc_fail(uint8_t op, uint8_t rsp, uint8_t reason, int local_err) {
  const ble_audio_uc_error_t e = {.op = op, .rsp_code = rsp, .reason = reason};
  if (rsp) {
    ESP_LOGE(TAG, "unicast client: peer rejected %s (rsp 0x%02x reason 0x%02x), stopping", uc_op_name(op), rsp, reason);
  } else {
    ESP_LOGE(TAG, "unicast client: %s failed locally (err %d), stopping", uc_op_name(op), local_err);
  }
  /* err carries the local errno (0 for peer rejections); the C++ error
   * callback reports op/rsp/reason and the stop follows asynchronously. */
  bleAudioEngineEmit(BLE_AUDIO_EVT_UC_ERROR, CONN_NONE, local_err ? local_err : -1, &e);
  /* Drop optimistic snapshots: SetCodec/SetQos published them during
   * validation but the peer never accepted the configuration. */
  for (int i = 0; i < POOL_SIZE; i++) {
    ble_audio_slot_t *s = &s_pool[i];
    if (s->in_use && s->kind == BLE_AUDIO_STREAM_UNICAST_CLIENT) {
      portENTER_CRITICAL(&s_slot_mux);
      s->has_codec = false;
      s->has_qos = false;
      portEXIT_CRITICAL(&s_slot_mux);
    }
  }
  atomic_store(&s_uc.stop_requested, true);
  uc_bump();
  uc_advance();
}

/**
 * @brief Create the unicast group for every active client slot.
 *
 * A sink and a source stream on the same peer are paired onto one CIS
 * (bidirectional); any stream left over gets a CIS of its own. The parameter
 * arrays are automatic locals: group_create() consumes them synchronously and
 * the sequencer guard keeps two advancers from sharing them. Callers must
 * hold the sequencer guard.
 */
static int uc_create_group(void) {
  /* POOL_UC is CONFIG_BT_BAP_UNICAST_CLIENT_GROUP_STREAM_COUNT (a few
   * entries); ~32 B each, negligible on either task stack. */
  esp_ble_audio_bap_unicast_group_stream_param_t sp[POOL_UC];
  esp_ble_audio_bap_unicast_group_stream_pair_param_t pairs[POOL_UC];
  size_t n_sp = 0, n_pairs = 0;

  for (int i = 0; i < POOL_SIZE; i++) {
    ble_audio_slot_t *s = &s_pool[i];
    if (!uc_slot_active(s) || s->partner != NULL) {
      continue;
    }
    esp_ble_audio_bap_unicast_group_stream_pair_param_t *pair = &pairs[n_pairs++];
    memset(pair, 0, sizeof(*pair));
    /* Pair with the first unpaired stream of the opposite direction on the same peer. */
    ble_audio_slot_t *mate = NULL;
    for (int j = i + 1; j < POOL_SIZE && mate == NULL; j++) {
      ble_audio_slot_t *m = &s_pool[j];
      if (uc_slot_active(m) && m->partner == NULL && m->conn_handle == s->conn_handle && m->tx != s->tx) {
        mate = m;
      }
    }
    ble_audio_slot_t *members[2] = {s, mate};
    for (int k = 0; k < 2; k++) {
      ble_audio_slot_t *m = members[k];
      if (m == NULL) {
        continue;
      }
      sp[n_sp].stream = BAP(m);
      sp[n_sp].qos = &m->qos_cfg;
      if (m->tx) {
        pair->tx_param = &sp[n_sp];
      } else {
        pair->rx_param = &sp[n_sp];
      }
      n_sp++;
    }
    if (mate) {
      s->partner = mate;
      mate->partner = s;
    }
  }
  esp_ble_audio_bap_unicast_group_param_t param = {
    .params_count = n_pairs,
    .params = pairs,
    .packing = ESP_BLE_ISO_PACKING_SEQUENTIAL,
  };
  return (int)esp_ble_audio_bap_unicast_group_create(&param, &s_uc.group);
}

/* Non-blocking sequencer guard (see s_uc). True when the caller owns it. */
static bool uc_guard_enter(void) {
  return !atomic_exchange(&s_uc.guard, true);
}

static void uc_guard_exit(void) {
  atomic_store(&s_uc.guard, false);
}

/** @brief Record a sequencer mutation made without holding the guard. */
static void uc_bump(void) {
  atomic_fetch_add(&s_uc.gen, 1);
}

/**
 * @brief Issue the next setup step; at most one control-point operation is outstanding.
 *
 * Steps in priority order: Config Codec any requested stream, create the group
 * once all are configured, Config QoS per peer (covers all its streams),
 * Enable each stream, then connect each CIS once both of its streams are
 * enabled. On the CIS-connect step the client does NOT wait for the server's
 * Receiver Start Ready: the CIS may be established any time after QoS
 * Configured (BAP v1.0 ASCS flow; the server's RSR is acked via uc_start_ack
 * and Tx streaming starts on CIS connect). Returns early while an operation
 * is pending. Callers must hold the sequencer guard.
 */
static void uc_advance_pass(void) {
  if (atomic_exchange(&s_uc.stop_requested, false)) {
    uc_stop_locked();
  }
  /* A release that lost the guard race lands here instead of double-emitting. */
  uc_check_released_locked();
  if (!s_uc.active || s_uc.stopping) {
    return;
  }
  for (int i = 0; i < POOL_SIZE; i++) {
    uint8_t st = atomic_load(&s_pool[i].uc_stage);
    if (uc_slot_active(&s_pool[i]) && (st == UC_CONFIG_PENDING || st == UC_QOS_PENDING || st == UC_ENABLE_PENDING || st == UC_CONNECTING)) {
      return;
    }
  }
  for (int i = 0; i < POOL_SIZE; i++) {
    ble_audio_slot_t *s = &s_pool[i];
    if (uc_slot_active(s) && atomic_load(&s->uc_stage) == UC_REQUESTED) {
      atomic_store(&s->uc_stage, UC_CONFIG_PENDING);
      esp_err_t err = esp_ble_audio_bap_stream_config(atomic_load(&s->conn_handle), BAP(s), atomic_load(&s->uc_ep), &s->codec_cfg);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "unicast client: Config Codec on conn %u: %d", atomic_load(&s->conn_handle), err);
        uc_fail(BLE_AUDIO_UC_OP_CONFIG, 0, 0, (int)err);
      }
      return;
    }
  }
  if (s_uc.group == NULL) {
    int err = uc_create_group();
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "unicast client: esp_ble_audio_bap_unicast_group_create: %d", err);
      uc_fail(BLE_AUDIO_UC_OP_GROUP, 0, 0, err);
      return;
    }
  }
  for (int i = 0; i < POOL_SIZE; i++) {
    ble_audio_slot_t *s = &s_pool[i];
    if (uc_slot_active(s) && atomic_load(&s->uc_stage) == UC_CONFIGURED) {
      uint16_t conn = atomic_load(&s->conn_handle);
      for (int j = 0; j < POOL_SIZE; j++) {
        if (uc_slot_active(&s_pool[j]) && atomic_load(&s_pool[j].conn_handle) == conn && atomic_load(&s_pool[j].uc_stage) == UC_CONFIGURED) {
          atomic_store(&s_pool[j].uc_stage, UC_QOS_PENDING);
        }
      }
      esp_err_t err = esp_ble_audio_bap_stream_qos(conn, s_uc.group);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "unicast client: Config QoS on conn %u: %d", conn, err);
        uc_fail(BLE_AUDIO_UC_OP_QOS, 0, 0, (int)err);
      }
      return;
    }
  }
  for (int i = 0; i < POOL_SIZE; i++) {
    ble_audio_slot_t *s = &s_pool[i];
    if (uc_slot_active(s) && atomic_load(&s->uc_stage) == UC_QOS_SET) {
      atomic_store(&s->uc_stage, UC_ENABLE_PENDING);
      esp_err_t err = esp_ble_audio_bap_stream_enable(BAP(s), s->codec_cfg.meta, s->codec_cfg.meta_len);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "unicast client: Enable on conn %u: %d", atomic_load(&s->conn_handle), err);
        uc_fail(BLE_AUDIO_UC_OP_ENABLE, 0, 0, (int)err);
      }
      return;
    }
  }
  for (int i = 0; i < POOL_SIZE; i++) {
    ble_audio_slot_t *s = &s_pool[i];
    if (!uc_slot_active(s) || atomic_load(&s->uc_stage) != UC_ENABLED) {
      continue;
    }
    ble_audio_slot_t *partner = atomic_load(&s->partner);
    if (partner != NULL && atomic_load(&partner->uc_stage) != UC_ENABLED) {
      continue;
    }
    atomic_store(&s->uc_stage, UC_CONNECTING);
    if (partner) {
      atomic_store(&partner->uc_stage, UC_CONNECTING);
    }
    /* One connect per CIS; the paired stream rides on it. One outstanding
     * control-point operation: return after the first connect so a second
     * CIS waits for the next pass (after its stream callback). */
    esp_err_t err = esp_ble_audio_bap_stream_connect(BAP(s));
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "unicast client: CIS connect on conn %u: %d", atomic_load(&s->conn_handle), err);
      uc_fail(BLE_AUDIO_UC_OP_CONNECT, 0, 0, (int)err);
    }
    return;
  }
}

/**
 * @brief Run sequencer passes until no mutation is pending.
 *
 * A pass issues at most a few stack calls and never blocks; concurrent
 * mutations bump gen, so the holder re-runs instead of losing the wake-up.
 * Callers must not hold the guard (uc_fail funnels here on purpose).
 */
static void uc_advance(void) {
  if (!uc_guard_enter()) {
    return;
  }
  uc_advance_locked();
  uc_guard_exit();
}

/** @brief Pass loop for callers that already hold the sequencer guard. */
static void uc_advance_locked(void) {
  uint32_t seen;
  do {
    seen = atomic_load(&s_uc.gen);
    uc_advance_pass();
  } while (atomic_load(&s_uc.gen) != seen);
}

/**
 * @brief Stream op feedback: record the stage @p slot reached and continue the setup.
 *
 * Stages only move forward. UC_STARTED is emitted once, when every stream of
 * the setup is streaming.
 */
static void uc_on_stream_event(ble_audio_slot_t *slot, uint8_t stage) {
  if (!uc_slot_active(slot) || stage <= atomic_load(&slot->uc_stage)) {
    return;
  }
  atomic_store(&slot->uc_stage, stage);
  if (stage == UC_STREAMING) {
    for (int i = 0; i < POOL_SIZE; i++) {
      if (uc_slot_active(&s_pool[i]) && atomic_load(&s_pool[i].uc_stage) != UC_STREAMING) {
        return;
      }
    }
    bool expected = false;
    if (atomic_compare_exchange_strong(&s_uc.started_emitted, &expected, true)) {
      bleAudioEngineEmit(BLE_AUDIO_EVT_UC_STARTED, CONN_NONE, 0, NULL);
    }
    return;
  }
  uc_bump();
  uc_advance();
}

/** @brief Once the last setup stream is released: delete the group and emit UC_STOPPED. Callers must hold the sequencer guard. */
static void uc_check_released_locked(void) {
  if (!s_uc.active) {
    return;
  }
  for (int i = 0; i < POOL_SIZE; i++) {
    if (uc_slot_active(&s_pool[i])) {
      return;
    }
  }
  /* Deleting a CIG while its CIS is still disconnecting leaves the controller
   * pending and the next session fails with PendingIsoExist. Wait for the
   * native disconnect; op_disconnected() calls us again. */
  for (int i = 0; i < POOL_SIZE; i++) {
    const struct bt_iso_chan *iso = BAP(&s_pool[i])->iso;
    if (s_pool[i].in_use && s_pool[i].kind == BLE_AUDIO_STREAM_UNICAST_CLIENT && iso != NULL && iso->state != BT_ISO_STATE_DISCONNECTED) {
      return;
    }
  }
  if (s_uc.group) {
    esp_err_t err = esp_ble_audio_bap_unicast_group_delete(s_uc.group);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "unicast client: group delete failed (%d), dropping the handle anyway", err);
    }
    s_uc.group = NULL;
  }
  s_uc.active = false;
  s_uc.stopping = false;
  bleAudioEngineEmit(BLE_AUDIO_EVT_UC_STOPPED, CONN_NONE, 0, NULL);
}

/**
 * @brief Release check from host callbacks (no guard held).
 *
 * Takes the guard when free; when a pass owns it, bumps gen so the holder's
 * prologue runs the check instead of risking a double group-delete/STOPPED
 * against a concurrent stop.
 */
static void uc_check_released(void) {
  if (!uc_guard_enter()) {
    uc_bump();
    return;
  }
  uc_check_released_locked();
  uc_guard_exit();
}

/*
 * ASCS control-point responses. Success needs no action (the matching stream
 * op advances the setup); a rejection aborts it. Responses to operations sent
 * while stopping, and to Disable/Metadata/Release, are informational only.
 */
static void uc_ack(uint8_t op, esp_ble_audio_bap_ascs_rsp_code_t rsp, esp_ble_audio_bap_ascs_reason_t reason) {
  if (rsp != ESP_BLE_AUDIO_BAP_ASCS_RSP_CODE_SUCCESS && s_uc.active && !s_uc.stopping) {
    uc_fail(op, (uint8_t)rsp, (uint8_t)reason, 0);
  }
}

static void uc_config_ack(esp_ble_audio_bap_stream_t *st, esp_ble_audio_bap_ascs_rsp_code_t rsp, esp_ble_audio_bap_ascs_reason_t reason) {
  (void)st;
  uc_ack(BLE_AUDIO_UC_OP_CONFIG, rsp, reason);
}

static void uc_qos_ack(esp_ble_audio_bap_stream_t *st, esp_ble_audio_bap_ascs_rsp_code_t rsp, esp_ble_audio_bap_ascs_reason_t reason) {
  (void)st;
  uc_ack(BLE_AUDIO_UC_OP_QOS, rsp, reason);
}

static void uc_enable_ack(esp_ble_audio_bap_stream_t *st, esp_ble_audio_bap_ascs_rsp_code_t rsp, esp_ble_audio_bap_ascs_reason_t reason) {
  (void)st;
  uc_ack(BLE_AUDIO_UC_OP_ENABLE, rsp, reason);
}

static void uc_start_ack(esp_ble_audio_bap_stream_t *st, esp_ble_audio_bap_ascs_rsp_code_t rsp, esp_ble_audio_bap_ascs_reason_t reason) {
  (void)st;
  uc_ack(BLE_AUDIO_UC_OP_START, rsp, reason);
}

static void uc_other_ack(esp_ble_audio_bap_stream_t *st, esp_ble_audio_bap_ascs_rsp_code_t rsp, esp_ble_audio_bap_ascs_reason_t reason) {
  (void)st;
  (void)rsp;
  (void)reason;
}

/*
 * Discovery callbacks. The stack reports PAC records, ASEs, locations and
 * contexts one at a time; they are accumulated in the peer entry and emitted
 * together with UC_DISCOVERED once both directions are done.
 */

/** @brief Decode one remote LC3 PAC record and forward it (non-LC3 records are ignored). */
static void uc_pac_record(esp_ble_conn_t *conn, esp_ble_audio_dir_t dir, const esp_ble_audio_codec_cap_t *cap) {
  if (cap == NULL || cap->id != ESP_BLE_ISO_CODING_FORMAT_LC3) {
    return;
  }
  ble_audio_uc_pac_t out = {.dir = (uint8_t)dir, .pac = {.max_frames = 1}};
  const uint8_t *d = cap->data;
  size_t len = cap->data_len;
  while (len >= 2 && d[0] >= 1 && (size_t)d[0] + 1 <= len) {
    const uint8_t l = d[0], type = d[1], *v = &d[2];
    if (type == ESP_BLE_AUDIO_CODEC_CAP_TYPE_FREQ && l == 3) {
      out.pac.freq_mask = audio_get_le16(v);
    } else if (type == ESP_BLE_AUDIO_CODEC_CAP_TYPE_DURATION && l == 2) {
      out.pac.dur_mask = v[0];
    } else if (type == ESP_BLE_AUDIO_CODEC_CAP_TYPE_CHAN_COUNT && l == 2) {
      out.pac.chan_counts = v[0];
    } else if (type == ESP_BLE_AUDIO_CODEC_CAP_TYPE_FRAME_LEN && l == 5) {
      out.pac.min_octets = audio_get_le16(v);
      out.pac.max_octets = audio_get_le16(v + 2);
    } else if (type == ESP_BLE_AUDIO_CODEC_CAP_TYPE_FRAME_COUNT && l == 2) {
      out.pac.max_frames = v[0];
    }
    len -= (size_t)l + 1;
    d += (size_t)l + 1;
  }
  if (out.pac.chan_counts == 0) {
    out.pac.chan_counts = 0x01; /* Absent LTV means mono only. */
  }
  bleAudioEngineEmit(BLE_AUDIO_EVT_UC_PAC, conn->handle, 0, &out);
}

/** @brief Record a remote ASE; ASEs beyond the packaged Kconfig count are ignored. */
static void uc_endpoint(esp_ble_conn_t *conn, esp_ble_audio_dir_t dir, esp_ble_audio_bap_ep_t *ep) {
  portENTER_CRITICAL(&s_uc_peer_mux);
  uc_peer_t *p = uc_peer(conn->handle, false);
  if (p != NULL) {
    if (dir == ESP_BLE_AUDIO_DIR_SINK && p->n_snk < CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SNK_COUNT) {
      p->snk[p->n_snk++] = ep;
    } else if (dir == ESP_BLE_AUDIO_DIR_SOURCE && p->n_src < CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SRC_COUNT) {
      p->src[p->n_src++] = ep;
    }
  }
  portEXIT_CRITICAL(&s_uc_peer_mux);
}

static void uc_location(esp_ble_conn_t *conn, esp_ble_audio_dir_t dir, esp_ble_audio_location_t loc) {
  portENTER_CRITICAL(&s_uc_peer_mux);
  uc_peer_t *p = uc_peer(conn->handle, false);
  if (p) {
    if (dir == ESP_BLE_AUDIO_DIR_SINK) {
      p->info.sink_loc = (uint32_t)loc;
    } else {
      p->info.source_loc = (uint32_t)loc;
    }
  }
  portEXIT_CRITICAL(&s_uc_peer_mux);
}

static void uc_available_contexts(esp_ble_conn_t *conn, esp_ble_audio_context_t snk, esp_ble_audio_context_t src) {
  portENTER_CRITICAL(&s_uc_peer_mux);
  uc_peer_t *p = uc_peer(conn->handle, false);
  if (p) {
    p->info.sink_ctx = (uint16_t)snk;
    p->info.source_ctx = (uint16_t)src;
  }
  portEXIT_CRITICAL(&s_uc_peer_mux);
}

/**
 * @brief One direction finished: after sinks, discover sources; after sources, emit UC_DISCOVERED.
 *
 * The result is 0 when at least one ASE was found in either direction.
 */
static void uc_discover(esp_ble_conn_t *conn, int err, esp_ble_audio_dir_t dir) {
  const uint16_t handle = conn->handle;
  bool discover_source = false;
  portENTER_CRITICAL(&s_uc_peer_mux);
  uc_peer_t *p = uc_peer(handle, false);
  if (p == NULL) {
    portEXIT_CRITICAL(&s_uc_peer_mux);
    return;
  }
  if (dir == ESP_BLE_AUDIO_DIR_SINK) {
    /* A peer without sink ASEs reports an error here; still look for sources. */
    p->sink_err = err;
    discover_source = true;
  }
  portEXIT_CRITICAL(&s_uc_peer_mux);
  /* Engine call outside the peer lock: it can complete inline and re-enter
   * through callbacks (see s_uc_peer_mux). */
  if (discover_source && esp_ble_audio_bap_unicast_client_discover(handle, ESP_BLE_AUDIO_DIR_SOURCE) == ESP_OK) {
    return;
  }
  /* Re-acquire: a disconnect may have released the entry while unlocked. */
  ble_audio_uc_discovered_t info = {0};
  int result = 0;
  int sink_err = 0;
  portENTER_CRITICAL(&s_uc_peer_mux);
  p = uc_peer(handle, false);
  if (p == NULL) {
    portEXIT_CRITICAL(&s_uc_peer_mux);
    return;
  }
  p->info.sink_eps = p->n_snk;
  p->info.source_eps = p->n_src;
  result = (p->n_snk + p->n_src) ? 0 : (err ? err : p->sink_err);
  sink_err = p->sink_err;
  info = p->info;
  portEXIT_CRITICAL(&s_uc_peer_mux);
  if (result != 0) {
    ESP_LOGE(TAG, "unicast client: discovery of conn %u found no ASE (err %d sink_err %d)", handle, err, sink_err);
  }
  /* Emit outside the lock: the C++ dispatch runs synchronously and may call
   * back into the engine. */
  bleAudioEngineEmit(BLE_AUDIO_EVT_UC_DISCOVERED, handle, result, &info);
}

static esp_ble_audio_bap_unicast_client_cb_t s_uc_cb = {
  .location = uc_location,
  .available_contexts = uc_available_contexts,
  .config = uc_config_ack,
  .qos = uc_qos_ack,
  .enable = uc_enable_ack,
  .start = uc_start_ack,
  .stop = uc_other_ack,
  .disable = uc_other_ack,
  .metadata = uc_other_ack,
  .release = uc_other_ack,
  .pac_record = uc_pac_record,
  .endpoint = uc_endpoint,
  .discover = uc_discover,
};

static void uc_reset(void) {
  if (s_uc.group) {
    esp_err_t err = esp_ble_audio_bap_unicast_group_delete(s_uc.group);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "unicast client: group delete on reset failed (%d)", err);
    }
    s_uc.group = NULL;
  }
  /* No memset: active/stopping/started_emitted/guard/gen are _Atomic and may
   * be observed from the host task; store each field explicitly. */
  s_uc.registered = false;
  atomic_store(&s_uc.active, false);
  atomic_store(&s_uc.stopping, false);
  atomic_store(&s_uc.started_emitted, false);
  atomic_store(&s_uc.guard, false);
  atomic_store(&s_uc.gen, 0);
  atomic_store(&s_uc.stop_requested, false);
  portENTER_CRITICAL(&s_uc_peer_mux);
  for (int i = 0; i < UC_MAX_PEERS; i++) {
    memset(&s_uc.peers[i], 0, sizeof(s_uc.peers[i]));
    s_uc.peers[i].conn = CONN_NONE;
  }
  portEXIT_CRITICAL(&s_uc_peer_mux);
}

/* Forget the peer's ASEs; its streams go idle through op_released. */
static void uc_on_disconnect(uint16_t conn) {
  portENTER_CRITICAL(&s_uc_peer_mux);
  uc_peer_t *p = uc_peer(conn, false);
  if (p) {
    p->conn = CONN_NONE;
  }
  portEXIT_CRITICAL(&s_uc_peer_mux);
}

int bleAudioUcInit(void) {
  if (s_uc.registered) {
    return 0;
  }
  uc_reset();
  esp_err_t err = esp_ble_audio_bap_unicast_client_register_cb(&s_uc_cb);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "unicast client: callback registration failed: %d", err);
    return (int)err;
  }
  s_uc.registered = true;
  return 0;
}

/* Rediscovering a known peer starts from a clean entry. */
int bleAudioUcDiscover(uint16_t conn_handle) {
  if (!s_uc.registered) {
    ESP_LOGE(TAG, "unicast client: discover conn %u before init", conn_handle);
    return ESP_ERR_INVALID_STATE;
  }
  portENTER_CRITICAL(&s_uc_peer_mux);
  uc_peer_t *p = uc_peer(conn_handle, true);
  if (p != NULL) {
    memset(p, 0, sizeof(*p));
    p->conn = conn_handle;
  }
  portEXIT_CRITICAL(&s_uc_peer_mux);
  if (p == NULL) {
    ESP_LOGE(TAG, "unicast client: no peer slot for conn %u", conn_handle);
    return ESP_ERR_NO_MEM;
  }
  /* Engine call outside the peer lock (see s_uc_peer_mux): discovery events
   * land through callbacks, some of them inline. */
  int err = (int)esp_ble_audio_bap_unicast_client_discover(conn_handle, ESP_BLE_AUDIO_DIR_SINK);
  if (err != 0) {
    ESP_LOGE(TAG, "unicast client: PACS/ASCS discover conn %u failed: %d", conn_handle, err);
  }
  return err;
}

int bleAudioUcStart(const ble_audio_uc_stream_req_t *reqs, uint8_t count) {
  if (!s_uc.registered || reqs == NULL || count == 0 || count > POOL_UC) {
    return ESP_ERR_INVALID_ARG;
  }
  if (s_uc.active) {
    return ESP_ERR_INVALID_STATE;
  }
  /* Serialize against host-task callbacks (and a concurrent Stop): a second
   * advancer must not interleave validation with a release. */
  if (!uc_guard_enter()) {
    return -EBUSY;
  }
  int rc = 0;
  /* Snapshot every request's endpoint under the peer lock so validation and
   * binding below see one consistent instant: a disconnect landing between
   * the two loops must fail closed, not re-look-up a released entry (the old
   * bind loop dereferenced the re-lookup unchecked). The lock is not held
   * across slot setup or logs (see s_uc_peer_mux). */
  struct {
    bool peer_found;
    esp_ble_audio_bap_ep_t *ep;
  } snap[POOL_UC];
  portENTER_CRITICAL(&s_uc_peer_mux);
  for (uint8_t i = 0; i < count; i++) {
    const ble_audio_uc_stream_req_t *r = &reqs[i];
    snap[i].peer_found = false;
    snap[i].ep = NULL;
    uc_peer_t *p = uc_peer(r->conn_handle, false);
    if (p == NULL) {
      continue;
    }
    snap[i].peer_found = true;
    if (r->dir == BLE_AUDIO_DIR_SINK && r->ep_index < p->n_snk) {
      snap[i].ep = p->snk[r->ep_index];
    } else if (r->dir == BLE_AUDIO_DIR_SOURCE && r->ep_index < p->n_src) {
      snap[i].ep = p->src[r->ep_index];
    }
  }
  portEXIT_CRITICAL(&s_uc_peer_mux);
  /* Validate every request before touching any slot, so a bad entry leaves nothing half set up. */
  for (uint8_t i = 0; i < count; i++) {
    const ble_audio_uc_stream_req_t *r = &reqs[i];
    ble_audio_slot_t *s = r->slot;
    if (s == NULL || !s->in_use || s->kind != BLE_AUDIO_STREAM_UNICAST_CLIENT) {
      ESP_LOGE(TAG, "unicast client: request %u has no client stream", i);
      rc = ESP_ERR_INVALID_ARG;
      break;
    }
    if (!snap[i].peer_found) {
      ESP_LOGE(TAG, "unicast client: request %u targets conn %u, which was not discovered", i, r->conn_handle);
      rc = ESP_ERR_INVALID_ARG;
      break;
    }
    if (snap[i].ep == NULL) {
      ESP_LOGE(TAG, "unicast client: request %u: conn %u has no %s ASE %u", i, r->conn_handle, r->dir == BLE_AUDIO_DIR_SINK ? "sink" : "source", r->ep_index);
      rc = ESP_ERR_INVALID_ARG;
      break;
    }
    if (bleAudioStreamSetCodec(s, &r->codec, r->context) == NULL) {
      rc = ESP_ERR_INVALID_ARG;
      break;
    }
    bleAudioStreamSetQos(s, &r->qos);
  }
  if (rc == 0) {
    for (uint8_t i = 0; i < count; i++) {
      ble_audio_slot_t *s = reqs[i].slot;
      s->uc_ep = snap[i].ep;
      s->partner = NULL;
      /* We send to a remote sink ASE. */
      bleAudioStreamBind(s, reqs[i].conn_handle, reqs[i].dir == BLE_AUDIO_DIR_SINK);
      s->uc_stage = UC_REQUESTED;
    }
    s_uc.active = true;
    s_uc.stopping = false;
    s_uc.started_emitted = false;
    uc_bump();
    uc_advance_locked();
  }
  uc_guard_exit();
  return rc;
}

/*
 * Release every setup stream; UC_STOPPED follows from uc_check_released once
 * the last one is idle (immediately if none was configured on a peer).
 * Callers must hold the sequencer guard.
 */
static void uc_stop_locked(void) {
  if (!s_uc.active) {
    return;
  }
  atomic_store(&s_uc.stopping, true);
  for (int i = 0; i < POOL_SIZE; i++) {
    ble_audio_slot_t *s = &s_pool[i];
    if (!uc_slot_active(s)) {
      continue;
    }
    /* UC_REQUESTED means "listed, nothing sent yet": nothing to release. A
     * UC_CONFIG_PENDING stream already sent Config Codec, so its remote ASE is
     * Codec Configured and must be released, otherwise the peer keeps a
     * dangling configured ASE into the next setup. */
    if (s->uc_stage <= UC_REQUESTED || esp_ble_audio_bap_stream_release(BAP(s)) != ESP_OK) {
      s->uc_stage = UC_IDLE;
      s->partner = NULL;
      portENTER_CRITICAL(&s_slot_mux);
      s->has_codec = false;
      s->has_qos = false;
      portEXIT_CRITICAL(&s_slot_mux);
    }
  }
  uc_check_released_locked();
}

int bleAudioUcStop(void) {
  if (!s_uc.active) {
    return 0;
  }
  if (!uc_guard_enter()) {
    return -EBUSY; /* A pass is in flight; retry (C++ maps to Busy). */
  }
  uc_stop_locked();
  uc_guard_exit();
  return 0;
}

#else /* !BLE_AUDIO_UNICAST_CLIENT_SUPPORTED */

static void uc_on_stream_event(ble_audio_slot_t *slot, uint8_t stage) {
  (void)slot;
  (void)stage;
}
static void uc_check_released(void) {}

#endif /* BLE_AUDIO_UNICAST_CLIENT_SUPPORTED */

#if BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED || BLE_AUDIO_BROADCAST_SINK_SUPPORTED
/* A BIG going down ends every stream on it. The stack reports BIG-level stops
 * without per-stream stopped/released callbacks, so mark the pool here: a
 * C++-owned slot stops streaming, and a zombie (freed C++ handle, see
 * bleAudioStreamFree()) returns to the pool now that nothing references it.
 * Only the slots of the stopped BIG's own kind are touched. Shared with the
 * CAP unit, whose broadcast stop reports the same way. */
void bleAudioBroadcastSlotsDown(ble_audio_stream_kind_t kind) {
  for (int i = 0; i < POOL_SIZE; i++) {
    ble_audio_slot_t *s = &s_pool[i];
    if (!s->in_use || s->kind != (uint8_t)kind) {
      continue;
    }
    atomic_store(&s->streaming, false);
    /* A released endpoint carries no configuration: drop the snapshots so
     * codecConfig()/qos() cannot report a stale codec after the BIG went
     * down (matches op_released for unicast). */
    portENTER_CRITICAL(&s_slot_mux);
    s->has_codec = false;
    s->has_qos = false;
    portEXIT_CRITICAL(&s_slot_mux);
    if (s->owner == NULL) {
      atomic_store(&s->bound, false);
      atomic_store(&s->in_use, false);
    }
  }
}
#endif

/* ── Broadcast source ───────────────────────────────────────────────────── */

#if BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED

/*
 * A single broadcast source (one BIG, one subgroup). The C++ side owns the
 * extended/periodic advertising set; the engine only attaches the BASE to it.
 */
static struct {
  bool cb_registered;
  bool adv_added;           /* The advertising set is registered with the BAP broadcast layer. */
  _Atomic bool big_pending; /* broadcast_source_start succeeded, BIG not yet STREAMING or stopped. */
  uint8_t adv_handle;
  _Atomic(esp_ble_audio_bap_broadcast_source_t *) source; /* NULL when no source exists; set by the caller task, compared by the host task. */
} s_bsrc;

/* BIG created / terminated; per-BIS state arrives through the stream ops. */
static void bsrc_started(esp_ble_audio_bap_broadcast_source_t *source) {
  (void)source;
  bsrc_report_started();
  bleAudioNotifyBsrcState();
}

static void bsrc_stopped(esp_ble_audio_bap_broadcast_source_t *source, uint8_t reason) {
  (void)reason;
  if (source == atomic_load(&s_bsrc.source)) {
    bleAudioBroadcastSlotsDown(BLE_AUDIO_STREAM_BROADCAST_SOURCE);
  }
  atomic_store(&s_bsrc.big_pending, false);
  atomic_store(&s_bsrc_up, false);
  bleAudioNotifyBsrcState();
  bleAudioEngineEmit(BLE_AUDIO_EVT_BSRC_STOPPED, CONN_NONE, reason, NULL);
}

static esp_ble_audio_bap_broadcast_source_cb_t s_bsrc_cb = {
  .started = bsrc_started,
  .stopped = bsrc_stopped,
};

static void bsrc_report_started(void) {
  if (atomic_load(&s_bsrc_up)) {
    return;
  }
  atomic_store(&s_bsrc_up, true);
  bleAudioEngineEmit(BLE_AUDIO_EVT_BSRC_STARTED, CONN_NONE, 0, NULL);
}

int bleAudioBsrcCreate(
  ble_audio_slot_t *const slots[], const uint32_t *locations, uint8_t count, const ble_audio_codec_t *codec, const ble_audio_qos_t *qos, uint16_t context,
  const uint8_t *code
) {
  /*
   * The subgroup codec config and the QoS are stored in slots[0]; every BIS
   * shares them. With per-BIS locations the subgroup carries no channel
   * allocation and each BIS gets its own (e.g. front-left / front-right).
   * Automatic locals: broadcast_source_create() consumes them synchronously,
   * like the unicast group_create() (see uc_create_group()).
   */
  esp_ble_audio_bap_broadcast_source_stream_param_t sp[POOL_BSRC];
  esp_ble_audio_bap_broadcast_source_subgroup_param_t subgroup = {0};
  esp_ble_audio_bap_broadcast_source_param_t param = {0};

  if (atomic_load(&s_bsrc.source) || slots == NULL || count == 0 || count > POOL_BSRC || codec == NULL || qos == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!s_bsrc.cb_registered) {
    esp_err_t err = esp_ble_audio_bap_broadcast_source_register_cb(&s_bsrc_cb);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "broadcast source: callback registration failed: %d", err);
      return (int)err;
    }
    s_bsrc.cb_registered = true;
  }
  ble_audio_codec_t sub = *codec;
  if (locations) {
    sub.chan_alloc = 0; /* per-BIS allocation below */
  }
  esp_ble_audio_codec_cfg_t *cfg = bleAudioStreamSetCodec(slots[0], &sub, context);
  if (cfg == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  for (uint8_t i = 0; i < count; i++) {
    ble_audio_slot_t *s = slots[i];
    if (s == NULL || s->kind != BLE_AUDIO_STREAM_BROADCAST_SOURCE) {
      return ESP_ERR_INVALID_ARG;
    }
    /* Every BIS must carry the subgroup QoS; the lib builds one ISO channel
     * per stream. Leaving slots[1+] at the slot_init zero-fill made stereo
     * Create BIG hang with no complete event (1-BIS phase 5 was fine). */
    (void)bleAudioStreamSetQos(s, qos);
    memset(&sp[i], 0, sizeof(sp[i]));
    sp[i].stream = BAP(s);
    if (locations && locations[i]) {
      s->bis_data[0] = 5;
      s->bis_data[1] = ESP_BLE_AUDIO_CODEC_CFG_CHAN_ALLOC;
      ble_audio_put_le32(&s->bis_data[2], locations[i]);
      sp[i].data = s->bis_data;
      sp[i].data_len = sizeof(s->bis_data);
    }
  }
  subgroup.params_count = count;
  subgroup.params = sp;
  subgroup.codec_cfg = cfg;
  param.params_count = 1;
  param.params = &subgroup;
  param.qos = bleAudioStreamSetQos(slots[0], qos);
  param.packing = ESP_BLE_ISO_PACKING_SEQUENTIAL;
  param.encryption = (code != NULL);
  if (code) {
    memcpy(param.broadcast_code, code, BLE_AUDIO_BCODE_SIZE);
  }
  esp_ble_audio_bap_broadcast_source_t *created = NULL;
  esp_err_t err = esp_ble_audio_bap_broadcast_source_create(&param, &created);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "broadcast source: create (%u BIS, %s) failed: %d", count, code ? "encrypted" : "open", err);
    return (int)err;
  }
  atomic_store(&s_bsrc.source, created);
  return 0;
}

/* Worst-case BASE for one subgroup: 8 B service header + ~24 B subgroup head
 * and metadata + ~40 B per BIS (index, codec ID, LC3 config, QoS). Sized from
 * POOL_BSRC so raising CONFIG_BT_BAP_BROADCAST_SRC_STREAM_COUNT cannot
 * overflow it; 128 B for the Arduino default of 2 BIS. */
#define BSRC_BASE_MAX (32u + 48u * (unsigned)POOL_BSRC)

int bleAudioBsrcGetBase(uint8_t *out, uint16_t cap, uint16_t *out_len) {
  NET_BUF_SIMPLE_DEFINE(base_buf, BSRC_BASE_MAX);
  if (atomic_load(&s_bsrc.source) == NULL || out == NULL || out_len == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  esp_err_t err = esp_ble_audio_bap_broadcast_source_get_base(atomic_load(&s_bsrc.source), &base_buf);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "broadcast source: BASE encoding failed: %d", err);
    return (int)err;
  }
  if (base_buf.len > cap) {
    ESP_LOGE(TAG, "broadcast source: BASE is %u bytes, caller buffer holds %u", base_buf.len, cap);
    return ESP_ERR_NO_MEM;
  }
  memcpy(out, base_buf.data, base_buf.len);
  *out_len = base_buf.len;
  return 0;
}

/* The advertising set is registered once and reused across stop/start cycles. */
int bleAudioBsrcStart(uint8_t adv_handle) {
  if (atomic_load(&s_bsrc.source) == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  atomic_store(&s_bsrc_up, false);
  esp_ble_audio_bap_broadcast_adv_info_t info = {.adv_handle = adv_handle};
  if (!s_bsrc.adv_added) {
    esp_err_t err = esp_ble_audio_bap_broadcast_adv_add(&info);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "broadcast source: registering advertising set %u failed: %d", adv_handle, err);
      return (int)err;
    }
    s_bsrc.adv_added = true;
    s_bsrc.adv_handle = adv_handle;
  }
  esp_err_t err = esp_ble_audio_bap_broadcast_source_start(atomic_load(&s_bsrc.source), adv_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "broadcast source: start on advertising set %u failed: %d", adv_handle, err);
  } else {
    atomic_store(&s_bsrc.big_pending, true);
  }
  return (int)err;
}

/** First in-use broadcast-source endpoint state, or -1 / -2 if none. */
static int bsrc_ep_state(void) {
  for (int i = 0; i < POOL_SIZE; i++) {
    ble_audio_slot_t *s = &s_pool[i];
    if (!s->in_use || s->kind != BLE_AUDIO_STREAM_BROADCAST_SOURCE) {
      continue;
    }
    if (BAP(s)->ep == NULL) {
      return -1;
    }
    esp_ble_audio_bap_ep_info_t info = {0};
    if (esp_ble_audio_bap_ep_get_info(BAP(s)->ep, &info) != ESP_OK) {
      return -1;
    }
    return (int)info.state;
  }
  return -2;
}

int bleAudioBsrcStop(void) {
  return bleAudioBsrcStopTimeout(5000);
}

int bleAudioBsrcStopTimeout(uint32_t timeout_ms) {
  if (atomic_load(&s_bsrc.source) == NULL) {
    atomic_store(&s_bsrc.big_pending, false);
    atomic_store(&s_bsrc_up, false);
    return ESP_ERR_INVALID_STATE;
  }
  /*
   * BSRC_STARTED can fire at QoS Configured (state 2) while Create_BIG is in
   * flight (s_bsrc.big_pending). source_stop() is illegal at QoS Configured
   * and stalls the QoS->STREAMING transition, so wait on BLESync until the
   * endpoint reaches STREAMING or Create_BIG terminates. If start() was never
   * issued (big_pending == false), delete immediately with zero delay. The
   * bound is the caller's share of the stop budget (see BLEAudioStopBudget).
   */
  const TickType_t start_tick = xTaskGetTickCount();
  const TickType_t budget = pdMS_TO_TICKS(timeout_ms);
  int st = bsrc_ep_state();
  while (st != ESP_BLE_AUDIO_BAP_EP_STATE_STREAMING && atomic_load(&s_bsrc.big_pending)) {
    const TickType_t elapsed = xTaskGetTickCount() - start_tick;
    if (elapsed >= budget) {
      break;
    }
    bleAudioArmBsrcStateWait();
    st = bsrc_ep_state();
    if (st == ESP_BLE_AUDIO_BAP_EP_STATE_STREAMING || !atomic_load(&s_bsrc.big_pending)) {
      bleAudioNotifyBsrcState();
      break;
    }
    const uint32_t remain_ms = (uint32_t)((uint64_t)(budget - elapsed) * portTICK_PERIOD_MS);
    if (!bleAudioWaitBsrcState(remain_ms ? remain_ms : 1)) {
      bleAudioNotifyBsrcState();
      break;
    }
    st = bsrc_ep_state();
  }
  atomic_store(&s_bsrc.big_pending, false);
  if (st == ESP_BLE_AUDIO_BAP_EP_STATE_STREAMING) {
    int err = (int)esp_ble_audio_bap_broadcast_source_stop(atomic_load(&s_bsrc.source));
    if (err == 0) {
      atomic_store(&s_bsrc_up, false);
      return 0;
    }
    ESP_LOGW(TAG, "broadcast source: stop failed in STREAMING (err=%d)", err);
    return err;
  }
  if (st >= 0 && st <= ESP_BLE_AUDIO_BAP_EP_STATE_QOS_CONFIGURED) {
    bleAudioBsrcDelete();
    if (atomic_load(&s_bsrc.source) == NULL) {
      atomic_store(&s_bsrc_up, false);
      bleAudioEngineEmit(BLE_AUDIO_EVT_BSRC_STOPPED, CONN_NONE, 0, NULL);
      return 0;
    }
  }
  ESP_LOGW(TAG, "broadcast source: stop failed (ep=%d)", st);
  return ESP_FAIL;
}

/* Replaces the subgroup metadata; the stack refreshes the BASE in the periodic advertising. */
int bleAudioBsrcUpdateContext(uint16_t context) {
  if (atomic_load(&s_bsrc.source) == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  uint8_t meta[META_CTX_LEN];
  size_t len = build_ctx_meta(meta, ESP_BLE_AUDIO_METADATA_TYPE_STREAM_CONTEXT, context);
  return (int)esp_ble_audio_bap_broadcast_source_update_metadata(atomic_load(&s_bsrc.source), meta, len);
}

/* Deletion is refused while the BIG is up; the source and its advertising set are then kept. */
void bleAudioBsrcDelete(void) {
  esp_ble_audio_bap_broadcast_source_t *gone = atomic_load(&s_bsrc.source);
  if (gone) {
    esp_err_t err = esp_ble_audio_bap_broadcast_source_delete(gone);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "broadcast source: delete refused (%d), stop the broadcast first", err);
      return;
    }
    atomic_store(&s_bsrc.source, NULL);
    atomic_store(&s_bsrc.big_pending, false);
  }
  if (s_bsrc.adv_added) {
    esp_ble_audio_bap_broadcast_adv_info_t info = {.adv_handle = s_bsrc.adv_handle};
    (void)esp_ble_audio_bap_broadcast_adv_delete(&info);
    s_bsrc.adv_added = false;
  }
}

void *bleAudioBsrcHandle(void) {
  return atomic_load(&s_bsrc.source);
}

#endif /* BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED */

/* ── Broadcast sink + scan delegator ────────────────────────────────────── */

#if BLE_AUDIO_BROADCAST_SINK_SUPPORTED

/*
 * Sync flow: the C++ side scans and creates the PA sync itself (or receives
 * it by PAST); an armed sink claims the next PA sync and creates the BAP
 * broadcast sink on it. The BASE then describes the BISes, BIGInfo reports
 * the BIG is syncable, and bsnk_syncable picks the BISes and syncs.
 *
 * With the scan delegator, a Broadcast Assistant drives the same flow over
 * BASS: its PA sync request arms the sink, its BIS sync request selects BISes
 * (0 = leave the BIG), and it may supply the Broadcast Code.
 */

#define AD_TYPE_NAME_COMPLETE  0x09
#define AD_TYPE_SERVICE_DATA16 0x16
#define AD_TYPE_BROADCAST_NAME 0x30

static struct {
  bool enabled; /* bleAudioBsinkInit() ran; GAP events are routed here. */
  bool cb_registered;
  bool delegator; /* Scan delegator (BASS) registered. */
  /* Cross-task state (caller-task API vs. host-task stack callbacks): plain
   * readers/writers on different cores are a data race, so the claim-path
   * flags, handles, IDs and BIS-selection inputs are _Atomic (same
   * convention as the stream slots and s_uc.group). Still plain: init-once
   * publishes (enabled, cb_registered, delegator), host-confined latches
   * (base_received, failed_no_match, fail_mask, base_bis), and the slot/code
   * tables, which are staged at apply time strictly before any host event
   * can reference them and cleared by deinit while the host is down. */
  _Atomic bool scanning; /* Report Broadcast Audio announcements (BSINK_FOUND). */
  _Atomic bool armed;    /* The next PA sync belongs to the sink. */
  _Atomic bool hold;     /* Stopped on request: do not resync until re-armed. */
  bool base_received;    /* BASE decoded for the current sink; later copies are ignored. */
  _Atomic bool syncing;  /* BIG sync requested or established. */
  _Atomic bool has_code;
  bool failed_no_match;          /* BSINK_SYNC_FAILED(-ENOENT) latched for these inputs. */
  uint32_t fail_mask;            /* BIS-selection inputs the latch was computed from. */
  _Atomic bool failed_no_code;   /* BSINK_SYNC_FAILED(-EACCES) latched until a code arrives. */
  _Atomic uint16_t sync_handle;  /* PA sync in use, SYNC_NONE when not synced. */
  _Atomic uint32_t broadcast_id; /* 24-bit Broadcast_ID of the target. */
  uint32_t base_bis;             /* BIS indexes (bitmask, bit 0 = BIS 1) offered by the BASE. */
  _Atomic uint32_t pref_mask;    /* Local BIS preference, 0 = any. */
  _Atomic uint32_t requested;    /* Assistant BIS request, BIS_SYNC_NO_PREF = any. */
  _Atomic uint8_t n_slots;
  ble_audio_slot_t *slots[POOL_BSNK]; /* Streams the chosen BISes are bound to, in order. */
  uint8_t code[BLE_AUDIO_BCODE_SIZE];
  _Atomic(esp_ble_audio_bap_broadcast_sink_t *) sink; /* Claimed PA sync's sink; host task creates/deletes, C++ stops. */
} s_bsnk;

/**
 * @brief (Re)create the BAP broadcast sink on the current PA sync.
 *
 * Any previous sink is deleted first so BASE/BIGInfo reporting restarts from
 * scratch. A failure is reported as BSINK_SYNC_FAILED.
 */
static void bsnk_create(void) {
  if (atomic_load(&s_bsnk.sink)) {
    (void)esp_ble_audio_bap_broadcast_sink_delete(atomic_load(&s_bsnk.sink));
    atomic_store(&s_bsnk.sink, NULL);
  }
  s_bsnk.base_received = false;
  atomic_store(&s_bsnk.syncing, false);
  s_bsnk.failed_no_match = false;
  atomic_store(&s_bsnk.failed_no_code, false);
  esp_ble_audio_bap_broadcast_sink_t *sink = NULL;
  esp_err_t err = esp_ble_audio_bap_broadcast_sink_create(atomic_load(&s_bsnk.sync_handle), atomic_load(&s_bsnk.broadcast_id), &sink);
  atomic_store(&s_bsnk.sink, sink);
  if (err != ESP_OK) {
    ESP_LOGE(
      TAG, "broadcast sink: create on PA sync 0x%04x (broadcast 0x%06lx) failed: %d", atomic_load(&s_bsnk.sync_handle),
      (unsigned long)atomic_load(&s_bsnk.broadcast_id), err
    );
    bleAudioEngineEmit(BLE_AUDIO_EVT_BSINK_SYNC_FAILED, CONN_NONE, (int)err, NULL);
  }
}

typedef struct {
  ble_audio_bsink_found_t *out;
  bool has_id; /* A Broadcast Audio Announcement was present. */
} scan_parse_t;

/**
 * @brief Advertising data walker: collect the name, Broadcast_ID and PBA features.
 *
 * The Broadcast Name wins over the Complete Local Name. Always returns true
 * to keep walking.
 */
static bool bsnk_ad_cb(uint8_t type, const uint8_t *data, uint8_t len, void *user) {
  scan_parse_t *sp = (scan_parse_t *)user;
  if ((type == AD_TYPE_BROADCAST_NAME || (type == AD_TYPE_NAME_COMPLETE && sp->out->name[0] == '\0')) && len > 0) {
    size_t n = len > BLE_AUDIO_BCAST_NAME_MAX ? BLE_AUDIO_BCAST_NAME_MAX : len;
    memcpy(sp->out->name, data, n);
    sp->out->name[n] = '\0';
  } else if (type == AD_TYPE_SERVICE_DATA16 && len >= 2) {
    uint16_t uuid = audio_get_le16(data);
    if (uuid == ESP_BLE_AUDIO_UUID_BROADCAST_AUDIO_VAL && len >= 5) {
      sp->out->broadcast_id = audio_get_le24(data + 2);
      sp->has_id = true;
    } else if (uuid == ESP_BLE_AUDIO_UUID_PBA_VAL && len >= 3) {
      sp->out->pba_features = data[2];
      sp->out->has_pba = true;
    }
  }
  return true;
}

/* Only extended advertisers with periodic advertising can be Broadcast Sources. */
static void bsnk_on_scan(const esp_ble_audio_gap_app_event_t *ev) {
  if (!atomic_load(&s_bsnk.scanning) || ev->ext_scan_recv.per_adv_itvl == 0 || ev->ext_scan_recv.data == NULL) {
    return;
  }
  ble_audio_bsink_found_t found = {0};
  scan_parse_t sp = {.out = &found};
  (void)esp_ble_audio_data_parse(ev->ext_scan_recv.data, ev->ext_scan_recv.data_len, bsnk_ad_cb, &sp);
  if (!sp.has_id) {
    return;
  }
  found.addr_type = ev->ext_scan_recv.addr.type;
  bleAudioAddrFromGap(found.addr, ev->ext_scan_recv.addr.val);
  found.sid = ev->ext_scan_recv.sid;
  found.rssi = ev->ext_scan_recv.rssi;
  bleAudioEngineEmit(BLE_AUDIO_EVT_BSINK_FOUND, CONN_NONE, 0, &found);
}

/**
 * @brief PA sync established (scan or PAST): claim it if armed and create the sink.
 *
 * @param conn ACL that sent the PAST, CONN_NONE for a locally created sync.
 */
static void bsnk_on_pa_sync(uint8_t status, uint16_t sync_handle, uint16_t conn) {
  if (!atomic_load(&s_bsnk.armed)) {
    return;
  }
  atomic_store(&s_bsnk.armed, false);
  if (status) {
    bleAudioEngineEmit(BLE_AUDIO_EVT_BSINK_PA_SYNCED, conn, status, NULL);
    return;
  }
  atomic_store(&s_bsnk.sync_handle, sync_handle);
  bleAudioEngineEmit(BLE_AUDIO_EVT_BSINK_PA_SYNCED, conn, 0, NULL);
  bsnk_create();
}

/** @brief BASE subgroup walker: decode the first subgroup's codec into @p user, then stop. */
static bool bsnk_first_subgroup(const esp_ble_audio_bap_base_subgroup_t *subgroup, void *user) {
  /* Backing buffers cover both a copying and a pointer-assigning implementation. */
  uint8_t data[64], meta[64];
  esp_ble_audio_codec_cfg_t cfg = {.data = data, .meta = meta};
  if (esp_ble_audio_bap_base_subgroup_codec_to_codec_cfg(subgroup, &cfg) == ESP_OK) {
    (void)parse_cfg(&cfg, (ble_audio_codec_t *)user);
  }
  return false;
}

/*
 * The BASE repeats in every periodic advertising event; only the first copy
 * per sink is decoded and reported (BSINK_BASE). A BASE without BISes is
 * skipped and the next copy is tried.
 */
static void bsnk_base_recv(esp_ble_audio_bap_broadcast_sink_t *sink, const esp_ble_audio_bap_base_t *base, size_t base_size) {
  (void)sink;
  (void)base_size;
  if (s_bsnk.base_received) {
    return;
  }
  ble_audio_bsink_base_t info = {0};
  if (esp_ble_audio_bap_base_get_bis_indexes(base, &info.bis_mask) != ESP_OK || info.bis_mask == 0) {
    return;
  }
  (void)esp_ble_audio_bap_base_get_subgroup_count(base, &info.subgroups);
  (void)esp_ble_audio_bap_base_get_pres_delay(base, &info.pd_us);
  (void)esp_ble_audio_bap_base_foreach_subgroup(base, bsnk_first_subgroup, &info.codec);
  s_bsnk.base_bis = info.bis_mask;
  s_bsnk.base_received = true;
  bleAudioEngineEmit(BLE_AUDIO_EVT_BSINK_BASE, CONN_NONE, 0, &info);
}

/** @brief Keep the @p n lowest set bits of @p mask. */
static uint32_t lowest_bits(uint32_t mask, uint8_t n) {
  uint32_t out = 0;
  while (mask && n--) {
    uint32_t bit = mask & (~mask + 1U);
    out |= bit;
    mask &= ~bit;
  }
  return out;
}

/**
 * @brief BIGInfo received: pick the BISes and sync to the BIG.
 *
 * The BIS set is BASE ∩ assistant request ∩ local preference, trimmed to the
 * lowest indexes that fit the bound streams. Reports BSINK_SYNC_FAILED with
 * -ENOENT when nothing matches and -EACCES when the BIG is encrypted but no
 * Broadcast Code is known. BIGInfo repeats, so a failed attempt is retried
 * on the next one once the cause is fixed.
 */
static void bsnk_syncable(esp_ble_audio_bap_broadcast_sink_t *sink, const esp_ble_iso_biginfo_t *biginfo) {
  if (atomic_load(&s_bsnk.syncing) || atomic_load(&s_bsnk.hold) || !s_bsnk.base_received || atomic_load(&s_bsnk.n_slots) == 0) {
    return;
  }
  uint32_t mask = s_bsnk.base_bis & atomic_load(&s_bsnk.requested);
  if (atomic_load(&s_bsnk.pref_mask)) {
    mask &= atomic_load(&s_bsnk.pref_mask);
  }
  uint32_t inputs = mask; /* The selector inputs, before the slot-count trim. */
  mask = lowest_bits(mask, atomic_load(&s_bsnk.n_slots));
  if (mask == 0) {
    /* BIGInfo repeats periodically: only report an empty intersection again
     * when the inputs changed (assistant request, BASE or local preference),
     * otherwise a permanently empty match spams the failure callback. */
    if (s_bsnk.failed_no_match && s_bsnk.fail_mask == inputs) {
      return;
    }
    s_bsnk.failed_no_match = true;
    atomic_store(&s_bsnk.failed_no_code, false);
    s_bsnk.fail_mask = inputs;
    bleAudioEngineEmit(BLE_AUDIO_EVT_BSINK_SYNC_FAILED, CONN_NONE, -ENOENT, NULL);
    return;
  }
  s_bsnk.failed_no_match = false;
  if (biginfo && biginfo->encryption && !atomic_load(&s_bsnk.has_code)) {
    if (atomic_load(&s_bsnk.failed_no_code)) {
      return; /* Cleared by bleAudioBsinkSetCode(). */
    }
    atomic_store(&s_bsnk.failed_no_code, true);
    bleAudioEngineEmit(BLE_AUDIO_EVT_BSINK_SYNC_FAILED, CONN_NONE, -EACCES, NULL);
    return;
  }
  atomic_store(&s_bsnk.failed_no_code, false);
  /* mask holds at most n_slots bits (trimmed above); the stack consumes one
   * stream per set bit, so entries past popcount(mask) are never read. */
  esp_ble_audio_bap_stream_t *streams[POOL_BSNK];
  for (uint8_t i = 0; i < atomic_load(&s_bsnk.n_slots); i++) {
    streams[i] = BAP(s_bsnk.slots[i]);
  }
  esp_err_t err = esp_ble_audio_bap_broadcast_sink_sync(sink, mask, streams, s_bsnk.code);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "broadcast sink: BIG sync to BIS mask 0x%08lx failed: %d", (unsigned long)mask, err);
    bleAudioEngineEmit(BLE_AUDIO_EVT_BSINK_SYNC_FAILED, CONN_NONE, (int)err, NULL);
    return;
  }
  atomic_store(&s_bsnk.syncing, true);
}

static void bsnk_started(esp_ble_audio_bap_broadcast_sink_t *sink) {
  (void)sink;
  if (atomic_load(&s_bsnk_joined)) {
    return;
  }
  atomic_store(&s_bsnk_joined, true);
  bleAudioEngineEmit(BLE_AUDIO_EVT_BSINK_STARTED, CONN_NONE, 0, NULL);
}

static void bsnk_stopped(esp_ble_audio_bap_broadcast_sink_t *sink, uint8_t reason) {
  if (sink == atomic_load(&s_bsnk.sink)) {
    bleAudioBroadcastSlotsDown(BLE_AUDIO_STREAM_BROADCAST_SINK);
  }
  atomic_store(&s_bsnk.syncing, false);
  atomic_store(&s_bsnk_joined, false);
  bleAudioEngineEmit(BLE_AUDIO_EVT_BSINK_STOPPED, CONN_NONE, reason, NULL);
  /* Delete here, after BASS bis_sync was cleared, then keep a fresh sink on
   * the PA so BASE/BIGInfo keep arriving for a later resync. */
  esp_ble_audio_bap_broadcast_sink_t *stopped = atomic_load(&s_bsnk.sink);
  if (stopped) {
    (void)esp_ble_audio_bap_broadcast_sink_delete(stopped);
    atomic_store(&s_bsnk.sink, NULL);
  }
  if (atomic_load(&s_bsnk.sync_handle) != SYNC_NONE) {
    bsnk_create();
  }
}

static esp_ble_audio_bap_broadcast_sink_cb_t s_bsnk_cb = {
  .base_recv = bsnk_base_recv,
  .syncable = bsnk_syncable,
  .started = bsnk_started,
  .stopped = bsnk_stopped,
};

#if BLE_AUDIO_SCAN_DELEGATOR_SUPPORTED

/*
 * BASS scan delegator callbacks: requests from a Broadcast Assistant. The
 * return value is the BASS response (0 accepts, negative errno rejects).
 */

/**
 * @brief Assistant asks us to sync to a source's periodic advertising.
 *
 * The C++ side decides (and performs the sync or waits for PAST) through the
 * reply pointer in BSINK_PA_REQ; the event is dispatched synchronously so the
 * reply is set on return. Rejected while a PA sync is already in use.
 */
static int sd_pa_sync_req(esp_ble_conn_t *conn, const esp_ble_audio_bap_scan_delegator_recv_state_t *rs, bool past, uint16_t pa_interval) {
  if (atomic_load(&s_bsnk.sync_handle) != SYNC_NONE || atomic_load(&s_bsnk.armed)) {
    return -EALREADY;
  }
  int reply = -ENOTSUP;
  ble_audio_bsink_pa_req_t req = {
    .addr_type = rs->addr.type,
    .peer_addr_type = conn ? conn->le.dst.type : 0,
    .sid = rs->adv_sid,
    .broadcast_id = rs->broadcast_id,
    .past = past,
    .pa_interval = pa_interval,
    .reply = &reply,
  };
  memcpy(req.addr, rs->addr.a.val, 6);
  if (conn) {
    bleAudioAddrFromGap(req.peer_addr, conn->le.dst.a.val);
  }
  bleAudioEngineEmit(BLE_AUDIO_EVT_BSINK_PA_REQ, conn ? conn->handle : CONN_NONE, 0, &req);
  if (reply != 0) {
    return reply;
  }
  atomic_store(&s_bsnk.broadcast_id, rs->broadcast_id);
  atomic_store(&s_bsnk.armed, true);
  atomic_store(&s_bsnk.hold, false);
  if (past) {
    /* Tell the assistant we are waiting for its PAST. */
    esp_err_t err = esp_ble_audio_bap_scan_delegator_set_pa_state(rs->src_id, ESP_BLE_AUDIO_BAP_PA_STATE_INFO_REQ);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "scan delegator: setting source %u to SyncInfo Request failed: %d", rs->src_id, err);
    }
  }
  return 0;
}

/* Assistant asks us to drop the PA sync; the C++ side terminates/disarms it (BSINK_PA_TERM_REQ). */
static int sd_pa_sync_term_req(esp_ble_conn_t *conn, const esp_ble_audio_bap_scan_delegator_recv_state_t *rs) {
  (void)rs;
  const bool was_armed = atomic_load(&s_bsnk.armed);
  atomic_store(&s_bsnk.armed, false);
  if (atomic_load(&s_bsnk.sync_handle) == SYNC_NONE && !was_armed) {
    return 0;
  }
  bleAudioEngineEmit(BLE_AUDIO_EVT_BSINK_PA_TERM_REQ, conn ? conn->handle : CONN_NONE, 0, NULL);
  return 0;
}

/* Assistant supplied the Broadcast Code; it is used from the next BIG sync attempt. */
static void
  sd_broadcast_code(esp_ble_conn_t *conn, const esp_ble_audio_bap_scan_delegator_recv_state_t *rs, const uint8_t code[ESP_BLE_ISO_BROADCAST_CODE_SIZE]) {
  (void)rs;
  memcpy(s_bsnk.code, code, BLE_AUDIO_BCODE_SIZE);
  atomic_store(&s_bsnk.has_code, true);
  bleAudioEngineEmit(BLE_AUDIO_EVT_BSINK_CODE, conn ? conn->handle : CONN_NONE, 0, NULL);
}

/**
 * @brief Assistant selects BISes per subgroup.
 *
 * Only one subgroup can be synced (the sink syncs a single BIS set), so a
 * request naming specific BISes in two subgroups is rejected. An all-zero
 * request means "leave the BIG": the sink stops and holds until re-armed.
 */
static int sd_bis_sync_req(
  esp_ble_conn_t *conn, const esp_ble_audio_bap_scan_delegator_recv_state_t *rs, const uint32_t bis_sync_req[CONFIG_BT_BAP_BASS_MAX_SUBGROUPS]
) {
  (void)conn;
  uint32_t req = 0;
  for (uint8_t i = 0; i < rs->num_subgroups && i < CONFIG_BT_BAP_BASS_MAX_SUBGROUPS; i++) {
    if (bis_sync_req[i] == 0) {
      continue;
    }
    if (req != 0 && req != ESP_BLE_AUDIO_BAP_BIS_SYNC_NO_PREF && bis_sync_req[i] != ESP_BLE_AUDIO_BAP_BIS_SYNC_NO_PREF) {
      return -EINVAL; /* one subgroup at a time */
    }
    req = bis_sync_req[i];
  }
  if (req == 0) {
    atomic_store(&s_bsnk.hold, true);
    if (atomic_load(&s_bsnk.sink) && atomic_load(&s_bsnk.syncing)) {
      (void)esp_ble_audio_bap_broadcast_sink_stop(atomic_load(&s_bsnk.sink));
    }
  } else {
    atomic_store(&s_bsnk.requested, req);
    atomic_store(&s_bsnk.hold, false);
  }
  return 0;
}

static esp_ble_audio_bap_scan_delegator_cb_t s_sd_cb = {
  .pa_sync_req = sd_pa_sync_req,
  .pa_sync_term_req = sd_pa_sync_term_req,
  .broadcast_code = sd_broadcast_code,
  .bis_sync_req = sd_bis_sync_req,
};

#endif /* BLE_AUDIO_SCAN_DELEGATOR_SUPPORTED */

/*
 * GAP events for the sink. On PA sync loss the sink is deleted unless a BIG
 * is still synced; in that case bsnk_stopped deletes it when the BIG ends,
 * and does not recreate it because the sync handle is gone.
 */
static void bsnk_on_gap(const esp_ble_audio_gap_app_event_t *ev) {
  switch (ev->type) {
    case ESP_BLE_AUDIO_GAP_EVENT_EXT_SCAN_RECV: bsnk_on_scan(ev); break;
    case ESP_BLE_AUDIO_GAP_EVENT_PA_SYNC:       bsnk_on_pa_sync(ev->pa_sync.status, ev->pa_sync.sync_handle, CONN_NONE); break;
    case ESP_BLE_AUDIO_GAP_EVENT_PA_SYNC_PAST:  bsnk_on_pa_sync(ev->pa_sync_past.status, ev->pa_sync_past.sync_handle, ev->pa_sync_past.conn_handle); break;
    case ESP_BLE_AUDIO_GAP_EVENT_PA_SYNC_LOST:
      if (ev->pa_sync_lost.sync_handle == atomic_load(&s_bsnk.sync_handle)) {
        atomic_store(&s_bsnk.sync_handle, SYNC_NONE);
        s_bsnk.base_received = false;
        esp_ble_audio_bap_broadcast_sink_t *lost = atomic_load(&s_bsnk.sink);
        if (lost && !atomic_load(&s_bsnk.syncing)) {
          (void)esp_ble_audio_bap_broadcast_sink_delete(lost);
          atomic_store(&s_bsnk.sink, NULL);
        }
        bleAudioEngineEmit(BLE_AUDIO_EVT_BSINK_PA_LOST, CONN_NONE, ev->pa_sync_lost.reason, NULL);
      }
      break;
    default: break;
  }
}

int bleAudioBsinkInit(bool delegator) {
  if (!s_bsnk.cb_registered) {
    esp_err_t err = esp_ble_audio_bap_broadcast_sink_register_cb(&s_bsnk_cb);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "broadcast sink: callback registration failed: %d", err);
      return (int)err;
    }
    s_bsnk.cb_registered = true;
  }
#if BLE_AUDIO_SCAN_DELEGATOR_SUPPORTED
  if (delegator && !s_bsnk.delegator) {
    esp_err_t err = esp_ble_audio_bap_scan_delegator_register(&s_sd_cb);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "scan delegator: BASS registration failed: %d", err);
      return (int)err;
    }
    s_bsnk.delegator = true;
  }
#else
  if (delegator) {
    return ESP_ERR_NOT_SUPPORTED;
  }
#endif
  s_bsnk.enabled = true;
  atomic_store(&s_bsnk.sync_handle, SYNC_NONE);
  atomic_store(&s_bsnk.requested, ESP_BLE_AUDIO_BAP_BIS_SYNC_NO_PREF);
  return 0;
}

int bleAudioBsinkSetStreams(ble_audio_slot_t *const slots[], uint8_t count) {
  if (count > POOL_BSNK) {
    return ESP_ERR_INVALID_ARG;
  }
  for (uint8_t i = 0; i < count; i++) {
    if (slots[i] == NULL || slots[i]->kind != BLE_AUDIO_STREAM_BROADCAST_SINK) {
      return ESP_ERR_INVALID_ARG;
    }
    s_bsnk.slots[i] = slots[i];
    /* Mark assigned while the delegator/sink may reference the slot; the
     * free path zombies while bound so a later BIG sync cannot use a slot
     * whose C++ handle is gone (reclaimed on BIG teardown / deinit). */
    atomic_store(&slots[i]->bound, true);
  }
  atomic_store(&s_bsnk.n_slots, count);
  return 0;
}

void bleAudioBsinkSetScanning(bool on) {
  atomic_store(&s_bsnk.scanning, on);
}

/* Called by C++ right before it creates a PA sync, so bsnk_on_pa_sync claims it. */
void bleAudioBsinkArm(uint32_t broadcast_id) {
  /* Paired stores, same task; the host side reads armed first and only trusts broadcast_id when armed. */
  atomic_store(&s_bsnk.broadcast_id, broadcast_id & 0xFFFFFF);
  atomic_store(&s_bsnk.armed, true);
  atomic_store(&s_bsnk.hold, false);
}

void bleAudioBsinkDisarm(void) {
  atomic_store(&s_bsnk.armed, false);
}

void bleAudioBsinkSetCode(const uint8_t *code) {
  /* Publish the bytes before the flag: readers that observe has_code set see
   * a complete code. */
  memset(s_bsnk.code, 0, sizeof(s_bsnk.code));
  if (code) {
    memcpy(s_bsnk.code, code, BLE_AUDIO_BCODE_SIZE);
  }
  atomic_store(&s_bsnk.has_code, code != NULL);
  /* A new Broadcast Code unblocks the -EACCES latch in bsnk_syncable(). */
  atomic_store(&s_bsnk.failed_no_code, false);
}

void bleAudioBsinkSetBisMask(uint32_t mask) {
  atomic_store(&s_bsnk.pref_mask, mask);
}

/* Leaves the BIG but keeps the PA sync; hold blocks the automatic resync on the next BIGInfo. */
int bleAudioBsinkStop(void) {
  atomic_store(&s_bsnk.armed, false);
  if (atomic_load(&s_bsnk.sink) && atomic_load(&s_bsnk.syncing)) {
    atomic_store(&s_bsnk.hold, true);
    return (int)esp_ble_audio_bap_broadcast_sink_stop(atomic_load(&s_bsnk.sink));
  }
  return 0;
}

uint16_t bleAudioBsinkSyncHandle(void) {
  return atomic_load(&s_bsnk.sync_handle);
}

#endif /* BLE_AUDIO_BROADCAST_SINK_SUPPORTED */

/* ── Unit hooks ─────────────────────────────────────────────────────────── */

/* GAP fan-out from BLEAudioEngine.c; the payload is an esp_ble_audio_gap_app_event_t. */
static void bap_on_gap(const void *event) {
  const esp_ble_audio_gap_app_event_t *ev = (const esp_ble_audio_gap_app_event_t *)event;
#if BLE_AUDIO_BROADCAST_SINK_SUPPORTED
  if (s_bsnk.enabled) {
    bsnk_on_gap(ev);
  }
#endif
#if BLE_AUDIO_UNICAST_CLIENT_SUPPORTED
  if (ev->type == ESP_BLE_AUDIO_GAP_EVENT_ACL_DISCONNECT) {
    uc_on_disconnect(ev->acl_disconnect.conn_handle);
  }
#else
  (void)ev;
#endif
}

/* Engine deinit: forget every registration so the next start re-registers from scratch. */
static void bap_on_deinit(void) {
  /* C++ stream handles may outlive the session: keep their slots owned
   * (in_use, kind, owner) so a later bleAudioStreamFree() cannot release a
   * slot another owner took. The host is already down here (common_deinit ran
   * first), so this must not call back into the stack: slot_init()'s ops
   * registration would run without the host mutex. A successful end() waits
   * for ISO idle before this runs; roles.clear() then frees the slots through
   * the C++ destructors. */
  for (int i = 0; i < POOL_SIZE; i++) {
    ble_audio_slot_t *s = &s_pool[i];
    if (s->in_use) {
      slot_reset_nostack(s);
    }
  }
  memset(s_pac, 0, sizeof(s_pac));
  s_pacs_committed = false;
#if BLE_AUDIO_UNICAST_SERVER_SUPPORTED
  s_us_registered = false;
#endif
#if BLE_AUDIO_UNICAST_CLIENT_SUPPORTED
  uc_reset();
#endif
#if BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED
  /* Host mutex is already gone (common_deinit). The lib source is released
   * by lib_bap_broadcast_source_deinit(); a native delete here would only
   * return ESP_ERR_TIMEOUT. */
  atomic_store(&s_bsrc_up, false);
  s_bsrc.cb_registered = false;
  s_bsrc.adv_added = false;
  atomic_store(&s_bsrc.big_pending, false);
  s_bsrc.adv_handle = 0;
  atomic_store(&s_bsrc.source, NULL);
#endif
#if BLE_AUDIO_BROADCAST_SINK_SUPPORTED
  atomic_store(&s_bsnk_joined, false);
  s_bsnk.enabled = false;
  s_bsnk.cb_registered = false;
  s_bsnk.delegator = false;
  atomic_store(&s_bsnk.scanning, false);
  atomic_store(&s_bsnk.armed, false);
  atomic_store(&s_bsnk.hold, false);
  s_bsnk.base_received = false;
  atomic_store(&s_bsnk.syncing, false);
  atomic_store(&s_bsnk.has_code, false);
  s_bsnk.failed_no_match = false;
  s_bsnk.fail_mask = 0;
  atomic_store(&s_bsnk.failed_no_code, false);
  atomic_store(&s_bsnk.sync_handle, SYNC_NONE);
  atomic_store(&s_bsnk.broadcast_id, 0);
  s_bsnk.base_bis = 0;
  atomic_store(&s_bsnk.pref_mask, 0);
  atomic_store(&s_bsnk.requested, 0);
  atomic_store(&s_bsnk.n_slots, 0);
  memset(s_bsnk.slots, 0, sizeof(s_bsnk.slots));
  memset(s_bsnk.code, 0, sizeof(s_bsnk.code));
  atomic_store(&s_bsnk.sink, NULL);
  for (int i = 0; i < POOL_SIZE; i++) {
    ble_audio_slot_t *s = &s_pool[i];
    if (s->in_use && s->kind == BLE_AUDIO_STREAM_BROADCAST_SINK) {
      atomic_store(&s->bound, false);
    }
  }
#endif
}

static const ble_audio_unit_hooks_t s_hooks = {.on_gap = bap_on_gap, .on_deinit = bap_on_deinit};

/* Called from BLEAudio::begin(); registering the same hooks again is harmless. */
int bleAudioBapAttach(void) {
  return bleAudioEngineRegisterUnit(&s_hooks);
}

/* ==========================================================================
 * Feature-disabled stubs
 * ========================================================================== */

#if !BLE_AUDIO_UNICAST_SERVER_SUPPORTED
int bleAudioUsInit(uint8_t snk_cnt, uint8_t src_cnt, const ble_audio_qos_pref_t *pref) {
  (void)snk_cnt;
  (void)src_cnt;
  (void)pref;
  return ESP_ERR_NOT_SUPPORTED;
}
#endif /* !BLE_AUDIO_UNICAST_SERVER_SUPPORTED */

#if !BLE_AUDIO_UNICAST_CLIENT_SUPPORTED
int bleAudioUcInit(void) {
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioUcDiscover(uint16_t conn_handle) {
  (void)conn_handle;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioUcStart(const ble_audio_uc_stream_req_t *reqs, uint8_t count) {
  (void)reqs;
  (void)count;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioUcStop(void) {
  return ESP_ERR_NOT_SUPPORTED;
}
#endif /* !BLE_AUDIO_UNICAST_CLIENT_SUPPORTED */

#if !BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED
int bleAudioBsrcCreate(
  ble_audio_slot_t *const slots[], const uint32_t *locations, uint8_t count, const ble_audio_codec_t *codec, const ble_audio_qos_t *qos, uint16_t context,
  const uint8_t *code
) {
  (void)slots;
  (void)locations;
  (void)count;
  (void)codec;
  (void)qos;
  (void)context;
  (void)code;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioBsrcGetBase(uint8_t *out, uint16_t cap, uint16_t *out_len) {
  (void)out;
  (void)cap;
  (void)out_len;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioBsrcStart(uint8_t adv_handle) {
  (void)adv_handle;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioBsrcStop(void) {
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioBsrcStopTimeout(uint32_t timeout_ms) {
  (void)timeout_ms;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioBsrcUpdateContext(uint16_t context) {
  (void)context;
  return ESP_ERR_NOT_SUPPORTED;
}

void bleAudioBsrcDelete(void) {}

void *bleAudioBsrcHandle(void) {
  return NULL;
}
#endif /* !BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED */

#if !BLE_AUDIO_BROADCAST_SINK_SUPPORTED
int bleAudioBsinkInit(bool delegator) {
  (void)delegator;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioBsinkSetStreams(ble_audio_slot_t *const slots[], uint8_t count) {
  (void)slots;
  (void)count;
  return ESP_ERR_NOT_SUPPORTED;
}

void bleAudioBsinkSetScanning(bool on) {
  (void)on;
}

void bleAudioBsinkArm(uint32_t broadcast_id) {
  (void)broadcast_id;
}

void bleAudioBsinkDisarm(void) {}

void bleAudioBsinkSetCode(const uint8_t *code) {
  (void)code;
}

void bleAudioBsinkSetBisMask(uint32_t mask) {
  (void)mask;
}

int bleAudioBsinkStop(void) {
  return ESP_ERR_NOT_SUPPORTED;
}

uint16_t bleAudioBsinkSyncHandle(void) {
  return SYNC_NONE;
}
#endif /* !BLE_AUDIO_BROADCAST_SINK_SUPPORTED */

#endif /* BLE_AUDIO_SUPPORTED */
