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
 * @file BLEAudioEngineAssistant.c
 * @brief BAP Broadcast Assistant engine unit.
 *
 * Besides forwarding the BASS client callbacks, the unit keeps two pieces of
 * local state fed by the engine's GAP hook:
 *  - the Broadcast_IDs already reported during the current remote scan, so
 *    each source is reported once (small ring, MAX_SEEN entries);
 *  - the local PA syncs (MAX_SYNCS), so a remote SyncInfo request can be
 *    answered with PAST when we are synced to the requested source.
 *
 * API contract is documented on the declarations in
 * `BLEAudioEngineAssistant.h`; the definitions below carry implementation
 * notes only.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include "engine/BLEAudioEngineAssistant.h"

#include "esp_err.h"

#if BLE_AUDIO_BROADCAST_ASSISTANT_SUPPORTED

#include "esp_ble_audio_bap_api.h"
#include "esp_ble_audio_common_api.h"
#include "esp_ble_audio_defs.h"
#include "esp_ble_iso_common_api.h"
#include "esp_log.h"

#include <string.h>
#include <errno.h>

#define CONN_NONE              BLE_AUDIO_CONN_NONE
#define AD_TYPE_NAME_COMPLETE  0x09
#define AD_TYPE_SERVICE_DATA16 0x16
#define AD_TYPE_BROADCAST_NAME 0x30
#define MAX_SEEN               8 /* Broadcast_IDs remembered per scan; older ones may be re-reported. */
#define MAX_SYNCS              4 /* Local PA syncs tracked as PAST candidates. */

static const char *TAG = "BLEAudioAssistant";

/** A local PA sync that PAST could hand over. */
typedef struct {
  bool used;
  uint8_t addr[6]; /* LSB-first */
  uint8_t sid;
  uint16_t sync_handle;
} ba_sync_t;

static struct {
  bool cb_registered; /* this session */
  bool scanning;      /* a remote scan is active: report sources */
  uint8_t n_seen;     /* valid entries in seen[] */
  uint8_t seen_next;  /* ring write index */
  struct {
    uint32_t id;
    uint8_t addr[6];
    uint8_t sid;
  } seen[MAX_SEEN];           /* sources reported since the last scan start */
  ba_sync_t syncs[MAX_SYNCS]; /* local PA syncs, PAST candidates */
} s_ba;

/* Per-connection PAST dedup: two sinks asking for the same Source_ID must each
 * get one answer, so the "already answered" bits cannot be a single global
 * bitmap. One row per possible conn (same limit as the engine link table). */
#define BA_PAST_CONNS BLE_MAX_CONN
/* Tracked (Source_ID, Broadcast_ID) pairs per row. Receive states per link
 * are few; beyond BA_PAST_IDS, requests are served without dedup. */
#define BA_PAST_IDS 8
typedef struct {
  uint16_t conn;               /* CONN_NONE when free (handle 0 is a valid first ACL). */
  uint8_t sent[32];            /* 256-bit Source_ID bitmap (byte = id >> 3, bit = 1u << (id & 7)). */
  uint8_t ids[BA_PAST_IDS];    /* Source_IDs with a recorded Broadcast_ID. */
  uint32_t bcast[BA_PAST_IDS]; /* Broadcast_ID served for ids[] (24 bit). */
  uint8_t next;                /* Round-robin eviction cursor. */
} past_row_t;
static past_row_t s_ba_past[BA_PAST_CONNS];

static void past_reset(void) {
  memset(s_ba_past, 0, sizeof(s_ba_past));
  for (int i = 0; i < BA_PAST_CONNS; i++) {
    s_ba_past[i].conn = CONN_NONE;
  }
}

/** @return The bitmap row for @p conn, or NULL when it has none (not created). */
static uint8_t *past_find(uint16_t conn) {
  if (conn == CONN_NONE) {
    return NULL;
  }
  for (int i = 0; i < BA_PAST_CONNS; i++) {
    if (s_ba_past[i].conn == conn) {
      return s_ba_past[i].sent;
    }
  }
  return NULL;
}

