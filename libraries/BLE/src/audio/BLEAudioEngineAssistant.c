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

#include "audio/BLEAudioEngineAssistant.h"

#include "esp_err.h"

#if BLE_AUDIO_BROADCAST_ASSISTANT_SUPPORTED

#include "esp_ble_audio_bap_api.h"
#include "esp_ble_audio_common_api.h"
#include "esp_ble_audio_defs.h"
#include "esp_ble_iso_common_api.h"
#include "esp_log.h"

#include <string.h>
#include <errno.h>

/* PAST senders, declared here: the host headers do not mix with the Zephyr ones. */
#if BLE_NIMBLE && defined(CONFIG_BT_NIMBLE_PERIODIC_ADV_SYNC_TRANSFER)
#define BA_HAVE_PAST 1
int ble_gap_periodic_adv_sync_transfer(uint16_t sync_handle, uint16_t conn_handle, uint16_t service_data);
#elif BLE_BLUEDROID && defined(CONFIG_BT_BLE_FEAT_PERIODIC_ADV_SYNC_TRANSFER)
#define BA_HAVE_PAST 1
esp_err_t esp_ble_gap_periodic_adv_sync_trans(uint8_t addr[6], uint16_t service_data, uint16_t sync_handle);
#else
#define BA_HAVE_PAST 0
#endif

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

/* Survives deinit: the stack keeps the callback node linked if bt_le_audio_deinit()
 * does not reset its list, and a second register then fails with -EALREADY. */
static bool s_ba_cb_linked;

static struct {
  bool cb_registered;  /* this session */
  bool scanning;       /* a remote scan is active: report sources */
  uint8_t n_seen;      /* valid entries in seen[] */
  uint8_t seen_next;   /* ring write index */
  uint32_t seen[MAX_SEEN]; /* broadcast IDs reported since the last scan start */
  ba_sync_t syncs[MAX_SYNCS]; /* local PA syncs, PAST candidates */
  uint8_t past_sent[32];      /* src_id bitmap: SyncInfo request already answered */
} s_ba;

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
 * leaves the SyncInfo Request state, so a later request is served again.
 * The outcome is reported as BLE_AUDIO_BA_OP_PAST.
 */
static void ba_past(esp_ble_conn_t *conn, const esp_ble_audio_bap_scan_delegator_recv_state_t *st) {
  const uint8_t id = st->src_id;
  const uint8_t bit = (uint8_t)(1u << (id & 7));
  uint8_t *slot = &s_ba.past_sent[id >> 3];
  if (st->pa_sync_state != ESP_BLE_AUDIO_BAP_PA_STATE_INFO_REQ) {
    *slot &= (uint8_t)~bit;
    return;
  }
  if (*slot & bit) {
    return;
  }
  *slot |= bit;

  int err = -ENOENT;
  for (int i = 0; i < MAX_SYNCS; i++) {
    const ba_sync_t *s = &s_ba.syncs[i];
    if (!s->used || s->sid != st->adv_sid || memcmp(s->addr, st->addr.a.val, 6) != 0) {
      continue;
    }
    /* Service data: bits 0-7 AdvA matches the added source (0), bits 8-15 Source_ID. */
    const uint16_t service_data = (uint16_t)id << 8;
#if BA_HAVE_PAST && BLE_NIMBLE
    err = ble_gap_periodic_adv_sync_transfer(s->sync_handle, conn->handle, service_data) == 0 ? 0 : ESP_FAIL;
#elif BA_HAVE_PAST
    /* Bluedroid keeps conn->le.dst in its own (MSB-first) order, as this API expects. */
    err = (int)esp_ble_gap_periodic_adv_sync_trans(conn->le.dst.a.val, service_data, s->sync_handle);
#else
    (void)service_data;
    err = -ENOTSUP;
#endif
    break;
  }
  if (err != 0) {
    ESP_LOGW(TAG, "conn %u: SyncInfo request for source %u not served (%s): %d", conn->handle, id,
             err == -ENOENT ? "no local PA sync to it" : err == -ENOTSUP ? "host built without PAST" : "PAST send failed", err);
  }
  emit_result(conn->handle, err, BLE_AUDIO_BA_OP_PAST, id);
}

/** Record (or refresh) a local PA sync; silently ignored when all MAX_SYNCS slots are taken. */
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
  bool has_id;  /* Broadcast Audio Announcement found: this is a broadcast source */
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
 * carrying a Broadcast Audio Announcement, each Broadcast_ID once.
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
  for (uint8_t i = 0; i < s_ba.n_seen; i++) {
    if (s_ba.seen[i] == src.broadcast_id) {
      return;
    }
  }
  s_ba.seen[s_ba.seen_next] = src.broadcast_id;
  s_ba.seen_next = (uint8_t)((s_ba.seen_next + 1) % MAX_SEEN);
  if (s_ba.n_seen < MAX_SEEN) {
    s_ba.n_seen++;
  }

  src.addr_type = ev->ext_scan_recv.addr.type;
  bleAudioAddrFromGap(src.addr, ev->ext_scan_recv.addr.val);
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
    case ESP_BLE_AUDIO_GAP_EVENT_PA_SYNC_LOST: sync_remove(ev->pa_sync_lost.sync_handle); break;
    default:                                   break;
  }
}

/** Engine deinit hook; s_ba_cb_linked deliberately survives (see its comment). */
static void ba_on_deinit(void) {
  memset(&s_ba, 0, sizeof(s_ba));
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
  s_ba.past_sent[src_id >> 3] &= (uint8_t)~(1u << (src_id & 7));
  const ble_audio_ba_result_t r = {.op = BLE_AUDIO_BA_OP_REMOVE_SOURCE, .src_id = src_id};
  bleAudioEngineEmit(BLE_AUDIO_EVT_BA_RECV_STATE_REMOVED, conn_of(conn), 0, &r);
}

/* Control point write completions: all map to BLE_AUDIO_EVT_BA_RESULT with their op. */
#define BA_RESULT_CB(name, op)                            \
  static void name(esp_ble_conn_t *conn, int err) {       \
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
  err = esp_ble_audio_bap_broadcast_assistant_register_cb(&s_ba_cb);
  /* After a re-init the table may still be linked from the previous session; that is fine. */
  if (err != ESP_OK && !s_ba_cb_linked) {
    ESP_LOGE(TAG, "broadcast assistant: callback registration failed: %d", err);
    return err;
  }
  s_ba_cb_linked = true;
  s_ba.cb_registered = true;
  return 0;
}

/* Every entry point initialises lazily, so the role works without an explicit bleAudioBaInit(). */

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

#else /* !BLE_AUDIO_BROADCAST_ASSISTANT_SUPPORTED */

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

#endif /* BLE_AUDIO_BROADCAST_ASSISTANT_SUPPORTED */

#endif /* BLE_AUDIO_SUPPORTED */
