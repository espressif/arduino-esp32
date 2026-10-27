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
 * @file BLEAudioEngineCap.c
 * @brief CAP engine unit: CSIS server, CSIP set coordinator, CAP initiator
 *        (unicast and broadcast), CAP commander and CAP handover.
 *
 * Sections, in order:
 *  - Peers: one cap_peer_t per connection that a client procedure touched,
 *    holding the stack connection object (set procedures need the object,
 *    not the handle), discovered endpoints and per-role discovery flags.
 *  - CSIS server: up to two instances (standalone, CAS-included); lock and
 *    SIRK-read callbacks are emitted under the owning role's event group.
 *  - CSIP set coordinator: discovery, lock/release of every member of a set.
 *  - CAP initiator unicast: CAS -> (BASS) -> ASCS sink -> ASCS source
 *    discovery chain, then one unicast group started with the CAP procedure.
 *  - CAP initiator broadcast: one CAP broadcast source bound to BAP slots.
 *  - Commander: VCP/MICP/BASS procedures fanned out to every ready peer.
 *  - Handover: unicast-to-broadcast and back, built on the two above.
 *
 * All stack callbacks run on the host task and report through the engine's
 * synchronous event bridge. API contract is documented on the declarations in
 * `BLEAudioEngineCap.h`; the definitions below carry implementation notes only.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include "audio/BLEAudioEngineCap.h"

#include "esp_ble_audio_bap_api.h"
#include "esp_ble_audio_cap_api.h"
#include "esp_ble_audio_csip_api.h"
#include "esp_ble_audio_codec_api.h"
#include "esp_ble_audio_common_api.h"
#include "esp_ble_audio_defs.h"
#include "esp_ble_audio_micp_api.h"
#include "esp_ble_audio_vcp_api.h"
#include "esp_log.h"

#include <string.h>
#include <errno.h>

#define CONN_NONE BLE_AUDIO_CONN_NONE
#define COUNT_OF(a) (sizeof(a) / sizeof((a)[0]))

/* Feature switches derived from the role guards; BASS use and PAST need a combination of roles. */
#define CAP_INIT  BLE_AUDIO_CAP_INITIATOR_SUPPORTED
#define CAP_UC    (CAP_INIT && BLE_AUDIO_UNICAST_CLIENT_SUPPORTED)
#define CAP_BC    (CAP_INIT && BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED)
#define CAP_CMD   BLE_AUDIO_CAP_COMMANDER_SUPPORTED
#define CMD_VCP   (CAP_CMD && BLE_AUDIO_VCP_CONTROLLER_SUPPORTED)
#define CMD_MICP  (CAP_CMD && BLE_AUDIO_MICP_CONTROLLER_SUPPORTED)
#define CAP_HO    BLE_AUDIO_CAP_HANDOVER_SUPPORTED
#define CAP_BASS  (BLE_AUDIO_BROADCAST_ASSISTANT_SUPPORTED && (CAP_CMD || CAP_HO))
#define CAP_PAST  (CAP_BC && CAP_BASS)
#define CSIS_SRV  BLE_AUDIO_CSIP_MEMBER_SUPPORTED
#define CSIP_CRD  BLE_AUDIO_CSIP_COORDINATOR_SUPPORTED
#define CAP_PEERS (CAP_UC || CAP_CMD || CSIP_CRD)
#define CAP_ANY   (CAP_INIT || CAP_CMD || CSIS_SRV || CSIP_CRD)

#if CAP_ANY

static const char *TAG = "BLEAudioCap";

static void cap_on_gap(const void *event);
static void cap_on_deinit(void);
static const ble_audio_unit_hooks_t s_hooks = {.on_gap = cap_on_gap, .on_deinit = cap_on_deinit};

/** Register this unit's GAP/deinit hooks with the engine (idempotent). */
static int cap_attach(void) {
  return bleAudioEngineRegisterUnit(&s_hooks);
}

/* Little-endian writers for LTV metadata and advertising payloads. */
static inline void audio_put_le16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}

static inline void audio_put_le32(uint8_t *p, uint32_t v) {
  audio_put_le16(p, (uint16_t)v);
  audio_put_le16(p + 2, (uint16_t)(v >> 16));
}

static inline uint16_t handle_of(const esp_ble_conn_t *conn) {
  return conn ? conn->handle : CONN_NONE;
}

#endif /* CAP_ANY */

/* ── Peers ──────────────────────────────────────────────────────────────── */

#if CAP_PEERS

#ifdef CONFIG_BT_MAX_CONN
#define MAX_PEERS CONFIG_BT_MAX_CONN
#else
#define MAX_PEERS 2
#endif

/** Stage of the initiator's discovery chain (CAS, optional BASS, then ASCS sink and source). */
enum { D_IDLE = 0, D_CAS, D_BASS, D_SNK, D_SRC };

/** Everything the client roles know about one connected peer. */
typedef struct {
  bool used;
  uint16_t handle;
  esp_ble_conn_t *conn; /* from the discovery callbacks; set members need the object, not the handle */
#if CAP_PAST
  uint8_t addr_type;    ///< Peer identity, for hosts that address PAST by peer address.
  uint8_t addr[6];
#endif
#if CAP_UC
  uint8_t disc;         ///< D_* stage of an initiator discovery in flight.
  uint8_t n_snk;        ///< Sink endpoints collected so far.
  uint8_t n_src;        ///< Source endpoints collected so far.
  esp_ble_audio_bap_ep_t *snk[CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SNK_COUNT];
  esp_ble_audio_bap_ep_t *src[CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SRC_COUNT];
  ble_audio_cap_discovered_t info;  ///< Accumulated for BLE_AUDIO_EVT_CAP_DISCOVERED.
#endif
#if CAP_CMD
  bool cmd_disc;        ///< Commander discovery in flight.
  bool cmd_ready;       ///< CAS discovered: commander procedures include this peer.
  bool cmd_coord;       ///< The peer's CAS includes CSIS.
  uint8_t cmd_stage;    ///< CMD_D_* step of the commander discovery after CAS.
  const void *cmd_ctlr; ///< VCP / MICP controller instance whose discovery is in flight.
#endif
#if CAP_BASS
  bool bass;            ///< BASS discovered on the peer.
  bool src_valid;       ///< src_id holds the peer's Source_ID for the tracked broadcast.
  uint8_t src_id;
#endif
#if CSIP_CRD
  const esp_ble_audio_csip_set_coordinator_set_member_t *csip;  ///< Set member object from CSIP discovery.
#endif
} cap_peer_t;

static cap_peer_t s_peers[MAX_PEERS];

/**
 * Find the peer entry for @p handle, optionally claiming a free one.
 * @return NULL when absent (and @p create is false) or the table is full.
 */
static cap_peer_t *peer_get(uint16_t handle, bool create) {
  cap_peer_t *spare = NULL;
  for (int i = 0; i < MAX_PEERS; i++) {
    if (s_peers[i].used && s_peers[i].handle == handle) {
      return &s_peers[i];
    }
    if (!s_peers[i].used && spare == NULL) {
      spare = &s_peers[i];
    }
  }
  if (!create || spare == NULL) {
    return NULL;
  }
  memset(spare, 0, sizeof(*spare));
  spare->used = true;
  spare->handle = handle;
  return spare;
}

/**
 * Entry of an already-known peer from a stack callback, refreshing its cached
 * connection object. Unknown connections (procedures we did not start) give NULL.
 */
static cap_peer_t *peer_of(esp_ble_conn_t *conn) {
  cap_peer_t *p = conn ? peer_get(conn->handle, false) : NULL;
  if (p) {
    p->conn = conn;
  }
  return p;
}

#endif /* CAP_PEERS */

#if CAP_BASS
#define TRACK_NONE 0xFFFFFFFFu
static uint32_t s_track_id = TRACK_NONE; /* broadcast whose receive states we follow */
static int bass_attach(void);
#endif

/* ── CSIS server ────────────────────────────────────────────────────────── */

#if CSIS_SRV

static esp_ble_audio_csip_set_member_svc_inst_t *s_csis[2]; /* [0] standalone, [1] CAS-included */

/** Event type @p n in the group of the role owning @p svc (CAP acceptor or CSIP member). */
static uint16_t csis_evt(const esp_ble_audio_csip_set_member_svc_inst_t *svc, uint8_t n) {
  return BLE_AUDIO_EVT(svc == s_csis[1] ? BLE_AUDIO_GRP_CAP_ACCEPTOR : BLE_AUDIO_GRP_CSIP_MEMBER, n);
}

/** Lock changed by a client (@p conn), locally, or by lock timeout (@p conn NULL). */
static void csis_lock_changed(esp_ble_conn_t *conn, esp_ble_audio_csip_set_member_svc_inst_t *svc, bool locked) {
  ble_audio_csis_lock_t l = {.locked = locked};
  bleAudioEngineEmit(csis_evt(svc, BLE_AUDIO_CSIS_EVT_LOCK), handle_of(conn), 0, &l);
}

/**
 * A client reads the SIRK: let the application choose the response through
 * the synchronous emit, then map BLE_AUDIO_SIRK_* to the stack value
 * (out-of-range replies fall back to ACCEPT).
 */
static uint8_t csis_sirk_req(esp_ble_conn_t *conn, esp_ble_audio_csip_set_member_svc_inst_t *svc) {
  static const uint8_t rsp[] = {
    ESP_BLE_AUDIO_CSIP_READ_SIRK_REQ_RSP_ACCEPT,
    ESP_BLE_AUDIO_CSIP_READ_SIRK_REQ_RSP_ACCEPT_ENC,
    ESP_BLE_AUDIO_CSIP_READ_SIRK_REQ_RSP_REJECT,
    ESP_BLE_AUDIO_CSIP_READ_SIRK_REQ_RSP_OOB_ONLY,
  };
  uint8_t reply = BLE_AUDIO_SIRK_ACCEPT;
  ble_audio_csis_sirk_req_t r = {.reply = &reply};
  bleAudioEngineEmit(csis_evt(svc, BLE_AUDIO_CSIS_EVT_SIRK_REQ), handle_of(conn), 0, &r);
  return rsp[reply < COUNT_OF(rsp) ? reply : 0];
}