/** @return The row for @p conn, or NULL when it has none. */
static past_row_t *past_row(uint16_t conn) {
  if (conn == CONN_NONE) {
    return NULL;
  }
  for (int i = 0; i < BA_PAST_CONNS; i++) {
    if (s_ba_past[i].conn == conn) {
      return &s_ba_past[i];
    }
  }
  return NULL;
}

/** @return The bitmap row for @p conn, claiming (and clearing) a free one. */
static uint8_t *past_claim(uint16_t conn) {
  if (conn == CONN_NONE) {
    return NULL;
  }
  uint8_t *row = past_find(conn);
  if (row) {
    return row;
  }
  for (int i = 0; i < BA_PAST_CONNS; i++) {
    if (s_ba_past[i].conn == CONN_NONE) {
      memset(&s_ba_past[i], 0, sizeof(s_ba_past[i]));
      s_ba_past[i].conn = conn;
      return s_ba_past[i].sent;
    }
  }
  return NULL;
}

/** Forget a peer's row (on disconnect), so its handle can be reused cleanly. */
static void past_forget(uint16_t conn) {
  for (int i = 0; i < BA_PAST_CONNS; i++) {
    if (s_ba_past[i].conn == conn) {
      memset(&s_ba_past[i], 0, sizeof(s_ba_past[i]));
      s_ba_past[i].conn = CONN_NONE;
    }
  }
}

static inline uint16_t conn_of(const esp_ble_conn_t *conn) {
  return conn ? conn->handle : CONN_NONE;
}

/** Emit BLE_AUDIO_EVT_BA_RESULT for @p op. */
static void emit_result(uint16_t conn, int err, uint8_t op, uint8_t src_id) {
  const ble_audio_ba_result_t r = {.op = op, .src_id = src_id};
  bleAudioEngineEmit(BLE_AUDIO_EVT_BA_RESULT, conn, err, &r);
}

/* ── PAST ───────────────────────────────────────────────────────────────── */

/**
 * Answer a SyncInfo request in @p st with PAST, once per request: the
 * per-Source_ID bit is set when served and cleared as soon as the remote
 * leaves the SyncInfo Request state, so a later request is served again. A
 * ModifySource that swaps the Broadcast_ID under a reused Source_ID while the
 * state stays INFO_REQ is detected through the recorded Broadcast_ID and
 * served again. The outcome is reported as BLE_AUDIO_BA_OP_PAST.
 */
static void ba_past(esp_ble_conn_t *conn, const esp_ble_audio_bap_scan_delegator_recv_state_t *st) {
  const uint8_t id = st->src_id;
  const uint8_t idx = id >> 3;
  const uint8_t bit = (uint8_t)(1u << (id & 7));
  const uint32_t bcast = st->broadcast_id;
  if (st->pa_sync_state != ESP_BLE_AUDIO_BAP_PA_STATE_INFO_REQ) {
    uint8_t *slot = past_find(conn->handle);
    if (slot) {
      slot[idx] &= (uint8_t)~bit;
    }
    return;
  }
  past_row_t *row = past_row(conn->handle);
  if (row == NULL) {
    if (past_claim(conn->handle) == NULL) {
      /* No free row (more peers than links): fail closed with one result
       * instead of re-sending PAST on every duplicate request. */
      ESP_LOGW(TAG, "conn %u: SyncInfo request for source %u dropped, no PAST row free", conn->handle, id);
      emit_result(conn->handle, -ENOMEM, BLE_AUDIO_BA_OP_PAST, id);
      return;
    }
    row = past_row(conn->handle);
  }
  uint8_t *slot = row->sent;
  if (slot[idx] & bit) {
    /* Served before: re-serve only when the Source_ID now names a different
     * broadcast (delegator reused the ID without an intervening removal). */
    for (uint8_t i = 0; i < BA_PAST_IDS; i++) {
      if (row->ids[i] == id) {
        if (row->bcast[i] != bcast) {
          row->bcast[i] = bcast;
          slot[idx] &= (uint8_t)~bit;
        }
        break;
      }
    }
    if (slot[idx] & bit) {
      return;
    }
  }
  /* Attempt first: the served bit is only set on success (below), so a
   * failed attempt (no local PA sync, no PAST support, send failure) leaves
   * no trace and the sink's next SyncInfo request retries instead of hitting
   * the served short-circuit above forever. */
  int err = -ENOENT;
  for (int i = 0; i < MAX_SYNCS; i++) {
    const ba_sync_t *s = &s_ba.syncs[i];
    if (!s->used || s->sid != st->adv_sid || memcmp(s->addr, st->addr.a.val, 6) != 0) {
      continue;
    }
    /* Service data: bits 0-7 AdvA matches the added source (0), bits 8-15 Source_ID. */
    const uint16_t service_data = (uint16_t)id << 8;
#if BLE_PAST_SUPPORTED
    uint8_t peer_addr[6];
    bleAudioAddrFromGap(peer_addr, conn->le.dst.a.val);
    err = bleAudioEngineSendPastSync(s->sync_handle, conn->handle, service_data, peer_addr, conn->le.dst.type);
    if (err == ESP_ERR_NOT_SUPPORTED) {
      err = -ENOTSUP;
    }
#else
    (void)service_data;
    err = -ENOTSUP;
#endif
    break;
  }
  if (err == 0) {
    /* Served: evict the oldest entry and record this one, so a duplicate
     * request for the same Source_ID is dropped instead of re-sending PAST
     * on every duplicate request. */
    {
      const uint8_t evicted = row->ids[row->next];
      const uint8_t ev_idx = evicted >> 3;
      const uint8_t ev_bit = (uint8_t)(1u << (evicted & 7));
      slot[ev_idx] &= (uint8_t)~ev_bit;
      for (uint8_t i = 0; i < BA_PAST_IDS; i++) {
        if (i != row->next && row->ids[i] == evicted) {
          slot[ev_idx] |= ev_bit;
          break;
        }
      }
    }
    row->ids[row->next] = id;
    row->bcast[row->next] = bcast;
    row->next = (uint8_t)((row->next + 1) % BA_PAST_IDS);
    slot[idx] |= bit;
  } else {
    ESP_LOGW(
      TAG, "conn %u: SyncInfo request for source %u not served (%s): %d", conn->handle, id,
      err == -ENOENT    ? "no local PA sync to it"
      : err == -ENOTSUP ? "host built without PAST"
                        : "PAST send failed",
      err
    );
  }
  emit_result(conn->handle, err, BLE_AUDIO_BA_OP_PAST, id);
}

/** Record (or refresh) a local PA sync; logs and drops when all MAX_SYNCS slots are taken. */
static void sync_add(uint16_t handle, const uint8_t addr[6], uint8_t sid) {
  ba_sync_t *free_slot = NULL;
  for (int i = 0; i < MAX_SYNCS; i++) {
    ba_sync_t *s = &s_ba.syncs[i];
    if (s->used && s->sync_handle == handle) {
      free_slot = s;
      break;
    }
    if (!s->used && free_slot == NULL) {
      free_slot = s;
    }
  }
  if (free_slot) {
    free_slot->used = true;
    free_slot->sync_handle = handle;
    free_slot->sid = sid;
    bleAudioAddrFromGap(free_slot->addr, addr);
  } else {
    ESP_LOGW(TAG, "PA sync table full (%d entries); ignoring sync 0x%04x", MAX_SYNCS, handle);
  }
}

static void sync_remove(uint16_t handle) {
  for (int i = 0; i < MAX_SYNCS; i++) {
    if (s_ba.syncs[i].used && s_ba.syncs[i].sync_handle == handle) {
      s_ba.syncs[i].used = false;
    }
  }
}

/* ── Scan reports ───────────────────────────────────────────────────────── */

/** AD parser state for one scan report. */
typedef struct {
  ble_audio_ba_source_t *out;
  bool has_id; /* Broadcast Audio Announcement found: this is a broadcast source */
} ba_parse_t;