static esp_ble_audio_csip_set_member_cb_t s_csis_cb = {
  .lock_changed = csis_lock_changed,
  .sirk_read_req = csis_sirk_req,
};

int bleAudioCsisRegister(bool cas, const ble_audio_csis_cfg_t *cfg) {
  esp_ble_audio_csip_set_member_register_param_t param = {0};
  esp_err_t err;

  if (cfg == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (s_csis[cas]) {
    return ESP_ERR_INVALID_STATE;
  }
#if !BLE_AUDIO_CAP_ACCEPTOR_SUPPORTED
  if (cas) {
    return ESP_ERR_NOT_SUPPORTED;
  }
#endif
  cap_attach();
  param.set_size = cfg->set_size;
  param.rank = cfg->rank;
  param.lockable = cfg->lockable;
  memcpy(param.sirk, cfg->sirk, sizeof(param.sirk));
  if (cfg->name && cfg->name_len) {
    param.set_name_len = cfg->name_len < sizeof(param.set_name) ? cfg->name_len : sizeof(param.set_name);
    memcpy(param.set_name, cfg->name, param.set_name_len);
  }
  param.cb = &s_csis_cb;
#if BLE_AUDIO_CAP_ACCEPTOR_SUPPORTED
  if (cas) {
    err = esp_ble_audio_cap_acceptor_register(&param, &s_csis[1]);
  } else
#endif
  {
    err = esp_ble_audio_csip_set_member_register(&param, &s_csis[0]);
  }
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "CSIP set member: CSIS registration (%s) failed: %d", cas ? "included in CAS" : "standalone", err);
    s_csis[cas] = NULL;
    return (int)err;
  }
  /* common_start must list every CSIS instance, so hand it to the engine. */
  return bleAudioEngineAddCsis(s_csis[cas], cas);
}

int bleAudioCsisSetSirk(bool cas, const uint8_t sirk[BLE_AUDIO_SIRK_LEN]) {
  return s_csis[cas] ? (int)esp_ble_audio_csip_set_member_sirk(s_csis[cas], sirk) : ESP_ERR_INVALID_STATE;
}

int bleAudioCsisSetSizeRank(bool cas, uint8_t size, uint8_t rank) {
  return s_csis[cas] ? (int)esp_ble_audio_csip_set_member_set_size_and_rank(s_csis[cas], size, rank) : ESP_ERR_INVALID_STATE;
}

int bleAudioCsisSetName(bool cas, const uint8_t *name, uint8_t len) {
  return s_csis[cas] ? (int)esp_ble_audio_csip_set_member_set_name(s_csis[cas], name, len) : ESP_ERR_INVALID_STATE;
}

int bleAudioCsisGenerateRsi(bool cas, uint8_t rsi[BLE_AUDIO_RSI_LEN]) {
  return s_csis[cas] ? (int)esp_ble_audio_csip_set_member_generate_rsi(s_csis[cas], rsi) : ESP_ERR_INVALID_STATE;
}

int bleAudioCsisLock(bool cas, bool lock, bool force) {
  return s_csis[cas] ? (int)esp_ble_audio_csip_set_member_lock(s_csis[cas], lock, force) : ESP_ERR_INVALID_STATE;
}

bool bleAudioCsisIsLocked(bool cas) {
  esp_ble_audio_csip_set_member_set_info_t info;
  return s_csis[cas] && esp_ble_audio_csip_set_member_get_info(s_csis[cas], &info) == ESP_OK && info.locked;
}

#else