/** AD structure visitor: pick the name (Broadcast Name wins) and the Broadcast_ID. */
static bool ba_ad_cb(uint8_t type, const uint8_t *data, uint8_t len, void *user) {
  ba_parse_t *p = (ba_parse_t *)user;
  if ((type == AD_TYPE_BROADCAST_NAME || (type == AD_TYPE_NAME_COMPLETE && p->out->name[0] == '\0')) && len > 0) {
    size_t n = len > BLE_AUDIO_BA_NAME_MAX ? BLE_AUDIO_BA_NAME_MAX : len;
    memcpy(p->out->name, data, n);
    p->out->name[n] = '\0';
  } else if (type == AD_TYPE_SERVICE_DATA16 && len >= 5 && (data[0] | (data[1] << 8)) == ESP_BLE_AUDIO_UUID_BROADCAST_AUDIO_VAL) {
    p->out->broadcast_id = (uint32_t)data[2] | ((uint32_t)data[3] << 8) | ((uint32_t)data[4] << 16);
    p->has_id = true;
  }
  return true;
}

/**
 * Scan report: while a remote scan is active, report periodic advertisers
 * carrying a Broadcast Audio Announcement, each (address, SID, Broadcast_ID)
 * once. The same Broadcast_ID from a different advertiser is a different
 * source and must still be reported.
 */
static void ba_on_scan(const esp_ble_audio_gap_app_event_t *ev) {
  if (!s_ba.scanning || ev->ext_scan_recv.data == NULL || ev->ext_scan_recv.per_adv_itvl == 0) {
    return;
  }
  ble_audio_ba_source_t src = {0};
  ba_parse_t p = {.out = &src};
  (void)esp_ble_audio_data_parse(ev->ext_scan_recv.data, ev->ext_scan_recv.data_len, ba_ad_cb, &p);
  if (!p.has_id) {
    return;
  }
  uint8_t addr_lsb[6];
  bleAudioAddrFromGap(addr_lsb, ev->ext_scan_recv.addr.val);
  for (uint8_t i = 0; i < s_ba.n_seen; i++) {
    if (s_ba.seen[i].id == src.broadcast_id && s_ba.seen[i].sid == ev->ext_scan_recv.sid && memcmp(s_ba.seen[i].addr, addr_lsb, 6) == 0) {
      return;
    }
  }
  s_ba.seen[s_ba.seen_next].id = src.broadcast_id;
  memcpy(s_ba.seen[s_ba.seen_next].addr, addr_lsb, 6);
  s_ba.seen[s_ba.seen_next].sid = ev->ext_scan_recv.sid;
  s_ba.seen_next = (uint8_t)((s_ba.seen_next + 1) % MAX_SEEN);
  if (s_ba.n_seen < MAX_SEEN) {
    s_ba.n_seen++;
  }

  src.addr_type = ev->ext_scan_recv.addr.type;
  memcpy(src.addr, addr_lsb, 6);
  src.sid = ev->ext_scan_recv.sid;
  src.rssi = ev->ext_scan_recv.rssi;
  src.pa_interval = ev->ext_scan_recv.per_adv_itvl;
  bleAudioEngineEmit(BLE_AUDIO_EVT_BA_SOURCE_FOUND, CONN_NONE, 0, &src);
}

/* ── Unit hooks ─────────────────────────────────────────────────────────── */

/** Engine GAP hook: scan reports and the local PA sync lifecycle. */
static void ba_on_gap(const void *event) {
  const esp_ble_audio_gap_app_event_t *ev = (const esp_ble_audio_gap_app_event_t *)event;
  switch (ev->type) {
    case ESP_BLE_AUDIO_GAP_EVENT_EXT_SCAN_RECV: ba_on_scan(ev); break;
    case ESP_BLE_AUDIO_GAP_EVENT_PA_SYNC:
      if (ev->pa_sync.status == 0) {
        sync_add(ev->pa_sync.sync_handle, ev->pa_sync.addr.val, ev->pa_sync.sid);
      }
      break;
    case ESP_BLE_AUDIO_GAP_EVENT_PA_SYNC_PAST:
      if (ev->pa_sync_past.status == 0) {
        sync_add(ev->pa_sync_past.sync_handle, ev->pa_sync_past.addr.val, ev->pa_sync_past.sid);
      }
      break;
    case ESP_BLE_AUDIO_GAP_EVENT_PA_SYNC_LOST:   sync_remove(ev->pa_sync_lost.sync_handle); break;
    case ESP_BLE_AUDIO_GAP_EVENT_ACL_DISCONNECT: past_forget(ev->acl_disconnect.conn_handle); break;
    default:                                     break;
  }
}

/** Engine deinit hook: clear session state so the next session re-registers callbacks. */
static void ba_on_deinit(void) {
  memset(&s_ba, 0, sizeof(s_ba));
  past_reset();
}

static const ble_audio_unit_hooks_t s_hooks = {.on_gap = ba_on_gap, .on_deinit = ba_on_deinit};

/* ── Stack callbacks ────────────────────────────────────────────────────── */

/** Remote BASS discovered; the stack reads the receive states next (RECV_STATE each). */
static void ba_discover_cb(esp_ble_conn_t *conn, int err, uint8_t recv_state_count) {
  const ble_audio_ba_discovered_t d = {.recv_states = err ? 0 : recv_state_count};
  bleAudioEngineEmit(BLE_AUDIO_EVT_BA_DISCOVERED, conn_of(conn), err, &d);
}

/** Receive state read or notified: report it, then serve a pending SyncInfo request. */
static void ba_recv_state_cb(esp_ble_conn_t *conn, int err, const esp_ble_audio_bap_scan_delegator_recv_state_t *st) {
  if (err != 0 || st == NULL) {
    if (err != 0) {
      ESP_LOGW(TAG, "conn %u: reading a Broadcast Receive State failed: %d", conn_of(conn), err);
    }
    return;
  }
  ble_audio_ba_recv_state_t out = {0};
  out.src_id = st->src_id;
  out.addr_type = st->addr.type;
  memcpy(out.addr, st->addr.a.val, 6);
  out.sid = st->adv_sid;
  out.broadcast_id = st->broadcast_id;
  out.pa_sync_state = (uint8_t)st->pa_sync_state;
  out.encrypt_state = (uint8_t)st->encrypt_state;
  if (st->encrypt_state == ESP_BLE_AUDIO_BAP_BIG_ENC_STATE_BAD_CODE) {
    memcpy(out.bad_code, st->bad_code, BLE_AUDIO_BA_CODE_SIZE);
  }
  out.num_subgroups = st->subgroups ? (st->num_subgroups > BLE_AUDIO_BA_MAX_SUBGROUPS ? BLE_AUDIO_BA_MAX_SUBGROUPS : st->num_subgroups) : 0;
  for (uint8_t i = 0; i < out.num_subgroups; i++) {
    out.bis_sync[i] = st->subgroups[i].bis_sync;
  }
  bleAudioEngineEmit(BLE_AUDIO_EVT_BA_RECV_STATE, conn_of(conn), 0, &out);
  if (conn) {
    ba_past(conn, st);
  }
}

/** The remote emptied a receive state; a reused Source_ID must be served PAST again. */
static void ba_recv_state_removed_cb(esp_ble_conn_t *conn, uint8_t src_id) {
  if (conn) {
    uint8_t *slot = past_find(conn_of(conn));
    if (slot) {
      slot[src_id >> 3] &= (uint8_t) ~(1u << (src_id & 7));
    }
  }
  const ble_audio_ba_result_t r = {.op = BLE_AUDIO_BA_OP_REMOVE_SOURCE, .src_id = src_id};
  bleAudioEngineEmit(BLE_AUDIO_EVT_BA_RECV_STATE_REMOVED, conn_of(conn), 0, &r);
}