int bleAudioCsisRegister(bool cas, const ble_audio_csis_cfg_t *cfg) {
  (void)cas;
  (void)cfg;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCsisSetSirk(bool cas, const uint8_t sirk[BLE_AUDIO_SIRK_LEN]) {
  (void)cas;
  (void)sirk;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCsisSetSizeRank(bool cas, uint8_t size, uint8_t rank) {
  (void)cas;
  (void)size;
  (void)rank;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCsisSetName(bool cas, const uint8_t *name, uint8_t len) {
  (void)cas;
  (void)name;
  (void)len;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCsisGenerateRsi(bool cas, uint8_t rsi[BLE_AUDIO_RSI_LEN]) {
  (void)cas;
  (void)rsi;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCsisLock(bool cas, bool lock, bool force) {
  (void)cas;
  (void)lock;
  (void)force;
  return ESP_ERR_NOT_SUPPORTED;
}
bool bleAudioCsisIsLocked(bool cas) {
  (void)cas;
  return false;
}

#endif /* CSIS_SRV */

/* ── CSIP set coordinator ───────────────────────────────────────────────── */

#if CSIP_CRD

static bool s_crd_registered;
/* Survives deinit: a callback still set in the stack makes a second register fail. */
static bool s_crd_cb_linked;

/**
 * CSIS discovery finished on a peer. The member object is kept for lock and
 * release; the first set's info is reported (set_count 0 when the peer has none).
 */
static void crd_discover(esp_ble_conn_t *conn, const esp_ble_audio_csip_set_coordinator_set_member_t *member, int err, size_t set_count) {
  ble_audio_csip_set_t s = {0};
  cap_peer_t *p = peer_of(conn);
  if (p) {
    p->csip = err ? NULL : member;
  }
  if (err == 0 && member && member->insts && set_count) {
    const esp_ble_audio_csip_set_coordinator_set_info_t *info = &member->insts[0].info;
    s.set_count = (uint8_t)set_count;
    s.set_size = info->set_size;
    s.rank = info->rank;
    s.lockable = info->lockable;
    memcpy(s.sirk, info->sirk, sizeof(s.sirk));
  }
  bleAudioEngineEmit(BLE_AUDIO_EVT_CSIP_DISCOVERED, handle_of(conn), err, &s);
}

/* Ordered lock / release procedures cover the whole set, so they carry no connection. */
static void crd_lock_set(int err) {
  bleAudioEngineEmit(BLE_AUDIO_EVT_CSIP_LOCKED, CONN_NONE, err, NULL);
}

static void crd_release_set(int err) {
  bleAudioEngineEmit(BLE_AUDIO_EVT_CSIP_RELEASED, CONN_NONE, err, NULL);
}

/** A member's lock was notified; find the peer owning @p inst (CONN_NONE if unknown). */
static void crd_lock_changed(esp_ble_audio_csip_set_coordinator_csis_inst_t *inst, bool locked) {
  uint16_t handle = CONN_NONE;
  for (int i = 0; i < MAX_PEERS; i++) {
    if (s_peers[i].used && s_peers[i].csip && s_peers[i].csip->insts == inst) {
      handle = s_peers[i].handle;
    }
  }
  ble_audio_csis_lock_t l = {.locked = locked};
  bleAudioEngineEmit(BLE_AUDIO_EVT_CSIP_LOCK_CHANGED, handle, 0, &l);
}

static esp_ble_audio_csip_set_coordinator_cb_t s_crd_cb = {
  .discover = crd_discover,
  .lock_set = crd_lock_set,
  .release_set = crd_release_set,
  .lock_changed = crd_lock_changed,
};

int bleAudioCsipCoordInit(void) {
  if (s_crd_registered) {
    return 0;
  }
  cap_attach();
  esp_err_t err = esp_ble_audio_csip_set_coordinator_register_cb(&s_crd_cb);
  /* After a re-init the table may still be linked from the previous session; that is fine. */
  if (err != ESP_OK && !s_crd_cb_linked) {
    return (int)err;
  }
  s_crd_cb_linked = true;
  s_crd_registered = true;
  return 0;
}

int bleAudioCsipCoordDiscover(uint16_t conn_handle) {
  if (!s_crd_registered) {
    return ESP_ERR_INVALID_STATE;
  }
  if (peer_get(conn_handle, true) == NULL) {
    return ESP_ERR_NO_MEM;
  }
  return (int)esp_ble_audio_csip_set_coordinator_discover(conn_handle);
}

int bleAudioCsipCoordLock(const uint8_t sirk[BLE_AUDIO_SIRK_LEN], bool lock) {
  /* Collect members of the matching set; the first match supplies the set info the stack needs. */
  const esp_ble_audio_csip_set_coordinator_set_member_t *members[MAX_PEERS];
  const esp_ble_audio_csip_set_coordinator_set_info_t *info = NULL;
  uint8_t n = 0;
  for (int i = 0; i < MAX_PEERS; i++) {
    const esp_ble_audio_csip_set_coordinator_set_member_t *m = s_peers[i].used ? s_peers[i].csip : NULL;
    if (m && m->insts && (sirk == NULL || memcmp(m->insts[0].info.sirk, sirk, BLE_AUDIO_SIRK_LEN) == 0)) {
      if (info == NULL) {
        info = &m->insts[0].info;
      }
      members[n++] = m;
    }
  }
  if (n == 0) {
    return ESP_ERR_INVALID_STATE;
  }
  return (int)(lock ? esp_ble_audio_csip_set_coordinator_lock(members, n, info) : esp_ble_audio_csip_set_coordinator_release(members, n, info));
}

bool bleAudioCsipIsSetMember(const uint8_t sirk[BLE_AUDIO_SIRK_LEN], const uint8_t *adv, uint16_t len) {
  /* Walk the AD structures (length, type, data...) and let the stack test each one; it ignores non-RSI types. */
  while (sirk && adv && len >= 2 && adv[0] >= 1 && adv[0] < len) {
    if (esp_ble_audio_csip_set_coordinator_is_set_member(sirk, adv[1], &adv[2], (uint8_t)(adv[0] - 1))) {
      return true;
    }
    len -= (uint16_t)(adv[0] + 1);
    adv += adv[0] + 1;
  }
  return false;
}

#else

int bleAudioCsipCoordInit(void) {
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCsipCoordDiscover(uint16_t conn_handle) {
  (void)conn_handle;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCsipCoordLock(const uint8_t sirk[BLE_AUDIO_SIRK_LEN], bool lock) {
  (void)sirk;
  (void)lock;
  return ESP_ERR_NOT_SUPPORTED;
}
bool bleAudioCsipIsSetMember(const uint8_t sirk[BLE_AUDIO_SIRK_LEN], const uint8_t *adv, uint16_t len) {
  (void)sirk;
  (void)adv;
  (void)len;
  return false;
}

#endif /* CSIP_CRD */

/* ── CAP initiator: unicast ─────────────────────────────────────────────── */

#if CAP_INIT
static bool s_init_registered;
/* Survives deinit: a callback still set in the stack makes a second register fail. */
static bool s_init_cb_linked;
#endif

#if CAP_UC

#define UC_MAX CONFIG_BT_BAP_UNICAST_CLIENT_GROUP_STREAM_COUNT
#define META_CTX_LEN 4  /* Streaming Audio Contexts LTV: len, type, 2-octet value. */

/**
 * The one unicast group the initiator runs. The parameter arrays are static
 * because the stack keeps pointers into them until the procedure completes.
 */
static struct {
  esp_ble_audio_cap_unicast_group_t *group;  ///< NULL when no group exists.
  uint8_t count;                             ///< Streams in the group.
  ble_audio_slot_t *slots[UC_MAX];           ///< Engine slot per stream.
  uint16_t conns[UC_MAX];                    ///< Peer per stream.
  esp_ble_audio_cap_stream_t *streams[UC_MAX];
  esp_ble_audio_cap_unicast_group_stream_param_t gsp[UC_MAX];
  esp_ble_audio_cap_unicast_group_stream_pair_param_t pairs[UC_MAX];  ///< One pair (CIS) per peer and direction pair.
  esp_ble_audio_cap_unicast_group_param_t gp;
  esp_ble_audio_cap_unicast_audio_start_stream_param_t ssp[UC_MAX];
  esp_ble_audio_cap_unicast_audio_start_param_t sp;
  esp_ble_audio_cap_unicast_audio_update_stream_param_t usp[UC_MAX];
  uint8_t meta[UC_MAX][META_CTX_LEN];       ///< Per-stream context metadata for start/update.
} s_uc;

/** End the discovery chain and report what was found (or @p err). */
static void disc_finish(cap_peer_t *p, int err) {
  p->disc = D_IDLE;
  p->info.eps.sink_eps = p->n_snk;
  p->info.eps.source_eps = p->n_src;
  bleAudioEngineEmit(BLE_AUDIO_EVT_CAP_DISCOVERED, p->handle, err, &p->info);
}

/**
 * Start ASCS discovery for one direction. If it cannot be queued, skip to the
 * next stage: a peer without ASEs of one direction is still usable.
 */
static void disc_ascs(cap_peer_t *p, uint8_t stage) {
  p->disc = stage;
  if (esp_ble_audio_bap_unicast_client_discover(p->handle, stage == D_SNK ? ESP_BLE_AUDIO_DIR_SINK : ESP_BLE_AUDIO_DIR_SOURCE) != ESP_OK) {
    if (stage == D_SNK) {
      disc_ascs(p, D_SRC);
    } else {
      disc_finish(p, 0);
    }
  }
}

/** CAS discovery finished: continue with BASS (handover) or ASCS, or report the failure. */
static void init_discovered(esp_ble_conn_t *conn, int err, const esp_ble_audio_csip_set_coordinator_set_member_t *member,
                            const esp_ble_audio_csip_set_coordinator_csis_inst_t *csis) {
  (void)member;
  cap_peer_t *p = peer_of(conn);
  if (p == NULL || p->disc != D_CAS) {
    return;
  }
  p->info.coordinated = (csis != NULL);
  if (err) {
    disc_finish(p, err);
    return;
  }
#if CAP_HO
  /* Handover adds sources through BASS, which must be discovered beforehand. */
  if (!p->bass) {
    p->disc = D_BASS;
    if (esp_ble_audio_bap_broadcast_assistant_discover(p->handle) == ESP_OK) {
      return;
    }
  }
#endif
  disc_ascs(p, D_SNK);
}

/* Unicast client callbacks also fire for plain BAP procedures; only a CAP discovery in flight is ours. */
static cap_peer_t *ascs_peer(esp_ble_conn_t *conn) {
  cap_peer_t *p = peer_of(conn);
  return (p && (p->disc == D_SNK || p->disc == D_SRC)) ? p : NULL;
}

/* ASCS discovery results, accumulated into the peer until uc_discover closes each direction. */
static void uc_endpoint(esp_ble_conn_t *conn, esp_ble_audio_dir_t dir, esp_ble_audio_bap_ep_t *ep) {
  cap_peer_t *p = ascs_peer(conn);
  if (p == NULL || ep == NULL) {
    return;
  }
  if (dir == ESP_BLE_AUDIO_DIR_SINK && p->n_snk < COUNT_OF(p->snk)) {
    p->snk[p->n_snk++] = ep;
  } else if (dir == ESP_BLE_AUDIO_DIR_SOURCE && p->n_src < COUNT_OF(p->src)) {
    p->src[p->n_src++] = ep;
  }
}

static void uc_location(esp_ble_conn_t *conn, esp_ble_audio_dir_t dir, esp_ble_audio_location_t loc) {
  cap_peer_t *p = ascs_peer(conn);
  if (p) {
    *(dir == ESP_BLE_AUDIO_DIR_SINK ? &p->info.eps.sink_loc : &p->info.eps.source_loc) = (uint32_t)loc;
  }
}

static void uc_contexts(esp_ble_conn_t *conn, esp_ble_audio_context_t snk, esp_ble_audio_context_t src) {
  cap_peer_t *p = ascs_peer(conn);
  if (p) {
    p->info.eps.sink_ctx = (uint16_t)snk;
    p->info.eps.source_ctx = (uint16_t)src;
  }
}

/** One direction's ASCS discovery finished: move from sink to source, then finish. */
static void uc_discover(esp_ble_conn_t *conn, int err, esp_ble_audio_dir_t dir) {
  (void)err; /* a peer without ASEs of one direction reports an error for it */
  cap_peer_t *p = ascs_peer(conn);
  if (p == NULL) {
    return;
  }
  if (p->disc == D_SNK && dir == ESP_BLE_AUDIO_DIR_SINK) {
    disc_ascs(p, D_SRC);
  } else if (p->disc == D_SRC && dir == ESP_BLE_AUDIO_DIR_SOURCE) {
    disc_finish(p, 0);
  }
}

static esp_ble_audio_bap_unicast_client_cb_t s_uc_cb = {
  .location = uc_location,
  .available_contexts = uc_contexts,
  .endpoint = uc_endpoint,
  .discover = uc_discover,
};

/* Fill the group and start parameters from @p reqs; streams sharing a peer pair onto one CIS. */
static int uc_prepare(const ble_audio_uc_stream_req_t *reqs, uint8_t count) {
  if (reqs == NULL || count == 0 || count > UC_MAX) {
    return ESP_ERR_INVALID_ARG;
  }
  memset(s_uc.pairs, 0, sizeof(s_uc.pairs));
  uint8_t np = 0;
  for (uint8_t i = 0; i < count; i++) {
    const ble_audio_uc_stream_req_t *r = &reqs[i];
    cap_peer_t *p = peer_get(r->conn_handle, false);
    const bool tx = (r->dir == BLE_AUDIO_DIR_SINK);
    esp_ble_audio_bap_ep_t *ep = NULL;
    if (p && p->conn) {
      if (tx && r->ep_index < p->n_snk) {
        ep = p->snk[r->ep_index];
      } else if (!tx && r->ep_index < p->n_src) {
        ep = p->src[r->ep_index];
      }
    }
    if (ep == NULL) {
      ESP_LOGE(TAG, "CAP unicast start: request %u names %s endpoint %u of conn %u, which was not discovered", i, tx ? "sink" : "source",
               r->ep_index, r->conn_handle);
      return ESP_ERR_INVALID_ARG;
    }
    esp_ble_audio_codec_cfg_t *cfg = bleAudioStreamSetCodec(r->slot, &r->codec, r->context);
    esp_ble_audio_bap_qos_cfg_t *qos = cfg ? bleAudioStreamSetQos(r->slot, &r->qos) : NULL;
    if (qos == NULL) {
      ESP_LOGE(TAG, "CAP unicast start: request %u has an unusable slot, codec or QoS", i);
      return ESP_ERR_INVALID_ARG;
    }
    bleAudioStreamBind(r->slot, r->conn_handle, tx);
    esp_ble_audio_cap_stream_t *st = bleAudioStreamCap(r->slot);
    s_uc.slots[i] = r->slot;
    s_uc.conns[i] = r->conn_handle;
    s_uc.streams[i] = st;
    s_uc.gsp[i].stream = st;
    s_uc.gsp[i].qos_cfg = qos;
    s_uc.ssp[i].member.member = p->conn;
    s_uc.ssp[i].stream = st;
    s_uc.ssp[i].ep = ep;
    s_uc.ssp[i].codec_cfg = cfg;
    /* Join an existing pair of the same peer whose opposite direction is taken, else open a new pair (CIS). */
    uint8_t j = 0;
    for (; j < np; j++) {
      esp_ble_audio_cap_unicast_group_stream_param_t **mine = tx ? &s_uc.pairs[j].tx_param : &s_uc.pairs[j].rx_param;
      const esp_ble_audio_cap_unicast_group_stream_param_t *other = tx ? s_uc.pairs[j].rx_param : s_uc.pairs[j].tx_param;
      if (*mine == NULL && other && s_uc.conns[other - s_uc.gsp] == r->conn_handle) {
        *mine = &s_uc.gsp[i];
        break;
      }
    }
    if (j == np) {
      *(tx ? &s_uc.pairs[np].tx_param : &s_uc.pairs[np].rx_param) = &s_uc.gsp[i];
      np++;
    }
  }
  s_uc.count = count;
  memset(&s_uc.gp, 0, sizeof(s_uc.gp));
  s_uc.gp.params_count = np;
  s_uc.gp.params = s_uc.pairs;
  s_uc.gp.packing = ESP_BLE_ISO_PACKING_SEQUENTIAL;
  s_uc.sp.type = ESP_BLE_AUDIO_CAP_SET_TYPE_AD_HOC;
  s_uc.sp.count = count;
  s_uc.sp.stream_params = s_uc.ssp;
  return 0;
}

/** Delete the group; on failure keep the pointer so a later Stop can retry. */
static void uc_release_group(void) {
  if (s_uc.group) {
    esp_err_t err = esp_ble_audio_cap_unicast_group_delete(s_uc.group);
    if (err == ESP_OK) {
      s_uc.group = NULL;
    } else {
      ESP_LOGW(TAG, "CAP initiator: unicast group delete failed (%d), keeping it for a retry", err);
    }
  }
  s_uc.count = 0;
}

/**
 * A peer dropped: forget its streams. Once no stream has a peer left the
 * stack cannot run the stop procedure, so the group is deleted here and
 * UC_STOPPED is reported directly.
 */
static void uc_on_disconnect(uint16_t handle) {
  bool left = false;
  for (uint8_t i = 0; i < s_uc.count; i++) {
    if (s_uc.conns[i] == handle) {
      s_uc.conns[i] = CONN_NONE;
    }
    left |= (s_uc.conns[i] != CONN_NONE);
  }
  if (s_uc.count && !left) {
    uc_release_group();
    bleAudioEngineEmit(BLE_AUDIO_EVT_CAP_UC_STOPPED, handle, 0, NULL);
  }
}

/* CAP procedure completions; @p conn is the peer that failed, or NULL on success. */
static void init_uc_started(int err, esp_ble_conn_t *conn) {
  bleAudioEngineEmit(BLE_AUDIO_EVT_CAP_UC_STARTED, handle_of(conn), err, NULL);
}

static void init_uc_updated(int err, esp_ble_conn_t *conn) {
  bleAudioEngineEmit(BLE_AUDIO_EVT_CAP_UC_UPDATED, handle_of(conn), err, NULL);
}

static void init_uc_stopped(int err, esp_ble_conn_t *conn) {
  uc_release_group();
  bleAudioEngineEmit(BLE_AUDIO_EVT_CAP_UC_STOPPED, handle_of(conn), err, NULL);
}

int bleAudioCapDiscover(uint16_t conn_handle) {
  if (!s_init_registered) {
    return ESP_ERR_INVALID_STATE;
  }
  cap_peer_t *p = peer_get(conn_handle, true);
  if (p == NULL) {
    return ESP_ERR_NO_MEM;
  }
  if (p->disc != D_IDLE) {
    return ESP_ERR_INVALID_STATE; /* a discovery of this peer is already running */
  }
  p->n_snk = p->n_src = 0;
  memset(&p->info, 0, sizeof(p->info));
  p->disc = D_CAS;
  esp_err_t err = esp_ble_audio_cap_initiator_unicast_discover(conn_handle);
  if (err != ESP_OK) {
    p->disc = D_IDLE;
  }
  return (int)err;
}

int bleAudioCapUnicastStart(const ble_audio_uc_stream_req_t *reqs, uint8_t count) {
  if (!s_init_registered || s_uc.group) {
    return ESP_ERR_INVALID_STATE;
  }
  int err = uc_prepare(reqs, count);
  if (err == 0) {
    err = esp_ble_audio_cap_unicast_group_create(&s_uc.gp, &s_uc.group);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "CAP unicast start: group create (%u streams) failed: %d", count, err);
      s_uc.group = NULL;
    } else if ((err = esp_ble_audio_cap_initiator_unicast_audio_start(&s_uc.sp)) != ESP_OK) {
      ESP_LOGE(TAG, "CAP unicast start: audio start procedure failed: %d", err);
      uc_release_group();
    }
  }
  if (err) {
    s_uc.count = 0;
  }
  return err;
}

int bleAudioCapUnicastUpdate(uint16_t context) {
  if (s_uc.group == NULL || s_uc.count == 0) {
    return ESP_ERR_INVALID_STATE;
  }
  /* Every stream gets the same Streaming Audio Contexts LTV; 0 maps to Unspecified. */
  for (uint8_t i = 0; i < s_uc.count; i++) {
    s_uc.meta[i][0] = 3;
    s_uc.meta[i][1] = ESP_BLE_AUDIO_METADATA_TYPE_STREAM_CONTEXT;
    audio_put_le16(&s_uc.meta[i][2], context ? context : ESP_BLE_AUDIO_CONTEXT_TYPE_UNSPECIFIED);
    s_uc.usp[i].stream = s_uc.streams[i];
    s_uc.usp[i].meta_len = META_CTX_LEN;
    s_uc.usp[i].meta = s_uc.meta[i];
  }
  esp_ble_audio_cap_unicast_audio_update_param_t param = {
    .type = ESP_BLE_AUDIO_CAP_SET_TYPE_AD_HOC,
    .count = s_uc.count,
    .stream_params = s_uc.usp,
  };
  return (int)esp_ble_audio_cap_initiator_unicast_audio_update(&param);
}

int bleAudioCapUnicastStop(void) {
  if (s_uc.group == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  esp_ble_audio_cap_unicast_audio_stop_param_t param = {
    .type = ESP_BLE_AUDIO_CAP_SET_TYPE_AD_HOC,
    .count = s_uc.count,
    .streams = s_uc.streams,
    .release = true,
  };
  esp_err_t err = s_uc.count ? esp_ble_audio_cap_initiator_unicast_audio_stop(&param) : ESP_FAIL;
  if (err != ESP_OK) {
    /* Nothing left to stop (start failed or streams already released): drop the group. */
    uc_release_group();
    if (s_uc.group) {
      return (int)err;
    }
    bleAudioEngineEmit(BLE_AUDIO_EVT_CAP_UC_STOPPED, CONN_NONE, 0, NULL);
  }
  return 0;
}

#else

int bleAudioCapDiscover(uint16_t conn_handle) {
  (void)conn_handle;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapUnicastStart(const ble_audio_uc_stream_req_t *reqs, uint8_t count) {
  (void)reqs;
  (void)count;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapUnicastUpdate(uint16_t context) {
  (void)context;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapUnicastStop(void) {
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* CAP_UC */

/* ── CAP initiator: broadcast ───────────────────────────────────────────── */

#if CAP_BC

#define BC_MAX CONFIG_BT_BAP_BROADCAST_SRC_STREAM_COUNT

/** The one CAP broadcast source; parameters are static because the stack keeps pointers into them. */
static struct {
  esp_ble_audio_cap_broadcast_source_t *source;  ///< NULL when no source exists.
  bool adv_added;                                ///< adv is registered with the BAP broadcast layer.
  ble_audio_cap_adv_t adv;                       ///< Advertising set the BIG is attached to.
  esp_ble_audio_cap_initiator_broadcast_stream_param_t sp[BC_MAX];
  esp_ble_audio_cap_initiator_broadcast_subgroup_param_t sub;
  esp_ble_audio_cap_initiator_broadcast_create_param_t cp;
  uint8_t bis[BC_MAX][6];                        ///< Per-BIS Audio Location LTV.
#if CAP_HO
  esp_ble_audio_codec_cfg_t cfg;                 ///< Handover: codec copied from the unicast streams.
  uint8_t cfg_data[24];                          ///< Its codec data, without the channel allocation.
  esp_ble_audio_bap_qos_cfg_t qos;               ///< Handover: QoS derived from the unicast streams.
#endif
} s_bc;

/* One subgroup; the per-BIS Audio Location goes in each BIS's codec data. */
static void bc_fill(esp_ble_audio_cap_stream_t *const st[], const uint32_t *loc, uint8_t count, esp_ble_audio_codec_cfg_t *cfg,
                    esp_ble_audio_bap_qos_cfg_t *qos, const uint8_t *code) {
  memset(s_bc.sp, 0, sizeof(s_bc.sp));
  for (uint8_t i = 0; i < count; i++) {
    s_bc.sp[i].stream = st[i];
    if (loc && loc[i]) {
      s_bc.bis[i][0] = 5;
      s_bc.bis[i][1] = ESP_BLE_AUDIO_CODEC_CFG_CHAN_ALLOC;
      audio_put_le32(&s_bc.bis[i][2], loc[i]);
      s_bc.sp[i].data = s_bc.bis[i];
      s_bc.sp[i].data_len = sizeof(s_bc.bis[i]);
    }
  }
  s_bc.sub.stream_count = count;
  s_bc.sub.stream_params = s_bc.sp;
  s_bc.sub.codec_cfg = cfg;
  memset(&s_bc.cp, 0, sizeof(s_bc.cp));
  s_bc.cp.subgroup_count = 1;
  s_bc.cp.subgroup_params = &s_bc.sub;
  s_bc.cp.qos = qos;
  s_bc.cp.packing = ESP_BLE_ISO_PACKING_SEQUENTIAL;
  s_bc.cp.encryption = (code != NULL);
  if (code) {
    memcpy(s_bc.cp.broadcast_code, code, BLE_AUDIO_BCODE_SIZE);
  }
}

/**
 * Register @p adv with the BAP broadcast layer (once; a different handle
 * replaces the previous set) and start following receive states for its
 * Broadcast_ID.
 */
static int bc_adv_add(const ble_audio_cap_adv_t *adv) {
  if (s_bc.adv_added && s_bc.adv.handle != adv->handle) {
    esp_ble_audio_bap_broadcast_adv_info_t old = {.adv_handle = s_bc.adv.handle};
    (void)esp_ble_audio_bap_broadcast_adv_delete(&old);
    s_bc.adv_added = false;
  }
  if (!s_bc.adv_added) {
    esp_ble_audio_bap_broadcast_adv_info_t info = {.adv_handle = adv->handle, .addr_type = adv->addr_type, .sid = adv->sid};
    memcpy(info.addr, adv->addr, sizeof(info.addr));
    esp_err_t err = esp_ble_audio_bap_broadcast_adv_add(&info);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "CAP broadcast: registering advertising set %u failed: %d", adv->handle, err);
      return (int)err;
    }
    s_bc.adv_added = true;
  }
  s_bc.adv = *adv;
#if CAP_BASS
  s_track_id = adv->broadcast_id;
#endif
  return 0;
}

static void init_bc_started(esp_ble_audio_cap_broadcast_source_t *source) {
  (void)source;
  bleAudioEngineEmit(BLE_AUDIO_EVT_CAP_BC_STARTED, CONN_NONE, 0, NULL);
}

/** BIG terminated; @p reason is the HCI reason, reported as the event's err. */
static void init_bc_stopped(esp_ble_audio_cap_broadcast_source_t *source, uint8_t reason) {
  (void)source;
  bleAudioEngineEmit(BLE_AUDIO_EVT_CAP_BC_STOPPED, CONN_NONE, reason, NULL);
}

int bleAudioCapBroadcastCreate(ble_audio_slot_t *const slots[], const uint32_t *locations, uint8_t count, const ble_audio_codec_t *codec,
                               const ble_audio_qos_t *qos, uint16_t context, const uint8_t *code) {
  esp_ble_audio_cap_stream_t *st[BC_MAX];

  if (!s_init_registered || s_bc.source) {
    return ESP_ERR_INVALID_STATE;
  }
  if (slots == NULL || count == 0 || count > BC_MAX || codec == NULL || qos == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  /* With per-BIS locations the subgroup codec must not carry a channel allocation of its own. */
  ble_audio_codec_t sub = *codec;
  if (locations) {
    sub.chan_alloc = 0;
  }
  esp_ble_audio_codec_cfg_t *cfg = slots[0] ? bleAudioStreamSetCodec(slots[0], &sub, context) : NULL;
  if (cfg == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  for (uint8_t i = 0; i < count; i++) {
    if (slots[i] == NULL) {
      return ESP_ERR_INVALID_ARG;
    }
    st[i] = bleAudioStreamCap(slots[i]);
  }
  bc_fill(st, locations, count, cfg, bleAudioStreamSetQos(slots[0], qos), code);
  esp_err_t err = esp_ble_audio_cap_initiator_broadcast_audio_create(&s_bc.cp, &s_bc.source);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "CAP broadcast: source create (%u BIS, %s) failed: %d", count, code ? "encrypted" : "open", err);
    s_bc.source = NULL;
  }
  return (int)err;
}

int bleAudioCapBroadcastGetBase(uint8_t *out, uint16_t cap, uint16_t *out_len) {
  NET_BUF_SIMPLE_DEFINE(base_buf, 128);
  if (s_bc.source == NULL || out == NULL || out_len == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  esp_err_t err = esp_ble_audio_cap_initiator_broadcast_get_base(s_bc.source, &base_buf);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "CAP broadcast: BASE encoding failed: %d", err);
    return (int)err;
  }
  if (base_buf.len > cap) {
    ESP_LOGE(TAG, "CAP broadcast: BASE is %u octets, caller buffer holds %u", base_buf.len, cap);
    return ESP_ERR_NO_MEM;
  }
  memcpy(out, base_buf.data, base_buf.len);
  *out_len = base_buf.len;
  return 0;
}

int bleAudioCapBroadcastStart(const ble_audio_cap_adv_t *adv) {
  if (s_bc.source == NULL || adv == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  int err = bc_adv_add(adv);
  return err ? err : (int)esp_ble_audio_cap_initiator_broadcast_audio_start(s_bc.source, adv->handle);
}

int bleAudioCapBroadcastUpdate(uint16_t context) {
  /* Streaming Audio Contexts LTV; 0 maps to Unspecified. */
  uint8_t meta[4] = {3, ESP_BLE_AUDIO_METADATA_TYPE_STREAM_CONTEXT};
  if (s_bc.source == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  audio_put_le16(&meta[2], context ? context : ESP_BLE_AUDIO_CONTEXT_TYPE_UNSPECIFIED);
  return (int)esp_ble_audio_cap_initiator_broadcast_audio_update(s_bc.source, meta, sizeof(meta));
}

int bleAudioCapBroadcastStop(void) {
  return s_bc.source ? (int)esp_ble_audio_cap_initiator_broadcast_audio_stop(s_bc.source) : ESP_ERR_INVALID_STATE;
}

void bleAudioCapBroadcastDelete(void) {
  if (s_bc.source) {
    esp_err_t err = esp_ble_audio_cap_initiator_broadcast_audio_delete(s_bc.source);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "CAP broadcast: source delete refused (%d); the broadcast must be stopped first", err);
      return;
    }
    s_bc.source = NULL;
  }
  if (s_bc.adv_added) {
    esp_ble_audio_bap_broadcast_adv_info_t info = {.adv_handle = s_bc.adv.handle};
    (void)esp_ble_audio_bap_broadcast_adv_delete(&info);
    s_bc.adv_added = false;
  }
}

#else

int bleAudioCapBroadcastCreate(ble_audio_slot_t *const slots[], const uint32_t *locations, uint8_t count, const ble_audio_codec_t *codec,
                               const ble_audio_qos_t *qos, uint16_t context, const uint8_t *code) {
  (void)slots;
  (void)locations;
  (void)count;
  (void)codec;
  (void)qos;
  (void)context;
  (void)code;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapBroadcastGetBase(uint8_t *out, uint16_t cap, uint16_t *out_len) {
  (void)out;
  (void)cap;
  (void)out_len;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapBroadcastStart(const ble_audio_cap_adv_t *adv) {
  (void)adv;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapBroadcastUpdate(uint16_t context) {
  (void)context;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapBroadcastStop(void) {
  return ESP_ERR_NOT_SUPPORTED;
}
void bleAudioCapBroadcastDelete(void) {}

#endif /* CAP_BC */

#if CAP_INIT

static esp_ble_audio_cap_initiator_cb_t s_init_cb = {
#if CAP_UC
  .unicast_discovery_complete = init_discovered,
  .unicast_start_complete = init_uc_started,
  .unicast_update_complete = init_uc_updated,
  .unicast_stop_complete = init_uc_stopped,
#endif
#if CAP_BC
  .broadcast_started = init_bc_started,
  .broadcast_stopped = init_bc_stopped,
#endif
};

int bleAudioCapInitiatorInit(void) {
  if (s_init_registered) {
    return 0;
  }
  cap_attach();
  esp_err_t err = esp_ble_audio_cap_initiator_register_cb(&s_init_cb);
#if CAP_UC
  /* ASCS discovery results come through the BAP unicast client callbacks (filtered by ascs_peer). */
  if (err == ESP_OK) {
    err = esp_ble_audio_bap_unicast_client_register_cb(&s_uc_cb);
  }
#endif
  /* After a re-init the tables may still be linked from the previous session; that is fine. */
  if (err != ESP_OK && !s_init_cb_linked) {
    ESP_LOGE(TAG, "CAP initiator: callback registration failed: %d", err);
    return (int)err;
  }
  s_init_cb_linked = true;
  s_init_registered = true;
  return 0;
}

#else

int bleAudioCapInitiatorInit(void) {
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* CAP_INIT */

/* ── CAP handover ───────────────────────────────────────────────────────── */

#if CAP_HO

static bool s_ho_registered;
/* Survives deinit: a callback still set in the stack makes a second register fail. */
static bool s_ho_cb_linked;

/*
 * Handover reuses s_uc and s_bc: the stack creates the broadcast source (or
 * the unicast group) itself and hands the new object back in the completion
 * callbacks, which store it where the regular initiator paths expect it.
 */

/** Copy LTV data without the entries of @p type (entries that do not fit in @p cap are dropped). */
static size_t ltv_without(uint8_t *dst, size_t cap, const uint8_t *src, size_t len, uint8_t type) {
  size_t n = 0;
  while (len >= 1 && (size_t)src[0] + 1 <= len) {
    const size_t l = (size_t)src[0] + 1;
    if ((src[0] == 0 || src[1] != type) && n + l <= cap) {
      memcpy(dst + n, src, l);
      n += l;
    }
    src += l;
    len -= l;
  }
  return n;
}

/** Unicast-to-broadcast: the source exists; the application must now publish its BASE. */
static void ho_created(esp_ble_audio_cap_broadcast_source_t *source) {
  s_bc.source = source;
  bleAudioEngineEmit(BLE_AUDIO_EVT_CAP_HO_CREATED, CONN_NONE, 0, NULL);
}

/** Unicast-to-broadcast finished; @p group / @p source are what the stack still holds. */
static void ho_to_broadcast(int err, esp_ble_conn_t *conn, esp_ble_audio_cap_unicast_group_t *group, esp_ble_audio_cap_broadcast_source_t *source) {
  s_uc.group = group;
  s_bc.source = source;
  if (err == 0) {
    /* Released streams cannot join the group b2u creates while this one holds them. */
    uc_release_group();
  }
  bleAudioEngineEmit(BLE_AUDIO_EVT_CAP_HO_TO_BROADCAST, handle_of(conn), err, NULL);
}

/** Broadcast-to-unicast finished; a NULL @p group means no unicast streams were started. */
static void ho_to_unicast(int err, esp_ble_conn_t *conn, esp_ble_audio_cap_broadcast_source_t *source, esp_ble_audio_cap_unicast_group_t *group) {
  s_bc.source = source;
  s_uc.group = group;
  if (group == NULL) {
    s_uc.count = 0;
  }
  bleAudioEngineEmit(BLE_AUDIO_EVT_CAP_HO_TO_UNICAST, handle_of(conn), err, NULL);
}

static esp_ble_audio_cap_handover_cb_t s_ho_cb = {
  .unicast_to_broadcast_created = ho_created,
  .unicast_to_broadcast_complete = ho_to_broadcast,
  .broadcast_to_unicast_complete = ho_to_unicast,
};

int bleAudioCapHandoverInit(void) {
  if (s_ho_registered) {
    return 0;
  }
  cap_attach();
  int err = bass_attach();
  if (err == 0) {
    err = (int)esp_ble_audio_cap_handover_register_cb(&s_ho_cb);
    /* After a re-init the table may still be linked from the previous session; that is fine. */
    if (err == 0 || s_ho_cb_linked) {
      s_ho_cb_linked = true;
      err = 0;
    }
  }
  s_ho_registered = (err == 0);
  return err;
}

int bleAudioCapHandoverToBroadcast(const ble_audio_cap_adv_t *adv, const ble_audio_qos_t *qos, const uint8_t *code) {
  /* Static: the stack references it until the procedure completes. */
  static esp_ble_audio_cap_handover_unicast_to_broadcast_param_t param;
  esp_ble_audio_cap_stream_t *st[BC_MAX];
  uint32_t loc[BC_MAX];
  uint8_t n = 0;

  if (adv == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (!s_ho_registered || s_uc.group == NULL || s_bc.source) {
    return ESP_ERR_INVALID_STATE;
  }
  /* Only streaming sink (our TX) streams move; each becomes one BIS keeping its Audio Location. */
  for (uint8_t i = 0; i < s_uc.count && n < BC_MAX; i++) {
    ble_audio_slot_t *slot = s_uc.slots[i];
    ble_audio_codec_t c;
    if (!bleAudioStreamIsTx(slot) || !bleAudioStreamIsStreaming(slot)) {
      continue;
    }
    loc[n] = bleAudioStreamGetCodec(slot, &c) ? c.chan_alloc : 0;
    st[n++] = bleAudioStreamCap(slot);
  }
  if (n == 0) {
    ESP_LOGE(TAG, "CAP handover to broadcast: no streaming sink stream in the unicast group");
    return ESP_ERR_INVALID_STATE;
  }
  const esp_ble_audio_bap_stream_t *b0 = &st[0]->bap_stream;
  if (b0->codec_cfg == NULL || b0->qos == NULL) {
    return ESP_ERR_INVALID_STATE;
  }
  /* Same codec as the unicast streams, so acceptors do not reconfigure their decoder. */
  s_bc.cfg = *b0->codec_cfg;
  s_bc.cfg.data = s_bc.cfg_data;
  s_bc.cfg.data_len = ltv_without(s_bc.cfg_data, sizeof(s_bc.cfg_data), b0->codec_cfg->data, b0->codec_cfg->data_len, ESP_BLE_AUDIO_CODEC_CFG_CHAN_ALLOC);
  s_bc.qos = *b0->qos;
  if (qos) {
    s_bc.qos.rtn = qos->rtn;
    s_bc.qos.latency = qos->latency_ms;
  }
  bc_fill(st, loc, n, &s_bc.cfg, &s_bc.qos, code);
  int err = bc_adv_add(adv);
  if (err) {
    return err;
  }
  memset(&param, 0, sizeof(param));
  param.type = ESP_BLE_AUDIO_CAP_SET_TYPE_AD_HOC;
  param.unicast_group = s_uc.group;
  param.adv_handle = adv->handle;
  param.pa_interval = adv->pa_interval;
  param.broadcast_id = adv->broadcast_id;
  param.broadcast_create_param = &s_bc.cp;
  err = (int)esp_ble_audio_cap_handover_unicast_to_broadcast(&param);
  if (err) {
    ESP_LOGE(TAG, "CAP handover to broadcast: procedure start (%u streams) failed: %d", n, err);
  }
  return err;
}

int bleAudioCapHandoverToUnicast(const ble_audio_uc_stream_req_t *reqs, uint8_t count) {
  /* Static: the stack references them until the procedure completes. */
  static esp_ble_audio_cap_commander_broadcast_reception_stop_member_param_t stop_members[MAX_PEERS];
  static esp_ble_audio_cap_commander_broadcast_reception_stop_param_t stop;
  static esp_ble_audio_cap_handover_broadcast_to_unicast_param_t param;

  if (!s_ho_registered || s_bc.source == NULL || s_uc.group) {
    return ESP_ERR_INVALID_STATE;
  }
  int err = uc_prepare(reqs, count);
  if (err) {
    return err;
  }
  /* Acceptors still receiving our broadcast are told to stop before the unicast streams start. */
  uint8_t ns = 0;
  for (int i = 0; i < MAX_PEERS; i++) {
    const cap_peer_t *p = &s_peers[i];
    if (p->used && p->conn && p->src_valid) {
      stop_members[ns].member.member = p->conn;
      stop_members[ns].src_id = p->src_id;
      stop_members[ns].num_subgroups = 1;
      ns++;
    }
  }
  stop.type = ESP_BLE_AUDIO_CAP_SET_TYPE_AD_HOC;
  stop.param = stop_members;
  stop.count = ns;
  memset(&param, 0, sizeof(param));
  param.reception_stop_param = ns ? &stop : NULL;
  /* Required even with reception_stop_param: receive states are matched on this triple. */
  param.broadcast_id = s_bc.adv.broadcast_id;
  param.adv_sid = s_bc.adv.sid;
  param.adv_type = s_bc.adv.addr_type;
  param.broadcast_source = s_bc.source;
  param.unicast_group_param = &s_uc.gp;
  param.unicast_start_param = &s_uc.sp;
  err = esp_ble_audio_cap_handover_broadcast_to_unicast(&param);
  if (err) {
    ESP_LOGE(TAG, "CAP handover to unicast: procedure start (%u streams, %u receivers) failed: %d", count, ns, err);
    s_uc.count = 0;
  }
  return err;
}

#else

int bleAudioCapHandoverInit(void) {
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapHandoverToBroadcast(const ble_audio_cap_adv_t *adv, const ble_audio_qos_t *qos, const uint8_t *code) {
  (void)adv;
  (void)qos;
  (void)code;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapHandoverToUnicast(const ble_audio_uc_stream_req_t *reqs, uint8_t count) {
  (void)reqs;
  (void)count;
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* CAP_HO */

/* ── CAP commander ──────────────────────────────────────────────────────── */

#if CAP_CMD

static bool s_cmd_registered;
/* Survives deinit: a callback still set in the stack makes a second register fail. */
static bool s_cmd_cb_linked;

/** End the commander discovery of @p p and report it. */
static void cmd_disc_done(cap_peer_t *p, int err) {
  ble_audio_cap_cmd_discovered_t d = {.coordinated = p->cmd_coord};
#if CAP_BASS
  d.bass = p->bass;
#endif
  p->cmd_disc = false;
  bleAudioEngineEmit(BLE_AUDIO_EVT_CAP_CMD_DISCOVERED, p->handle, err, &d);
}

/** Steps of the commander discovery that follow CAS, in order. */
enum { CMD_D_CAS = 0, CMD_D_VCP, CMD_D_MICP, CMD_D_BASS };

/**
 * Start the next discovery step after CAS, or finish. The stack refuses the
 * volume procedures for a member without a VCP controller instance and the
 * microphone ones without a MICP one, so both are discovered here unless the
 * application already did it on this link (BLEAudioVolumeController /
 * BLEAudioMicController). A step that cannot start or fails only limits the
 * procedures, like a missing BASS.
 */
static void cmd_disc_step(cap_peer_t *p) {
#if CMD_VCP
  if (p->cmd_stage < CMD_D_VCP) {
    p->cmd_stage = CMD_D_VCP;
    esp_ble_audio_vcp_vol_ctlr_t *ctlr = NULL;
    if (esp_ble_audio_vcp_vol_ctlr_get_by_conn(p->handle) == NULL && esp_ble_audio_vcp_vol_ctlr_discover(p->handle, &ctlr) == ESP_OK) {
      p->cmd_ctlr = ctlr;
      return;
    }
  }
#endif
#if CMD_MICP
  if (p->cmd_stage < CMD_D_MICP) {
    p->cmd_stage = CMD_D_MICP;
    esp_ble_audio_micp_mic_ctlr_t *ctlr = NULL;
    if (esp_ble_audio_micp_mic_ctlr_get_by_conn(p->handle) == NULL && esp_ble_audio_micp_mic_ctlr_discover(p->handle, &ctlr) == ESP_OK) {
      p->cmd_ctlr = ctlr;
      return;
    }
  }
#endif
#if CAP_BASS
  if (p->cmd_stage < CMD_D_BASS) {
    p->cmd_stage = CMD_D_BASS;
    if (!p->bass && esp_ble_audio_bap_broadcast_assistant_discover(p->handle) == ESP_OK) {
      return;  // bass_discover resumes here
    }
  }
#endif
  p->cmd_ctlr = NULL;
  cmd_disc_done(p, 0);
}

/**
 * CAS discovery finished: the peer joins the commander's set; continue with
 * the controller and BASS discoveries (cmd_disc_step).
 */
static void cmd_discovered(esp_ble_conn_t *conn, int err, const esp_ble_audio_csip_set_coordinator_set_member_t *member,
                           const esp_ble_audio_csip_set_coordinator_csis_inst_t *csis) {
  (void)member;
  cap_peer_t *p = peer_of(conn);
  if (p == NULL || !p->cmd_disc) {
    return;
  }
  p->cmd_coord = (csis != NULL);
  p->cmd_ready = (err == 0);
  if (err != 0) {
    cmd_disc_done(p, err);
    return;
  }
  p->cmd_stage = CMD_D_CAS;
  cmd_disc_step(p);
}

#if CMD_VCP || CMD_MICP
/** Resume the commander discovery waiting on controller instance @p ctlr (other discoveries are ignored). */
static void cmd_ctlr_discovered(const void *ctlr) {
  for (int i = 0; i < MAX_PEERS; i++) {
    cap_peer_t *p = &s_peers[i];
    if (p->used && p->cmd_disc && p->cmd_ctlr == ctlr) {
      p->cmd_ctlr = NULL;
      cmd_disc_step(p);
      return;
    }
  }
}
#endif

#if CMD_VCP
static void cmd_vcp_discovered(esp_ble_audio_vcp_vol_ctlr_t *ctlr, int err, uint8_t vocs_count, uint8_t aics_count) {
  (void)err;
  (void)vocs_count;
  (void)aics_count;
  cmd_ctlr_discovered(ctlr);
}

/* A second listener next to the VCP controller role's; the stack calls every registered table. */
static esp_ble_audio_vcp_vol_ctlr_cb_t s_cmd_vcp_cb = {.discover = cmd_vcp_discovered};
#endif

#if CMD_MICP
static void cmd_micp_discovered(esp_ble_audio_micp_mic_ctlr_t *ctlr, int err, uint8_t aics_count) {
  (void)err;
  (void)aics_count;
  cmd_ctlr_discovered(ctlr);
}

/* A second listener next to the MICP controller role's; the stack calls every registered table. */
static esp_ble_audio_micp_mic_ctlr_cb_t s_cmd_micp_cb = {.discover = cmd_micp_discovered};
#endif

/** One procedure finished (@p conn is the failing member, or NULL); the per-procedure callbacks below funnel here. */
static void cmd_done(uint8_t op, esp_ble_conn_t *conn, int err) {
  ble_audio_cap_cmd_done_t d = {.op = op};
  bleAudioEngineEmit(BLE_AUDIO_EVT_CAP_CMD_DONE, handle_of(conn), err, &d);
}

static void cmd_volume(esp_ble_conn_t *conn, int err) {
  cmd_done(BLE_AUDIO_CAP_OP_VOLUME, conn, err);
}
static void cmd_volume_mute(esp_ble_conn_t *conn, int err) {
  cmd_done(BLE_AUDIO_CAP_OP_VOLUME_MUTE, conn, err);
}
static void cmd_volume_offset(esp_ble_conn_t *conn, int err) {
  cmd_done(BLE_AUDIO_CAP_OP_VOLUME_OFFSET, conn, err);
}
static void cmd_mic_mute(esp_ble_conn_t *conn, int err) {
  cmd_done(BLE_AUDIO_CAP_OP_MIC_MUTE, conn, err);
}
static void cmd_mic_gain(esp_ble_conn_t *conn, int err) {
  cmd_done(BLE_AUDIO_CAP_OP_MIC_GAIN, conn, err);
}
static void cmd_rx_start(esp_ble_conn_t *conn, int err) {
  cmd_done(BLE_AUDIO_CAP_OP_RECEPTION_START, conn, err);
}
static void cmd_rx_stop(esp_ble_conn_t *conn, int err) {
  cmd_done(BLE_AUDIO_CAP_OP_RECEPTION_STOP, conn, err);
}
static void cmd_code(esp_ble_conn_t *conn, int err) {
  cmd_done(BLE_AUDIO_CAP_OP_BROADCAST_CODE, conn, err);
}

static esp_ble_audio_cap_commander_cb_t s_cmd_cb = {
  .discovery_complete = cmd_discovered,
  .volume_changed = cmd_volume,
  .volume_mute_changed = cmd_volume_mute,
  .volume_offset_changed = cmd_volume_offset,
  .microphone_mute_changed = cmd_mic_mute,
  .microphone_gain_changed = cmd_mic_gain,
  .broadcast_reception_start = cmd_rx_start,
  .broadcast_reception_stop = cmd_rx_stop,
  .distribute_broadcast_code = cmd_code,
};

/* Ad-hoc set of every peer the commander discovered (and, if @p need_src, that holds our receive state). */
static uint8_t cmd_members(esp_ble_conn_t **out, uint8_t *src_ids, bool need_src) {
  uint8_t n = 0;
  for (int i = 0; i < MAX_PEERS; i++) {
    const cap_peer_t *p = &s_peers[i];
    if (!p->used || !p->cmd_ready || p->conn == NULL) {
      continue;
    }
#if CAP_BASS
    if (need_src && !p->src_valid) {
      continue;
    }
    if (src_ids) {
      src_ids[n] = p->src_id;
    }
#else
    (void)src_ids;
    if (need_src) {
      continue;
    }
#endif
    out[n++] = p->conn;
  }
  return n;
}

int bleAudioCapCommanderInit(void) {
  if (s_cmd_registered) {
    return 0;
  }
  cap_attach();
  int err = (int)esp_ble_audio_cap_commander_register_cb(&s_cmd_cb);
  /* After a re-init the table may still be linked from the previous session; that is fine. */
  if (err == 0 || s_cmd_cb_linked) {
    s_cmd_cb_linked = true;
    err = 0;
  }
  /* These fail only when the static tables are still listed from an earlier session. */
#if CMD_VCP
  (void)esp_ble_audio_vcp_vol_ctlr_cb_register(&s_cmd_vcp_cb);
#endif
#if CMD_MICP
  (void)esp_ble_audio_micp_mic_ctlr_cb_register(&s_cmd_micp_cb);
#endif
#if CAP_BASS
  /* Reception procedures need BASS discovery and receive-state tracking. */
  if (err == 0) {
    err = bass_attach();
  }
#endif
  s_cmd_registered = (err == 0);
  return err;
}

int bleAudioCapCommanderDiscover(uint16_t conn_handle) {
  if (!s_cmd_registered) {
    return ESP_ERR_INVALID_STATE;
  }
  cap_peer_t *p = peer_get(conn_handle, true);
  if (p == NULL) {
    return ESP_ERR_NO_MEM;
  }
  p->cmd_disc = true;
  esp_err_t err = esp_ble_audio_cap_commander_discover(conn_handle);
  if (err != ESP_OK) {
    p->cmd_disc = false;
  }
  return (int)err;
}

/*
 * The commander procedures below share one shape: collect the ready peers
 * as an ad-hoc set, fill the per-member parameters, and start the CAP
 * procedure (ESP_ERR_INVALID_STATE when no peer qualifies). Each depends on
 * the matching client role being compiled into the stack.
 */

int bleAudioCapCommanderVolume(uint8_t volume) {
#if BLE_AUDIO_VCP_CONTROLLER_SUPPORTED
  esp_ble_conn_t *c[MAX_PEERS];
  esp_ble_audio_cap_set_member_t m[MAX_PEERS];
  const uint8_t n = cmd_members(c, NULL, false);
  for (uint8_t i = 0; i < n; i++) {
    m[i].member = c[i];
  }
  esp_ble_audio_cap_commander_change_volume_param_t param = {.type = ESP_BLE_AUDIO_CAP_SET_TYPE_AD_HOC, .members = m, .count = n, .volume = volume};
  return n ? (int)esp_ble_audio_cap_commander_change_volume(&param) : ESP_ERR_INVALID_STATE;
#else
  (void)volume;
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

int bleAudioCapCommanderVolumeMute(bool mute) {
#if BLE_AUDIO_VCP_CONTROLLER_SUPPORTED
  esp_ble_conn_t *c[MAX_PEERS];
  esp_ble_audio_cap_set_member_t m[MAX_PEERS];
  const uint8_t n = cmd_members(c, NULL, false);
  for (uint8_t i = 0; i < n; i++) {
    m[i].member = c[i];
  }
  esp_ble_audio_cap_commander_change_volume_mute_state_param_t param = {.type = ESP_BLE_AUDIO_CAP_SET_TYPE_AD_HOC, .members = m, .count = n, .mute = mute};
  return n ? (int)esp_ble_audio_cap_commander_change_volume_mute_state(&param) : ESP_ERR_INVALID_STATE;
#else
  (void)mute;
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

int bleAudioCapCommanderVolumeOffset(int16_t offset) {
#if BLE_AUDIO_VCP_CONTROLLER_SUPPORTED && defined(CONFIG_BT_VCP_VOL_CTLR_VOCS)
  esp_ble_conn_t *c[MAX_PEERS];
  esp_ble_audio_cap_commander_change_volume_offset_member_param_t m[MAX_PEERS];
  const uint8_t n = cmd_members(c, NULL, false);
  for (uint8_t i = 0; i < n; i++) {
    m[i].member.member = c[i];
    m[i].offset = offset;
  }
  esp_ble_audio_cap_commander_change_volume_offset_param_t param = {.type = ESP_BLE_AUDIO_CAP_SET_TYPE_AD_HOC, .param = m, .count = n};
  return n ? (int)esp_ble_audio_cap_commander_change_volume_offset(&param) : ESP_ERR_INVALID_STATE;
#else
  (void)offset;
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

int bleAudioCapCommanderMicMute(bool mute) {
#if BLE_AUDIO_MICP_CONTROLLER_SUPPORTED
  esp_ble_conn_t *c[MAX_PEERS];
  esp_ble_audio_cap_set_member_t m[MAX_PEERS];
  const uint8_t n = cmd_members(c, NULL, false);
  for (uint8_t i = 0; i < n; i++) {
    m[i].member = c[i];
  }
  esp_ble_audio_cap_commander_change_microphone_mute_state_param_t param = {.type = ESP_BLE_AUDIO_CAP_SET_TYPE_AD_HOC, .members = m, .count = n, .mute = mute};
  return n ? (int)esp_ble_audio_cap_commander_change_microphone_mute_state(&param) : ESP_ERR_INVALID_STATE;
#else
  (void)mute;
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

int bleAudioCapCommanderMicGain(int8_t gain) {
#if BLE_AUDIO_MICP_CONTROLLER_SUPPORTED && defined(CONFIG_BT_MICP_MIC_CTLR_AICS)
  esp_ble_conn_t *c[MAX_PEERS];
  esp_ble_audio_cap_commander_change_microphone_gain_setting_member_param_t m[MAX_PEERS];
  const uint8_t n = cmd_members(c, NULL, false);
  for (uint8_t i = 0; i < n; i++) {
    m[i].member.member = c[i];
    m[i].gain = gain;
  }
  esp_ble_audio_cap_commander_change_microphone_gain_setting_param_t param = {.type = ESP_BLE_AUDIO_CAP_SET_TYPE_AD_HOC, .param = m, .count = n};
  return n ? (int)esp_ble_audio_cap_commander_change_microphone_gain_setting(&param) : ESP_ERR_INVALID_STATE;
#else
  (void)gain;
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

int bleAudioCapCommanderReceptionStart(const ble_audio_cap_bcast_src_t *src) {
#if CAP_BASS
  /* One subgroup shared by all members; static because the stack references it during the procedure. */
  static esp_ble_audio_bap_bass_subgroup_t subgroup;
  esp_ble_conn_t *c[MAX_PEERS];
  esp_ble_audio_cap_commander_broadcast_reception_start_member_param_t m[MAX_PEERS];
  if (src == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  const uint8_t n = cmd_members(c, NULL, false);
  subgroup.bis_sync = src->bis_sync ? src->bis_sync : ESP_BLE_AUDIO_BAP_BIS_SYNC_NO_PREF;
  subgroup.metadata_len = 0;
  subgroup.metadata = NULL;
  memset(m, 0, sizeof(m));
  for (uint8_t i = 0; i < n; i++) {
    m[i].member.member = c[i];
    m[i].addr.type = src->addr_type;
    memcpy(m[i].addr.a.val, src->addr, sizeof(m[i].addr.a.val));
    m[i].adv_sid = src->sid;
    m[i].pa_interval = src->pa_interval;
    m[i].broadcast_id = src->broadcast_id;
    m[i].subgroups = &subgroup;
    m[i].num_subgroups = 1;
  }
  esp_ble_audio_cap_commander_broadcast_reception_start_param_t param = {.type = ESP_BLE_AUDIO_CAP_SET_TYPE_AD_HOC, .param = m, .count = n};
  if (n == 0) {
    return ESP_ERR_INVALID_STATE;
  }
  /* Follow receive states for this broadcast so Stop and DistributeCode know each peer's Source_ID. */
  s_track_id = src->broadcast_id;
  return (int)esp_ble_audio_cap_commander_broadcast_reception_start(&param);
#else
  (void)src;
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

int bleAudioCapCommanderReceptionStop(void) {
#if CAP_BASS
  esp_ble_conn_t *c[MAX_PEERS];
  uint8_t ids[MAX_PEERS];
  esp_ble_audio_cap_commander_broadcast_reception_stop_member_param_t m[MAX_PEERS];
  const uint8_t n = cmd_members(c, ids, true);
  for (uint8_t i = 0; i < n; i++) {
    m[i].member.member = c[i];
    m[i].src_id = ids[i];
    m[i].num_subgroups = 1;
  }
  esp_ble_audio_cap_commander_broadcast_reception_stop_param_t param = {.type = ESP_BLE_AUDIO_CAP_SET_TYPE_AD_HOC, .param = m, .count = n};
  return n ? (int)esp_ble_audio_cap_commander_broadcast_reception_stop(&param) : ESP_ERR_INVALID_STATE;
#else
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

int bleAudioCapCommanderDistributeCode(const uint8_t code[BLE_AUDIO_BCODE_SIZE]) {
#if CAP_BASS
  esp_ble_conn_t *c[MAX_PEERS];
  uint8_t ids[MAX_PEERS];
  esp_ble_audio_cap_commander_distribute_broadcast_code_member_param_t m[MAX_PEERS];
  esp_ble_audio_cap_commander_distribute_broadcast_code_param_t param = {.type = ESP_BLE_AUDIO_CAP_SET_TYPE_AD_HOC, .param = m};
  if (code == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  param.count = cmd_members(c, ids, true);
  for (uint8_t i = 0; i < param.count; i++) {
    m[i].member.member = c[i];
    m[i].src_id = ids[i];
  }
  memcpy(param.broadcast_code, code, BLE_AUDIO_BCODE_SIZE);
  return param.count ? (int)esp_ble_audio_cap_commander_distribute_broadcast_code(&param) : ESP_ERR_INVALID_STATE;
#else
  (void)code;
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

#else

int bleAudioCapCommanderInit(void) {
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapCommanderDiscover(uint16_t conn_handle) {
  (void)conn_handle;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapCommanderVolume(uint8_t volume) {
  (void)volume;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapCommanderVolumeMute(bool mute) {
  (void)mute;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapCommanderVolumeOffset(int16_t offset) {
  (void)offset;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapCommanderMicMute(bool mute) {
  (void)mute;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapCommanderMicGain(int8_t gain) {
  (void)gain;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapCommanderReceptionStart(const ble_audio_cap_bcast_src_t *src) {
  (void)src;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapCommanderReceptionStop(void) {
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioCapCommanderDistributeCode(const uint8_t code[BLE_AUDIO_BCODE_SIZE]) {
  (void)code;
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* CAP_CMD */

/* ── Broadcast assistant callbacks (commander / handover) ───────────────── */

#if CAP_BASS

/*
 * One BASS client callback table serves both the commander and handover;
 * the peer's discovery flags tell which chain a discovery belongs to.
 */

static bool s_bass_registered;
/* Survives deinit: a callback still set in the stack makes a second register fail. */
static bool s_bass_cb_linked;

/** BASS discovery finished: resume the initiator chain (ASCS) and/or finish the commander discovery. */
static void bass_discover(esp_ble_conn_t *conn, int err, uint8_t recv_state_count) {
  (void)recv_state_count;
  cap_peer_t *p = peer_of(conn);
  if (p == NULL) {
    return;
  }
  p->bass = (err == 0);
#if CAP_HO
  if (p->disc == D_BASS) {
    disc_ascs(p, D_SNK);
  }
#endif
#if CAP_CMD
  /* CAS succeeded; a missing BASS only limits the procedures (reported as bass = false). */
  if (p->cmd_disc && p->cmd_stage == CMD_D_BASS) {
    cmd_disc_step(p);
  }
#endif
}

/**
 * A peer's receive state was read or notified. For our tracked broadcast,
 * remember its Source_ID and forward a PAST request when the peer asks for
 * sync info; the commander also gets every receive state as is.
 */
static void bass_recv_state(esp_ble_conn_t *conn, int err, const esp_ble_audio_bap_scan_delegator_recv_state_t *state) {
  cap_peer_t *p = peer_of(conn);
  if (p == NULL || err || state == NULL) {
    return;
  }
  if (state->broadcast_id == s_track_id) {
    p->src_id = state->src_id;
    p->src_valid = true;
#if CAP_PAST
    if (state->pa_sync_state == ESP_BLE_AUDIO_BAP_PA_STATE_INFO_REQ) {
      ble_audio_cap_past_req_t r = {.src_id = state->src_id, .addr_type = p->addr_type};
      memcpy(r.addr, p->addr, sizeof(r.addr));
      bleAudioEngineEmit(BLE_AUDIO_EVT_CAP_PAST_REQ, p->handle, 0, &r);
    }
#endif
  }
#if CAP_CMD
  ble_audio_cap_recv_state_t rs = {
    .src_id = state->src_id,
    .pa_state = (uint8_t)state->pa_sync_state,
    .big_enc = (uint8_t)state->encrypt_state,
    .broadcast_id = state->broadcast_id,
  };
  for (uint8_t i = 0; i < state->num_subgroups; i++) {
    rs.bis_sync |= state->subgroups[i].bis_sync;
  }
  bleAudioEngineEmit(BLE_AUDIO_EVT_CAP_CMD_RECV_STATE, p->handle, 0, &rs);
#endif
}

/** The peer dropped a source; forget it if it was ours. */
static void bass_recv_state_removed(esp_ble_conn_t *conn, uint8_t src_id) {
  cap_peer_t *p = peer_of(conn);
  if (p && p->src_valid && p->src_id == src_id) {
    p->src_valid = false;
  }
}

static esp_ble_audio_bap_broadcast_assistant_cb_t s_bass_cb = {
  .discover = bass_discover,
  .recv_state = bass_recv_state,
  .recv_state_removed = bass_recv_state_removed,
};

/** Register the shared BASS client callbacks once per session. */
static int bass_attach(void) {
  if (s_bass_registered) {
    return 0;
  }
  esp_err_t err = esp_ble_audio_bap_broadcast_assistant_register_cb(&s_bass_cb);
  /* After a re-init the table may still be linked from the previous session; that is fine. */
  if (err != ESP_OK && !s_bass_cb_linked) {
    return (int)err;
  }
  s_bass_cb_linked = true;
  s_bass_registered = true;
  return 0;
}

#endif /* CAP_BASS */

/* ── Hooks ──────────────────────────────────────────────────────────────── */

#if CAP_ANY

/**
 * Engine GAP hook. Disconnect: drop the peer (and its unicast streams).
 * Connect (PAST builds only): record the peer address, which PAST requests
 * carry for hosts that address the transfer by peer.
 */
static void cap_on_gap(const void *event) {
#if CAP_PEERS
  const esp_ble_audio_gap_app_event_t *ev = (const esp_ble_audio_gap_app_event_t *)event;
  if (ev->type == ESP_BLE_AUDIO_GAP_EVENT_ACL_DISCONNECT) {
    const uint16_t handle = ev->acl_disconnect.conn_handle;
#if CAP_UC
    uc_on_disconnect(handle);
#endif
    cap_peer_t *p = peer_get(handle, false);
    if (p) {
      memset(p, 0, sizeof(*p));
    }
  }
#if CAP_PAST
  else if (ev->type == ESP_BLE_AUDIO_GAP_EVENT_ACL_CONNECT && ev->acl_connect.status == 0) {
    cap_peer_t *p = peer_get(ev->acl_connect.conn_handle, true);
    if (p) {
      p->addr_type = ev->acl_connect.dst.type;
      memcpy(p->addr, ev->acl_connect.dst.val, sizeof(p->addr));
    }
  }
#endif
#else
  (void)event;
#endif
}

/** Engine deinit hook: the stack released everything; clear all state so the next session re-registers. */
static void cap_on_deinit(void) {
#if CSIS_SRV
  memset(s_csis, 0, sizeof(s_csis));
#endif
#if CSIP_CRD
  s_crd_registered = false;
#endif
#if CAP_INIT
  s_init_registered = false;
#endif
#if CAP_UC
  memset(&s_uc, 0, sizeof(s_uc));
#endif
#if CAP_BC
  memset(&s_bc, 0, sizeof(s_bc));
#endif
#if CAP_HO
  s_ho_registered = false;
#endif
#if CAP_CMD
  s_cmd_registered = false;
#endif
#if CAP_BASS
  s_bass_registered = false;
  s_track_id = TRACK_NONE;
#endif
#if CAP_PEERS
  memset(s_peers, 0, sizeof(s_peers));
#endif
}

#endif /* CAP_ANY */

#endif /* BLE_AUDIO_SUPPORTED */