/* Control point write completions: all map to BLE_AUDIO_EVT_BA_RESULT with their op. */
#define BA_RESULT_CB(name, op)                                     \
  static void name(esp_ble_conn_t *conn, int err) {                \
    emit_result(conn_of(conn), err, op, BLE_AUDIO_BA_SRC_ID_NONE); \
  }
BA_RESULT_CB(ba_scan_start_cb, BLE_AUDIO_BA_OP_SCAN_START)
BA_RESULT_CB(ba_scan_stop_cb, BLE_AUDIO_BA_OP_SCAN_STOP)
BA_RESULT_CB(ba_add_src_cb, BLE_AUDIO_BA_OP_ADD_SOURCE)
BA_RESULT_CB(ba_mod_src_cb, BLE_AUDIO_BA_OP_MODIFY_SOURCE)
BA_RESULT_CB(ba_rem_src_cb, BLE_AUDIO_BA_OP_REMOVE_SOURCE)
BA_RESULT_CB(ba_bcode_cb, BLE_AUDIO_BA_OP_BROADCAST_CODE)

/* Not const: the stack links it into its callback list. */
static esp_ble_audio_bap_broadcast_assistant_cb_t s_ba_cb = {
  .discover = ba_discover_cb,
  .recv_state = ba_recv_state_cb,
  .recv_state_removed = ba_recv_state_removed_cb,
  .scan_start = ba_scan_start_cb,
  .scan_stop = ba_scan_stop_cb,
  .add_src = ba_add_src_cb,
  .mod_src = ba_mod_src_cb,
  .broadcast_code = ba_bcode_cb,
  .rem_src = ba_rem_src_cb,
};

/* ── API ────────────────────────────────────────────────────────────────── */

int bleAudioBaInit(void) {
  if (!bleAudioEngineIsInitialized()) {
    return ESP_ERR_INVALID_STATE;
  }
  int err = bleAudioEngineRegisterUnit(&s_hooks);
  if (err != 0 || s_ba.cb_registered) {
    return err;
  }
  past_reset();
  err = esp_ble_audio_bap_broadcast_assistant_register_cb(&s_ba_cb);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "broadcast assistant: callback registration failed: %d", err);
    return err;
  }
  s_ba.cb_registered = true;
  return 0;
}

/* Every entry point initializes lazily, so the role works without an explicit bleAudioBaInit(). */

int bleAudioBaDiscover(uint16_t conn_handle) {
  int err = bleAudioBaInit();
  return err ? err : (int)esp_ble_audio_bap_broadcast_assistant_discover(conn_handle);
}

int bleAudioBaScanStart(uint16_t conn_handle, bool stack_scan) {
  int err = bleAudioBaInit();
  if (err != 0) {
    return err;
  }
  /* Set before the call: reports may arrive before it returns. A new scan re-reports every source. */
  s_ba.n_seen = 0;
  s_ba.scanning = true;
  err = esp_ble_audio_bap_broadcast_assistant_scan_start(conn_handle, stack_scan);
  if (err != ESP_OK) {
    s_ba.scanning = false;
  }
  return err;
}

int bleAudioBaScanStop(uint16_t conn_handle) {
  int err = bleAudioBaInit();
  if (err != 0) {
    return err;
  }
  s_ba.scanning = false;
  return (int)esp_ble_audio_bap_broadcast_assistant_scan_stop(conn_handle);
}

int bleAudioBaAddSource(uint16_t conn_handle, const ble_audio_ba_add_src_t *src) {
  if (src == NULL || src->num_subgroups == 0 || src->num_subgroups > BLE_AUDIO_BA_MAX_SUBGROUPS) {
    return ESP_ERR_INVALID_ARG;
  }
  int err = bleAudioBaInit();
  if (err != 0) {
    return err;
  }
  /* The stack copies the parameters into the control point write, so locals suffice. */
  esp_ble_audio_bap_bass_subgroup_t sub[BLE_AUDIO_BA_MAX_SUBGROUPS];
  memset(sub, 0, sizeof(sub));
  for (uint8_t i = 0; i < src->num_subgroups; i++) {
    sub[i].bis_sync = src->bis_sync[i];
    sub[i].metadata_len = src->meta ? src->meta_len : 0;
    sub[i].metadata = (uint8_t *)src->meta;
  }
  esp_ble_audio_bap_broadcast_assistant_add_src_param_t p = {0};
  p.addr.type = src->addr_type & 0x01; /* identity types → public / random */
  memcpy(p.addr.a.val, src->addr, 6);
  p.adv_sid = src->sid;
  p.pa_sync = src->pa_sync;
  p.broadcast_id = src->broadcast_id;
  p.pa_interval = src->pa_interval;
  p.num_subgroups = src->num_subgroups;
  p.subgroups = sub;
  return (int)esp_ble_audio_bap_broadcast_assistant_add_src(conn_handle, &p);
}

int bleAudioBaModifySource(uint16_t conn_handle, const ble_audio_ba_mod_src_t *src) {
  if (src == NULL || src->num_subgroups > BLE_AUDIO_BA_MAX_SUBGROUPS) {
    return ESP_ERR_INVALID_ARG;
  }
  int err = bleAudioBaInit();
  if (err != 0) {
    return err;
  }
  esp_ble_audio_bap_bass_subgroup_t sub[BLE_AUDIO_BA_MAX_SUBGROUPS];
  memset(sub, 0, sizeof(sub));
  for (uint8_t i = 0; i < src->num_subgroups; i++) {
    sub[i].bis_sync = src->bis_sync[i];
  }
  esp_ble_audio_bap_broadcast_assistant_mod_src_param_t p = {0};
  p.src_id = src->src_id;
  p.pa_sync = src->pa_sync;
  p.pa_interval = src->pa_interval;
  p.num_subgroups = src->num_subgroups;
  p.subgroups = src->num_subgroups ? sub : NULL;
  return (int)esp_ble_audio_bap_broadcast_assistant_mod_src(conn_handle, &p);
}

int bleAudioBaRemoveSource(uint16_t conn_handle, uint8_t src_id) {
  int err = bleAudioBaInit();
  return err ? err : (int)esp_ble_audio_bap_broadcast_assistant_rem_src(conn_handle, src_id);
}

int bleAudioBaSetBroadcastCode(uint16_t conn_handle, uint8_t src_id, const uint8_t code[BLE_AUDIO_BA_CODE_SIZE]) {
  if (code == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  int err = bleAudioBaInit();
  return err ? err : (int)esp_ble_audio_bap_broadcast_assistant_set_broadcast_code(conn_handle, src_id, code);
}

#endif /* BLE_AUDIO_BROADCAST_ASSISTANT_SUPPORTED */

/* ==========================================================================
 * Feature-disabled stubs (!BLE_AUDIO_BROADCAST_ASSISTANT_SUPPORTED)
 * ========================================================================== */

#if !BLE_AUDIO_BROADCAST_ASSISTANT_SUPPORTED

int bleAudioBaInit(void) {
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioBaDiscover(uint16_t conn_handle) {
  (void)conn_handle;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioBaScanStart(uint16_t conn_handle, bool stack_scan) {
  (void)conn_handle;
  (void)stack_scan;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioBaScanStop(uint16_t conn_handle) {
  (void)conn_handle;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioBaAddSource(uint16_t conn_handle, const ble_audio_ba_add_src_t *src) {
  (void)conn_handle;
  (void)src;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioBaModifySource(uint16_t conn_handle, const ble_audio_ba_mod_src_t *src) {
  (void)conn_handle;
  (void)src;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioBaRemoveSource(uint16_t conn_handle, uint8_t src_id) {
  (void)conn_handle;
  (void)src_id;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioBaSetBroadcastCode(uint16_t conn_handle, uint8_t src_id, const uint8_t code[BLE_AUDIO_BA_CODE_SIZE]) {
  (void)conn_handle;
  (void)src_id;
  (void)code;
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* !BLE_AUDIO_BROADCAST_ASSISTANT_SUPPORTED */

#endif /* BLE_AUDIO_SUPPORTED */
