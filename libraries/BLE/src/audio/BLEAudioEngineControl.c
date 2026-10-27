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
 * @file
 * @brief Control-profile engine unit: VCP, MICP, MCP, CCP and HAS roles.
 *
 * Each role section follows the same shape:
 *  - static stack callbacks that translate the stack's arguments into one
 *    BLE_AUDIO_EVT_* envelope (bleAudioEngineEmit, synchronous);
 *  - an idempotent Init that attaches this unit to the engine and registers
 *    the stack service or callback table, recording the role in s_roles;
 *  - thin operations that check the role (NEED_ROLE) or the tracked peer and
 *    forward to the stack, returning its esp_err_t unchanged;
 *  - an #else branch with ESP_ERR_NOT_SUPPORTED stubs, so the C++ layer links
 *    whatever the Kconfig selection.
 *
 * The VCP, MICP and HAS clients track a single peer: the one passed to the
 * last Discover. The stack reports their events per instance pointer, so the
 * callbacks map the pointer back to that peer (vctl_conn, mctl_conn, ...) and
 * drop events from any other instance. The peer is forgotten on ACL
 * disconnect and on engine deinit (unit hooks). The MCP and CCP clients are
 * addressed by connection handle in the stack, so they keep no peer state.
 *
 * API contract is documented on the declarations in `BLEAudioEngineControl.h`;
 * the definitions below carry implementation notes only.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include "audio/BLEAudioEngineControl.h"

#include "esp_ble_audio_common_api.h"
#include "esp_ble_audio_vcp_api.h"
#include "esp_ble_audio_vocs_api.h"
#include "esp_ble_audio_aics_api.h"
#include "esp_ble_audio_micp_api.h"
#include "esp_ble_audio_media_proxy_api.h"
#include "esp_ble_audio_mcc_api.h"
#include "esp_ble_audio_tbs_api.h"
#include "esp_ble_audio_has_api.h"
#include "esp_log.h"

#include <string.h>

#define CONN_NONE  BLE_AUDIO_CONN_NONE
/** Handle of a stack connection pointer; server callbacks pass NULL for local changes. */
#define CONN_OF(c) ((c) ? (c)->handle : CONN_NONE)

/* The media player needs the local player API of the media proxy. */
#define MCP_SRV (BLE_AUDIO_MCP_SERVER_SUPPORTED && CONFIG_BT_MCTL_LOCAL_PLAYER_CONTROL)

/* Included-service support per role; each needs both the role and its Kconfig instance option. */
#define VREND_AICS (BLE_AUDIO_VCP_RENDERER_SUPPORTED && CONFIG_BT_VCP_VOL_REND_AICS_INSTANCE_COUNT > 0)
#define MDEV_AICS  (BLE_AUDIO_MICP_DEVICE_SUPPORTED && CONFIG_BT_MICP_MIC_DEV_AICS_INSTANCE_COUNT > 0)
#define VCTL_VOCS  (BLE_AUDIO_VCP_CONTROLLER_SUPPORTED && CONFIG_BT_VCP_VOL_CTLR_VOCS)
#define VCTL_AICS  (BLE_AUDIO_VCP_CONTROLLER_SUPPORTED && CONFIG_BT_VCP_VOL_CTLR_AICS)
#define MCTL_AICS  (BLE_AUDIO_MICP_CONTROLLER_SUPPORTED && CONFIG_BT_MICP_MIC_CTLR_AICS)

/* True when at least one role is compiled in; gates the shared state and hooks. */
#define CTL_ANY                                                                                                                        \
  (BLE_AUDIO_VCP_RENDERER_SUPPORTED || BLE_AUDIO_VCP_CONTROLLER_SUPPORTED || BLE_AUDIO_MICP_DEVICE_SUPPORTED                           \
   || BLE_AUDIO_MICP_CONTROLLER_SUPPORTED || MCP_SRV || BLE_AUDIO_MCP_CLIENT_SUPPORTED || BLE_AUDIO_CCP_SERVER_SUPPORTED                \
   || BLE_AUDIO_CCP_CLIENT_SUPPORTED || BLE_AUDIO_HAS_SUPPORTED || BLE_AUDIO_HAS_CLIENT_SUPPORTED)

#if CTL_ANY

static const char *TAG = "BLEAudioCtl";

/** One bit per role in s_roles; also printed (hex) when a registration fails. */
enum {
  ROLE_VCP_REND = 1 << 0,
  ROLE_VCP_CTLR = 1 << 1,
  ROLE_MICP_DEV = 1 << 2,
  ROLE_MICP_CTLR = 1 << 3,
  ROLE_MCP_SRV = 1 << 4,
  ROLE_MCP_CLI = 1 << 5,
  ROLE_CCP_SRV = 1 << 6,
  ROLE_CCP_CLI = 1 << 7,
  ROLE_HAS_SRV = 1 << 8,
  ROLE_HAS_CLI = 1 << 9,
};

/* Roles registered this session; common_deinit releases the stack side. */
static uint16_t s_roles;

/** Return ESP_ERR_INVALID_STATE from the calling function unless role @p r was initialised. */
#define NEED_ROLE(r)                    \
  do {                                  \
    if (!(s_roles & (r))) {             \
      return ESP_ERR_INVALID_STATE;     \
    }                                   \
  } while (0)

/* ── Per-peer client state (one peer per role) ──────────────────────────── */

#if BLE_AUDIO_VCP_CONTROLLER_SUPPORTED
/** Volume controller peer. */
static struct {
  uint16_t conn;                        ///< Peer passed to the last Discover, CONN_NONE when idle.
  bool syncing; /* discovered, waiting for the first state read */
  ble_audio_vcp_discovered_t disc;      ///< Held until the first state read completes.
  esp_ble_audio_vcp_vol_ctlr_t *ctlr;   ///< Stack instance for the peer.
  esp_ble_audio_vocs_t *vocs;           ///< Peer's first VOCS instance, NULL when absent.
  esp_ble_audio_aics_t *aics;           ///< Peer's first AICS instance, NULL when absent.
} s_vctl = {.conn = CONN_NONE};
#endif

#if BLE_AUDIO_MICP_CONTROLLER_SUPPORTED
/** Microphone controller peer. */
static struct {
  uint16_t conn;                        ///< Peer passed to the last Discover, CONN_NONE when idle.
  esp_ble_audio_micp_mic_ctlr_t *ctlr;  ///< Stack instance for the peer.
  esp_ble_audio_aics_t *aics;           ///< Peer's first microphone AICS instance, NULL when absent.
} s_mctl = {.conn = CONN_NONE};
#endif

#if BLE_AUDIO_HAS_CLIENT_SUPPORTED
/** Hearing aid controller peer. */
static struct {
  uint16_t conn;                        ///< Peer passed to the last Discover, CONN_NONE when idle.
  esp_ble_audio_has_t *has;             ///< Stack instance, set by the discover callback.
} s_hasc = {.conn = CONN_NONE};
#endif

#if MCP_SRV && CONFIG_BT_MCTL_LOCAL_PLAYER_LOCAL_CONTROL
/** Local player handle, needed to send commands to our own player. */
static esp_ble_audio_media_player_t *s_player;
#endif

#if BLE_AUDIO_CCP_SERVER_SUPPORTED
/** Bearer index of the registered GTBS instance. */
static uint8_t s_tbs_bearer;
#endif

#if BLE_AUDIO_HAS_SUPPORTED && !CONFIG_BT_HAS_PRESET_NAME_DYNAMIC && CONFIG_BT_HAS_PRESET_COUNT > 0
#define HAS_KEEP_NAMES 1
/* Without dynamic names the stack keeps the caller's name pointer. */
static struct {
  uint8_t index;
  char name[ESP_BLE_AUDIO_HAS_PRESET_NAME_MAX + 1];
} s_has_names[CONFIG_BT_HAS_PRESET_COUNT];
#endif

/** Forget a client peer: clear all instance pointers and mark it idle. */
#define PEER_RESET(p)             \
  do {                            \
    memset(&(p), 0, sizeof(p));   \
    (p).conn = CONN_NONE;         \
  } while (0)

/* ── Unit hooks ─────────────────────────────────────────────────────────── */

/** Engine GAP hook: forget any client peer whose link dropped. */
static void ctl_on_gap(const void *event) {
  const esp_ble_audio_gap_app_event_t *ev = (const esp_ble_audio_gap_app_event_t *)event;
  if (ev->type != ESP_BLE_AUDIO_GAP_EVENT_ACL_DISCONNECT) {
    return;
  }
  const uint16_t conn = ev->acl_disconnect.conn_handle;
  (void)conn;
#if BLE_AUDIO_VCP_CONTROLLER_SUPPORTED
  if (s_vctl.conn == conn) {
    PEER_RESET(s_vctl);
  }
#endif
#if BLE_AUDIO_MICP_CONTROLLER_SUPPORTED
  if (s_mctl.conn == conn) {
    PEER_RESET(s_mctl);
  }
#endif
#if BLE_AUDIO_HAS_CLIENT_SUPPORTED
  if (s_hasc.conn == conn) {
    PEER_RESET(s_hasc);
  }
#endif
}

/**
 * Engine deinit hook: the stack has released every service and callback
 * table, so clear all local state; the next session re-registers from scratch.
 */
static void ctl_on_deinit(void) {
  s_roles = 0;
#if BLE_AUDIO_VCP_CONTROLLER_SUPPORTED
  PEER_RESET(s_vctl);
#endif
#if BLE_AUDIO_MICP_CONTROLLER_SUPPORTED
  PEER_RESET(s_mctl);
#endif
#if BLE_AUDIO_HAS_CLIENT_SUPPORTED
  PEER_RESET(s_hasc);
#endif
#if MCP_SRV && CONFIG_BT_MCTL_LOCAL_PLAYER_LOCAL_CONTROL
  s_player = NULL;
#endif
#if HAS_KEEP_NAMES
  memset(s_has_names, 0, sizeof(s_has_names));
#endif
}

static const ble_audio_unit_hooks_t s_hooks = {.on_gap = ctl_on_gap, .on_deinit = ctl_on_deinit};

/**
 * First step of every Init: requires an initialised engine and registers
 * this unit's hooks (idempotent, so every role can call it).
 */
static int role_attach(void) {
  return bleAudioEngineIsInitialized() ? bleAudioEngineRegisterUnit(&s_hooks) : ESP_ERR_INVALID_STATE;
}

/** Last step of every Init: record @p role on success, log on failure; returns @p err. */
static int role_done(uint16_t role, int err) {
  if (err == ESP_OK) {
    s_roles |= role;
  } else {
    ESP_LOGE(TAG, "control role 0x%03x (ROLE_* bit) registration failed: %d", role, err);
  }
  return err;
}

#endif /* CTL_ANY */

/* ── Shared AICS helpers ────────────────────────────────────────────────── */

#if VREND_AICS || MDEV_AICS
/**
 * Registration defaults for a local AICS instance: unmuted, manual gain in
 * 1-unit steps over -100..100, active, writable description (@p desc).
 */
static void aics_srv_param(esp_ble_audio_aics_register_param_t *p, uint8_t type, const char *desc) {
  memset(p, 0, sizeof(*p));
  p->mute = ESP_BLE_AUDIO_AICS_STATE_UNMUTED;
  p->gain_mode = ESP_BLE_AUDIO_AICS_MODE_MANUAL;
  p->units = 1;
  p->min_gain = -100;
  p->max_gain = 100;
  p->type = type;
  p->status = true;
  p->desc_writable = true;
  p->description = (char *)desc;
}
#endif

#if VCTL_AICS || MCTL_AICS
/** Emit a remote AICS state as ble_audio_aics_state_t under event @p type. */
static void aics_emit(uint16_t type, uint16_t conn, int err, int8_t gain, uint8_t mute) {
  const ble_audio_aics_state_t d = {.gain = gain, .mute = mute == ESP_BLE_AUDIO_AICS_STATE_MUTED};
  bleAudioEngineEmit(type, conn, err, &d);
}
#endif

/* ── VCP volume renderer ────────────────────────────────────────────────── */

#if BLE_AUDIO_VCP_RENDERER_SUPPORTED

/** Volume state changed, by a client write (@p conn set) or a local call (@p conn NULL). */
static void vrend_state(esp_ble_conn_t *conn, int err, uint8_t volume, uint8_t mute) {
  const ble_audio_vcp_state_t d = {.volume = volume, .mute = mute == ESP_BLE_AUDIO_VCP_STATE_MUTED};
  bleAudioEngineEmit(BLE_AUDIO_EVT_VCP_REND_STATE, CONN_OF(conn), err, &d);
}

static esp_ble_audio_vcp_vol_rend_cb_t s_vrend_cb = {.state = vrend_state};

int bleAudioVcpRendInit(uint8_t volume, bool mute, uint8_t step) {
  if (s_roles & ROLE_VCP_REND) {
    return ESP_OK;
  }
  int err = role_attach();
  if (err == ESP_OK) {
    esp_ble_audio_vcp_vol_rend_register_param_t p = {
      .step = step,
      .mute = mute ? ESP_BLE_AUDIO_VCP_STATE_MUTED : ESP_BLE_AUDIO_VCP_STATE_UNMUTED,
      .volume = volume,
      .cb = &s_vrend_cb,
    };
#if CONFIG_BT_VCP_VOL_REND_VOCS_INSTANCE_COUNT > 0
    esp_ble_audio_vocs_register_param_t vocs[CONFIG_BT_VCP_VOL_REND_VOCS_INSTANCE_COUNT];
    memset(vocs, 0, sizeof(vocs));
    for (int i = 0; i < CONFIG_BT_VCP_VOL_REND_VOCS_INSTANCE_COUNT; i++) {
      vocs[i].location_writable = true;
      vocs[i].desc_writable = true;
      vocs[i].output_desc = (char *)"Output";
    }
    p.vocs_param = vocs;
#endif
#if VREND_AICS
    esp_ble_audio_aics_register_param_t aics[CONFIG_BT_VCP_VOL_REND_AICS_INSTANCE_COUNT];
    for (int i = 0; i < CONFIG_BT_VCP_VOL_REND_AICS_INSTANCE_COUNT; i++) {
      aics_srv_param(&aics[i], ESP_BLE_AUDIO_AICS_INPUT_TYPE_UNSPECIFIED, "Input");
    }
    p.aics_param = aics;
#endif
    err = esp_ble_audio_vcp_vol_rend_register(&p);
  }
  return role_done(ROLE_VCP_REND, err);
}

int bleAudioVcpRendOp(uint8_t op, uint8_t value) {
  NEED_ROLE(ROLE_VCP_REND);
  switch (op) {
    case BLE_AUDIO_VCP_OP_SET_VOLUME: return esp_ble_audio_vcp_vol_rend_set_vol(value);
    case BLE_AUDIO_VCP_OP_MUTE:       return esp_ble_audio_vcp_vol_rend_mute();
    case BLE_AUDIO_VCP_OP_UNMUTE:     return esp_ble_audio_vcp_vol_rend_unmute();
    case BLE_AUDIO_VCP_OP_UP:         return esp_ble_audio_vcp_vol_rend_vol_up();
    case BLE_AUDIO_VCP_OP_DOWN:       return esp_ble_audio_vcp_vol_rend_vol_down();
    case BLE_AUDIO_VCP_OP_SET_STEP:   return esp_ble_audio_vcp_vol_rend_set_step(value);
    default:                          return ESP_ERR_INVALID_ARG;
  }
}

#else

int bleAudioVcpRendInit(uint8_t volume, bool mute, uint8_t step) {
  (void)volume;
  (void)mute;
  (void)step;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioVcpRendOp(uint8_t op, uint8_t value) {
  (void)op;
  (void)value;
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* BLE_AUDIO_VCP_RENDERER_SUPPORTED */

/* ── VCP volume controller ──────────────────────────────────────────────── */

#if BLE_AUDIO_VCP_CONTROLLER_SUPPORTED

/** Map a stack instance (controller, VOCS or AICS) to the tracked peer, CONN_NONE if foreign. */
static uint16_t vctl_conn(const void *inst) {
  return (inst && (inst == s_vctl.ctlr || inst == s_vctl.vocs || inst == s_vctl.aics)) ? s_vctl.conn : CONN_NONE;
}

/**
 * Discovery finished. On success, capture the first VOCS/AICS instances and
 * start a state read; DISCOVERED is then deferred to vctl_state so the
 * application can write immediately. On failure (or if the read cannot be
 * queued) DISCOVERED is emitted now with the error.
 */
static void vctl_discover(esp_ble_audio_vcp_vol_ctlr_t *ctlr, int err, uint8_t vocs_count, uint8_t aics_count) {
  if (ctlr != s_vctl.ctlr) {
    return;
  }
  s_vctl.disc.vocs_count = vocs_count;
  s_vctl.disc.aics_count = aics_count;
  if (err == 0) {
#if VCTL_VOCS || VCTL_AICS
    esp_ble_audio_vcp_included_t inc = {0};
    if (esp_ble_audio_vcp_vol_ctlr_included_get(ctlr, &inc) == ESP_OK) {
      s_vctl.vocs = inc.vocs_cnt ? inc.vocs[0] : NULL;
      s_vctl.aics = inc.aics_cnt ? inc.aics[0] : NULL;
    }
#endif
    /* The controller must learn the change counter before its first write. */
    s_vctl.syncing = esp_ble_audio_vcp_vol_ctlr_read_state(ctlr) == ESP_OK;
    if (s_vctl.syncing) {
      return;
    }
  }
  bleAudioEngineEmit(BLE_AUDIO_EVT_VCP_CTLR_DISCOVERED, s_vctl.conn, err, &s_vctl.disc);
}

/** Volume state read or notified; the first one after discovery also releases DISCOVERED. */
static void vctl_state(esp_ble_audio_vcp_vol_ctlr_t *ctlr, int err, uint8_t volume, uint8_t mute) {
  const uint16_t conn = vctl_conn(ctlr);
  if (conn == CONN_NONE) {
    return;
  }
  if (s_vctl.syncing) {
    s_vctl.syncing = false;
    bleAudioEngineEmit(BLE_AUDIO_EVT_VCP_CTLR_DISCOVERED, conn, 0, &s_vctl.disc);
  }
  const ble_audio_vcp_state_t d = {.volume = volume, .mute = mute == ESP_BLE_AUDIO_VCP_STATE_MUTED};
  bleAudioEngineEmit(BLE_AUDIO_EVT_VCP_CTLR_STATE, conn, err, &d);
}

#if VCTL_VOCS
static void vctl_vocs_state(esp_ble_audio_vocs_t *inst, int err, int16_t offset) {
  const uint16_t conn = vctl_conn(inst);
  if (conn != CONN_NONE) {
    bleAudioEngineEmit(BLE_AUDIO_EVT_VCP_CTLR_OFFSET, conn, err, &offset);
  }
}
#endif

#if VCTL_AICS
static void vctl_aics_state(esp_ble_audio_aics_t *inst, int err, int8_t gain, uint8_t mute, uint8_t mode) {
  (void)mode;
  const uint16_t conn = vctl_conn(inst);
  if (conn != CONN_NONE) {
    aics_emit(BLE_AUDIO_EVT_VCP_CTLR_INPUT, conn, err, gain, mute);
  }
}
#endif

static esp_ble_audio_vcp_vol_ctlr_cb_t s_vctl_cb = {
  .state = vctl_state,
  .discover = vctl_discover,
#if VCTL_VOCS
  .vocs_cb = {.state = vctl_vocs_state},
#endif
#if VCTL_AICS
  .aics_cb = {.state = vctl_aics_state},
#endif
};

int bleAudioVcpCtlrInit(void) {
  if (s_roles & ROLE_VCP_CTLR) {
    return ESP_OK;
  }
  int err = role_attach();
  if (err == ESP_OK) {
    /* Fails only when the static table is still listed from an earlier session. */
    (void)esp_ble_audio_vcp_vol_ctlr_cb_register(&s_vctl_cb);
  }
  return role_done(ROLE_VCP_CTLR, err);
}

int bleAudioVcpCtlrDiscover(uint16_t conn_handle) {
  NEED_ROLE(ROLE_VCP_CTLR);
  /* A new Discover replaces the previous peer, even if it fails. */
  PEER_RESET(s_vctl);
  esp_ble_audio_vcp_vol_ctlr_t *ctlr = NULL;
  int err = esp_ble_audio_vcp_vol_ctlr_discover(conn_handle, &ctlr);
  if (err == ESP_OK) {
    s_vctl.conn = conn_handle;
    s_vctl.ctlr = ctlr;
  }
  return err;
}

int bleAudioVcpCtlrOp(uint16_t conn_handle, uint8_t op, uint8_t volume) {
  esp_ble_audio_vcp_vol_ctlr_t *c = conn_handle == s_vctl.conn ? s_vctl.ctlr : NULL;
  if (!c) {
    return ESP_ERR_INVALID_STATE;
  }
  switch (op) {
    case BLE_AUDIO_VCP_OP_SET_VOLUME: return esp_ble_audio_vcp_vol_ctlr_set_vol(c, volume);
    case BLE_AUDIO_VCP_OP_MUTE:       return esp_ble_audio_vcp_vol_ctlr_mute(c);
    case BLE_AUDIO_VCP_OP_UNMUTE:     return esp_ble_audio_vcp_vol_ctlr_unmute(c);
    case BLE_AUDIO_VCP_OP_UP:         return esp_ble_audio_vcp_vol_ctlr_vol_up(c);
    case BLE_AUDIO_VCP_OP_DOWN:       return esp_ble_audio_vcp_vol_ctlr_vol_down(c);
    case BLE_AUDIO_VCP_OP_READ:       return esp_ble_audio_vcp_vol_ctlr_read_state(c);
    default:                          return ESP_ERR_INVALID_ARG;
  }
}

int bleAudioVcpCtlrSetOffset(uint16_t conn_handle, int16_t offset) {
#if VCTL_VOCS
  if (conn_handle != s_vctl.conn || !s_vctl.vocs) {
    return ESP_ERR_INVALID_STATE;
  }
  return esp_ble_audio_vocs_state_set(s_vctl.vocs, offset);
#else
  (void)conn_handle;
  (void)offset;
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

int bleAudioVcpCtlrSetGain(uint16_t conn_handle, int8_t gain) {
#if VCTL_AICS
  if (conn_handle != s_vctl.conn || !s_vctl.aics) {
    return ESP_ERR_INVALID_STATE;
  }
  return esp_ble_audio_aics_gain_set(s_vctl.aics, gain);
#else
  (void)conn_handle;
  (void)gain;
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

#else

int bleAudioVcpCtlrInit(void) {
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioVcpCtlrDiscover(uint16_t conn_handle) {
  (void)conn_handle;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioVcpCtlrOp(uint16_t conn_handle, uint8_t op, uint8_t volume) {
  (void)conn_handle;
  (void)op;
  (void)volume;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioVcpCtlrSetOffset(uint16_t conn_handle, int16_t offset) {
  (void)conn_handle;
  (void)offset;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioVcpCtlrSetGain(uint16_t conn_handle, int8_t gain) {
  (void)conn_handle;
  (void)gain;
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* BLE_AUDIO_VCP_CONTROLLER_SUPPORTED */

/* ── MICP microphone device ─────────────────────────────────────────────── */

#if BLE_AUDIO_MICP_DEVICE_SUPPORTED

/** Mute state changed (the stack does not say by whom, so conn is always NONE). */
static void mdev_mute(uint8_t mute) {
  bleAudioEngineEmit(BLE_AUDIO_EVT_MICP_DEV_MUTE, CONN_NONE, 0, &mute);
}

static esp_ble_audio_micp_mic_dev_cb_t s_mdev_cb = {.mute = mdev_mute};

int bleAudioMicpDevInit(bool mute) {
  if (s_roles & ROLE_MICP_DEV) {
    return ESP_OK;
  }
  int err = role_attach();
  if (err == ESP_OK) {
    esp_ble_audio_micp_mic_dev_register_param_t p = {.cb = &s_mdev_cb};
#if MDEV_AICS
    esp_ble_audio_aics_register_param_t aics[CONFIG_BT_MICP_MIC_DEV_AICS_INSTANCE_COUNT];
    for (int i = 0; i < CONFIG_BT_MICP_MIC_DEV_AICS_INSTANCE_COUNT; i++) {
      aics_srv_param(&aics[i], ESP_BLE_AUDIO_AICS_INPUT_TYPE_MICROPHONE, "Mic");
    }
    p.aics_param = aics;
#endif
    err = esp_ble_audio_micp_mic_dev_register(&p);
    /* MICS registers unmuted; apply the initial state afterwards. */
    if (err == ESP_OK && mute) {
      (void)esp_ble_audio_micp_mic_dev_mute();
    }
  }
  return role_done(ROLE_MICP_DEV, err);
}

int bleAudioMicpDevSetMute(uint8_t state) {
  NEED_ROLE(ROLE_MICP_DEV);
  switch (state) {
    case BLE_AUDIO_MICP_UNMUTED:  return esp_ble_audio_micp_mic_dev_unmute();
    case BLE_AUDIO_MICP_MUTED:    return esp_ble_audio_micp_mic_dev_mute();
    case BLE_AUDIO_MICP_DISABLED: return esp_ble_audio_micp_mic_dev_mute_disable();
    default:                      return ESP_ERR_INVALID_ARG;
  }
}

#else

int bleAudioMicpDevInit(bool mute) {
  (void)mute;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioMicpDevSetMute(uint8_t state) {
  (void)state;
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* BLE_AUDIO_MICP_DEVICE_SUPPORTED */

/* ── MICP microphone controller ─────────────────────────────────────────── */

#if BLE_AUDIO_MICP_CONTROLLER_SUPPORTED

/** Map a stack instance (controller or AICS) to the tracked peer, CONN_NONE if foreign. */
static uint16_t mctl_conn(const void *inst) {
  return (inst && (inst == s_mctl.ctlr || inst == s_mctl.aics)) ? s_mctl.conn : CONN_NONE;
}

/**
 * Discovery finished: capture the first microphone AICS, emit DISCOVERED,
 * then read the mute state so MICP_CTLR_MUTE follows. MICS writes carry no
 * change counter, so unlike VCP nothing needs to be read before writing.
 */
static void mctl_discover(esp_ble_audio_micp_mic_ctlr_t *ctlr, int err, uint8_t aics_count) {
  const uint16_t conn = mctl_conn(ctlr);
  if (conn == CONN_NONE) {
    return;
  }
#if MCTL_AICS
  if (err == 0) {
    esp_ble_audio_micp_included_t inc = {0};
    if (esp_ble_audio_micp_mic_ctlr_included_get(ctlr, &inc) == ESP_OK && inc.aics_cnt) {
      s_mctl.aics = inc.aics[0];
    }
  }
#endif
  bleAudioEngineEmit(BLE_AUDIO_EVT_MICP_CTLR_DISCOVERED, conn, err, &aics_count);
  if (err == 0) {
    (void)esp_ble_audio_micp_mic_ctlr_mute_get(ctlr);
  }
}

/** Mute state read or notified by the peer. */
static void mctl_mute(esp_ble_audio_micp_mic_ctlr_t *ctlr, int err, uint8_t mute) {
  const uint16_t conn = mctl_conn(ctlr);
  if (conn != CONN_NONE) {
    bleAudioEngineEmit(BLE_AUDIO_EVT_MICP_CTLR_MUTE, conn, err, &mute);
  }
}

#if MCTL_AICS
static void mctl_aics_state(esp_ble_audio_aics_t *inst, int err, int8_t gain, uint8_t mute, uint8_t mode) {
  (void)mode;
  const uint16_t conn = mctl_conn(inst);
  if (conn != CONN_NONE) {
    aics_emit(BLE_AUDIO_EVT_MICP_CTLR_INPUT, conn, err, gain, mute);
  }
}
#endif

static esp_ble_audio_micp_mic_ctlr_cb_t s_mctl_cb = {
  .mute = mctl_mute,
  .discover = mctl_discover,
#if MCTL_AICS
  .aics_cb = {.state = mctl_aics_state},
#endif
};

int bleAudioMicpCtlrInit(void) {
  if (s_roles & ROLE_MICP_CTLR) {
    return ESP_OK;
  }
  int err = role_attach();
  if (err == ESP_OK) {
    /* Fails only when the static table is still listed from an earlier session. */
    (void)esp_ble_audio_micp_mic_ctlr_cb_register(&s_mctl_cb);
  }
  return role_done(ROLE_MICP_CTLR, err);
}

int bleAudioMicpCtlrDiscover(uint16_t conn_handle) {
  NEED_ROLE(ROLE_MICP_CTLR);
  /* A new Discover replaces the previous peer, even if it fails. */
  PEER_RESET(s_mctl);
  esp_ble_audio_micp_mic_ctlr_t *ctlr = NULL;
  int err = esp_ble_audio_micp_mic_ctlr_discover(conn_handle, &ctlr);
  if (err == ESP_OK) {
    s_mctl.conn = conn_handle;
    s_mctl.ctlr = ctlr;
  }
  return err;
}

/** Controller instance for @p conn_handle, NULL unless it is the discovered peer. */
static esp_ble_audio_micp_mic_ctlr_t *mctl_get(uint16_t conn_handle) {
  return conn_handle == s_mctl.conn ? s_mctl.ctlr : NULL;
}

int bleAudioMicpCtlrSetMute(uint16_t conn_handle, bool mute) {
  esp_ble_audio_micp_mic_ctlr_t *c = mctl_get(conn_handle);
  if (!c) {
    return ESP_ERR_INVALID_STATE;
  }
  return mute ? esp_ble_audio_micp_mic_ctlr_mute(c) : esp_ble_audio_micp_mic_ctlr_unmute(c);
}

int bleAudioMicpCtlrReadMute(uint16_t conn_handle) {
  esp_ble_audio_micp_mic_ctlr_t *c = mctl_get(conn_handle);
  return c ? esp_ble_audio_micp_mic_ctlr_mute_get(c) : ESP_ERR_INVALID_STATE;
}

int bleAudioMicpCtlrSetGain(uint16_t conn_handle, int8_t gain) {
#if MCTL_AICS
  if (conn_handle != s_mctl.conn || !s_mctl.aics) {
    return ESP_ERR_INVALID_STATE;
  }
  return esp_ble_audio_aics_gain_set(s_mctl.aics, gain);
#else
  (void)conn_handle;
  (void)gain;
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

#else

int bleAudioMicpCtlrInit(void) {
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioMicpCtlrDiscover(uint16_t conn_handle) {
  (void)conn_handle;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioMicpCtlrSetMute(uint16_t conn_handle, bool mute) {
  (void)conn_handle;
  (void)mute;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioMicpCtlrReadMute(uint16_t conn_handle) {
  (void)conn_handle;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioMicpCtlrSetGain(uint16_t conn_handle, int8_t gain) {
  (void)conn_handle;
  (void)gain;
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* BLE_AUDIO_MICP_CONTROLLER_SUPPORTED */

/* ── MCP media player ───────────────────────────────────────────────────── */

#if MCP_SRV

#if CONFIG_BT_MCTL_LOCAL_PLAYER_LOCAL_CONTROL
/*
 * Local control: the application drives its own player through the media
 * proxy, and the proxy reports every state change and command result, from
 * local or remote controllers alike (hence conn is always NONE).
 */

/** The proxy hands out the local player handle once, after registration. */
static void mpl_instance(esp_ble_audio_media_player_t *player, int err) {
  if (err == 0) {
    s_player = player;
  }
}

/** Player state changed. */
static void mpl_state(esp_ble_audio_media_player_t *player, int err, uint8_t state) {
  (void)player;
  bleAudioEngineEmit(BLE_AUDIO_EVT_MCP_SRV_STATE, CONN_NONE, err, &state);
}

/** A command was executed by the player; @p ntf is NULL when it could not be delivered. */
static void mpl_result(esp_ble_audio_media_player_t *player, int err, const esp_ble_audio_mpl_cmd_ntf_t *ntf) {
  (void)player;
  const ble_audio_mcp_result_t d = {.opcode = ntf ? ntf->requested_opcode : 0, .result = ntf ? ntf->result_code : 0};
  bleAudioEngineEmit(BLE_AUDIO_EVT_MCP_SRV_RESULT, CONN_NONE, err, &d);
}

static esp_ble_audio_media_proxy_ctrl_cbs_t s_mpl_cbs = {
  .local_player_instance = mpl_instance,
  .media_state_recv = mpl_state,
  .command_recv = mpl_result,
};
#endif

int bleAudioMcpSrvInit(void) {
  if (s_roles & ROLE_MCP_SRV) {
    return ESP_OK;
  }
  int err = role_attach();
  if (err == ESP_OK) {
    err = esp_ble_audio_media_proxy_pl_init();
  }
#if CONFIG_BT_MCTL_LOCAL_PLAYER_LOCAL_CONTROL
  /* The player itself works without local control; only state/result reporting and commands are lost. */
  if (err == ESP_OK && esp_ble_audio_media_proxy_ctrl_register(&s_mpl_cbs) != ESP_OK) {
    ESP_LOGW(TAG, "media player: local control registration failed, state and command results will not be reported");
  }
#endif
  return role_done(ROLE_MCP_SRV, err);
}

int bleAudioMcpSrvSetText(bool title, const char *text) {
  NEED_ROLE(ROLE_MCP_SRV);
  return title ? esp_ble_audio_media_proxy_pl_set_track_title((char *)text) : esp_ble_audio_media_proxy_pl_set_player_name((char *)text);
}

int bleAudioMcpSrvCommand(uint8_t opcode) {
#if CONFIG_BT_MCTL_LOCAL_PLAYER_LOCAL_CONTROL
  /* Not yet handed out by mpl_instance (or local control registration failed). */
  if (!s_player) {
    return ESP_ERR_INVALID_STATE;
  }
  const esp_ble_audio_mpl_cmd_t cmd = {.opcode = opcode};
  return esp_ble_audio_media_proxy_ctrl_send_command(s_player, &cmd);
#else
  (void)opcode;
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

#else

int bleAudioMcpSrvInit(void) {
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioMcpSrvSetText(bool title, const char *text) {
  (void)title;
  (void)text;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioMcpSrvCommand(uint8_t opcode) {
  (void)opcode;
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* MCP_SRV */

/* ── MCP media controller ───────────────────────────────────────────────── */

#if BLE_AUDIO_MCP_CLIENT_SUPPORTED

/** MCS/GMCS discovered and subscribed; follow up with a state read when that API is built in. */
static void mcc_discovered(esp_ble_conn_t *conn, int err) {
  const uint16_t h = CONN_OF(conn);
  bleAudioEngineEmit(BLE_AUDIO_EVT_MCP_CLI_DISCOVERED, h, err, NULL);
#if CONFIG_BT_MCC_READ_MEDIA_STATE
  if (err == 0) {
    (void)esp_ble_audio_mcc_read_media_state(h);
  }
#endif
}

/* Reads and notifications share these callbacks; strings are valid during the emit only. */
static void mcc_state(esp_ble_conn_t *conn, int err, uint8_t state) {
  bleAudioEngineEmit(BLE_AUDIO_EVT_MCP_CLI_STATE, CONN_OF(conn), err, &state);
}

static void mcc_player_name(esp_ble_conn_t *conn, int err, const char *name) {
  bleAudioEngineEmit(BLE_AUDIO_EVT_MCP_CLI_PLAYER_NAME, CONN_OF(conn), err, name);
}

static void mcc_track_title(esp_ble_conn_t *conn, int err, const char *title) {
  bleAudioEngineEmit(BLE_AUDIO_EVT_MCP_CLI_TRACK_TITLE, CONN_OF(conn), err, title);
}

/** Control point write completed; only a failed write is reported here (result 0). */
static void mcc_cmd_sent(esp_ble_conn_t *conn, int err, const esp_ble_audio_mpl_cmd_t *cmd) {
  if (err == 0) {
    return; /* the result arrives with the control point notification */
  }
  const ble_audio_mcp_result_t d = {.opcode = cmd ? cmd->opcode : 0, .result = 0};
  bleAudioEngineEmit(BLE_AUDIO_EVT_MCP_CLI_RESULT, CONN_OF(conn), err, &d);
}

/** The player's result for a command, from the control point notification. */
static void mcc_cmd_ntf(esp_ble_conn_t *conn, int err, const esp_ble_audio_mpl_cmd_ntf_t *ntf) {
  const ble_audio_mcp_result_t d = {.opcode = ntf ? ntf->requested_opcode : 0, .result = ntf ? ntf->result_code : 0};
  bleAudioEngineEmit(BLE_AUDIO_EVT_MCP_CLI_RESULT, CONN_OF(conn), err, &d);
}

static esp_ble_audio_mcc_cb_t s_mcc_cb = {
  .discover_mcs = mcc_discovered,
  .read_player_name = mcc_player_name,
  .read_track_title = mcc_track_title,
  .read_media_state = mcc_state,
  .send_cmd = mcc_cmd_sent,
  .cmd_ntf = mcc_cmd_ntf,
};

int bleAudioMcpCliInit(void) {
  if (s_roles & ROLE_MCP_CLI) {
    return ESP_OK;
  }
  int err = role_attach();
  if (err == ESP_OK) {
    err = esp_ble_audio_mcc_init(&s_mcc_cb);
  }
  return role_done(ROLE_MCP_CLI, err);
}

int bleAudioMcpCliDiscover(uint16_t conn_handle) {
  NEED_ROLE(ROLE_MCP_CLI);
  return esp_ble_audio_mcc_discover_mcs(conn_handle, true);
}

int bleAudioMcpCliCommand(uint16_t conn_handle, uint8_t opcode) {
  NEED_ROLE(ROLE_MCP_CLI);
#if CONFIG_BT_MCC_SET_MEDIA_CONTROL_POINT
  const esp_ble_audio_mpl_cmd_t cmd = {.opcode = opcode};
  return esp_ble_audio_mcc_send_cmd(conn_handle, &cmd);
#else
  (void)conn_handle;
  (void)opcode;
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

int bleAudioMcpCliRead(uint16_t conn_handle, uint8_t what) {
  NEED_ROLE(ROLE_MCP_CLI);
  /* State and track-title reads are optional in the stack (CONFIG_BT_MCC_READ_*). */
  switch (what) {
#if CONFIG_BT_MCC_READ_MEDIA_STATE
    case BLE_AUDIO_MCP_READ_STATE: return esp_ble_audio_mcc_read_media_state(conn_handle);
#endif
    case BLE_AUDIO_MCP_READ_PLAYER_NAME: return esp_ble_audio_mcc_read_player_name(conn_handle);
#if CONFIG_BT_MCC_READ_TRACK_TITLE
    case BLE_AUDIO_MCP_READ_TRACK_TITLE: return esp_ble_audio_mcc_read_track_title(conn_handle);
#endif
    default: return ESP_ERR_NOT_SUPPORTED;
  }
}

#else

int bleAudioMcpCliInit(void) {
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioMcpCliDiscover(uint16_t conn_handle) {
  (void)conn_handle;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioMcpCliCommand(uint16_t conn_handle, uint8_t opcode) {
  (void)conn_handle;
  (void)opcode;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioMcpCliRead(uint16_t conn_handle, uint8_t what) {
  (void)conn_handle;
  (void)what;
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* BLE_AUDIO_MCP_CLIENT_SUPPORTED */

/* ── CCP call control server ────────────────────────────────────────────── */

#if BLE_AUDIO_CCP_SERVER_SUPPORTED

/**
 * A client asked GTBS to place a call. The handler runs synchronously inside
 * the emit and may clear `accept`; returning false makes the stack reject
 * the request.
 */
static bool tbs_originate(esp_ble_conn_t *conn, uint8_t call_index, const char *uri) {
  bool accept = true;
  const ble_audio_ccp_originate_t d = {.call_index = call_index, .uri = uri, .accept = &accept};
  bleAudioEngineEmit(BLE_AUDIO_EVT_CCP_SRV_ORIGINATE, CONN_OF(conn), 0, &d);
  return accept;
}

/** Report a call operation performed by a client; the per-operation stack callbacks below funnel here. */
static void tbs_call(esp_ble_conn_t *conn, uint8_t op, uint8_t call_index, uint8_t reason) {
  const ble_audio_ccp_call_t d = {.op = op, .call_index = call_index, .reason = reason};
  bleAudioEngineEmit(BLE_AUDIO_EVT_CCP_SRV_CALL, CONN_OF(conn), 0, &d);
}

static void tbs_terminated(esp_ble_conn_t *conn, uint8_t call_index, uint8_t reason) {
  tbs_call(conn, BLE_AUDIO_CCP_OP_TERMINATE, call_index, reason);
}

static void tbs_accepted(esp_ble_conn_t *conn, uint8_t call_index) {
  tbs_call(conn, BLE_AUDIO_CCP_OP_ACCEPT, call_index, 0);
}

static void tbs_held(esp_ble_conn_t *conn, uint8_t call_index) {
  tbs_call(conn, BLE_AUDIO_CCP_OP_HOLD, call_index, 0);
}

static void tbs_retrieved(esp_ble_conn_t *conn, uint8_t call_index) {
  tbs_call(conn, BLE_AUDIO_CCP_OP_RETRIEVE, call_index, 0);
}

static esp_ble_audio_tbs_cb_t s_tbs_cb = {
  .originate_call = tbs_originate,
  .terminate_call = tbs_terminated,
  .hold_call = tbs_held,
  .accept_call = tbs_accepted,
  .retrieve_call = tbs_retrieved,
};

int bleAudioCcpSrvInit(const char *provider_name, const char *uci, const char *uri_schemes) {
  if (s_roles & ROLE_CCP_SRV) {
    return ESP_OK;
  }
  int err = role_attach();
  if (err == ESP_OK) {
    err = esp_ble_audio_tbs_register_cb(&s_tbs_cb);
  }
  /* A single generic bearer (GTBS) that any client may control without authorization. */
  if (err == ESP_OK) {
    const esp_ble_audio_tbs_register_param_t p = {
      .provider_name = (char *)provider_name,
      .uci = (char *)uci,
      .uri_schemes_supported = (char *)uri_schemes,
      .gtbs = true,
      .authorization_required = false,
      .technology = ESP_BLE_AUDIO_TBS_TECHNOLOGY_LTE,
      .supported_features = ESP_BLE_AUDIO_TBS_FEATURE_ALL,
    };
    err = esp_ble_audio_tbs_register_bearer(&p, &s_tbs_bearer);
  }
  return role_done(ROLE_CCP_SRV, err);
}

int bleAudioCcpSrvIncoming(const char *from, const char *friendly_name, uint8_t *call_index) {
  NEED_ROLE(ROLE_CCP_SRV);
  if (!from) {
    return ESP_ERR_INVALID_ARG;
  }
  /* "tel:server" is the local (target) URI; the caller name defaults to the caller URI. */
  return esp_ble_audio_tbs_remote_incoming(s_tbs_bearer, "tel:server", from, friendly_name ? friendly_name : from, call_index);
}

int bleAudioCcpSrvCallOp(uint8_t op, uint8_t call_index) {
  NEED_ROLE(ROLE_CCP_SRV);
  switch (op) {
    case BLE_AUDIO_CCP_OP_ACCEPT:           return esp_ble_audio_tbs_accept(call_index);
    case BLE_AUDIO_CCP_OP_TERMINATE:        return esp_ble_audio_tbs_terminate(call_index);
    case BLE_AUDIO_CCP_OP_HOLD:             return esp_ble_audio_tbs_hold(call_index);
    case BLE_AUDIO_CCP_OP_RETRIEVE:         return esp_ble_audio_tbs_retrieve(call_index);
    case BLE_AUDIO_CCP_OP_REMOTE_ANSWER:    return esp_ble_audio_tbs_remote_answer(call_index);
    case BLE_AUDIO_CCP_OP_REMOTE_TERMINATE: return esp_ble_audio_tbs_remote_terminate(call_index);
    default:                                return ESP_ERR_INVALID_ARG;
  }
}

int bleAudioCcpSrvSetProviderName(const char *name) {
  NEED_ROLE(ROLE_CCP_SRV);
  return esp_ble_audio_tbs_set_bearer_provider_name(s_tbs_bearer, name);
}

#else

int bleAudioCcpSrvInit(const char *provider_name, const char *uci, const char *uri_schemes) {
  (void)provider_name;
  (void)uci;
  (void)uri_schemes;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioCcpSrvIncoming(const char *from, const char *friendly_name, uint8_t *call_index) {
  (void)from;
  (void)friendly_name;
  (void)call_index;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioCcpSrvCallOp(uint8_t op, uint8_t call_index) {
  (void)op;
  (void)call_index;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioCcpSrvSetProviderName(const char *name) {
  (void)name;
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* BLE_AUDIO_CCP_SERVER_SUPPORTED */

/* ── CCP call control client ────────────────────────────────────────────── */

#if BLE_AUDIO_CCP_CLIENT_SUPPORTED

/*
 * The client drives only the peer's GTBS instance; events for individual TBS
 * instances are ignored so every call is reported exactly once.
 */

/** Discovery finished; read the GTBS call list so CCP_CLI_CALL events follow. */
static void tbsc_discovered(esp_ble_conn_t *conn, int err, uint8_t tbs_count, bool gtbs_found) {
  const uint16_t h = CONN_OF(conn);
  const ble_audio_ccp_discovered_t d = {.tbs_count = tbs_count, .gtbs = gtbs_found};
  bleAudioEngineEmit(BLE_AUDIO_EVT_CCP_CLI_DISCOVERED, h, err, &d);
  if (err == 0 && gtbs_found) {
    (void)esp_ble_audio_tbs_client_read_call_state(h, ESP_BLE_AUDIO_TBS_GTBS_INDEX);
  }
}

/** Result of a call control point write; the per-operation stack callbacks below funnel here. */
static void tbsc_result(esp_ble_conn_t *conn, int err, uint8_t op, uint8_t call_index) {
  const ble_audio_ccp_call_t d = {.op = op, .call_index = call_index};
  bleAudioEngineEmit(BLE_AUDIO_EVT_CCP_CLI_RESULT, CONN_OF(conn), err, &d);
}

static void tbsc_originated(esp_ble_conn_t *conn, int err, uint8_t inst_index, uint8_t call_index) {
  (void)inst_index;
  tbsc_result(conn, err, BLE_AUDIO_CCP_OP_ORIGINATE, call_index);
}

static void tbsc_terminated(esp_ble_conn_t *conn, int err, uint8_t inst_index, uint8_t call_index) {
  (void)inst_index;
  tbsc_result(conn, err, BLE_AUDIO_CCP_OP_TERMINATE, call_index);
}

static void tbsc_accepted(esp_ble_conn_t *conn, int err, uint8_t inst_index, uint8_t call_index) {
  (void)inst_index;
  tbsc_result(conn, err, BLE_AUDIO_CCP_OP_ACCEPT, call_index);
}

static void tbsc_held(esp_ble_conn_t *conn, int err, uint8_t inst_index, uint8_t call_index) {
  (void)inst_index;
  tbsc_result(conn, err, BLE_AUDIO_CCP_OP_HOLD, call_index);
}

static void tbsc_retrieved(esp_ble_conn_t *conn, int err, uint8_t inst_index, uint8_t call_index) {
  (void)inst_index;
  tbsc_result(conn, err, BLE_AUDIO_CCP_OP_RETRIEVE, call_index);
}

/** Call list read or notified: one CCP_CLI_CALL per call. */
static void tbsc_call_states(esp_ble_conn_t *conn, int err, uint8_t inst_index, uint8_t call_count, const esp_ble_audio_tbs_client_call_state_t *states) {
  if (inst_index != ESP_BLE_AUDIO_TBS_GTBS_INDEX) {
    return;
  }
  for (uint8_t i = 0; i < call_count; i++) {
    const ble_audio_ccp_call_state_t d = {.call_index = states[i].index, .state = states[i].state};
    bleAudioEngineEmit(BLE_AUDIO_EVT_CCP_CLI_CALL, CONN_OF(conn), err, &d);
  }
}

/** A call was terminated; the call list no longer carries it, so report the pseudo state ENDED. */
static void tbsc_ended(esp_ble_conn_t *conn, int err, uint8_t inst_index, uint8_t call_index, uint8_t reason) {
  (void)reason;
  if (inst_index == ESP_BLE_AUDIO_TBS_GTBS_INDEX) {
    const ble_audio_ccp_call_state_t d = {.call_index = call_index, .state = BLE_AUDIO_CCP_CALL_ENDED};
    bleAudioEngineEmit(BLE_AUDIO_EVT_CCP_CLI_CALL, CONN_OF(conn), err, &d);
  }
}

static esp_ble_audio_tbs_client_cb_t s_tbsc_cb = {
  .discover = tbsc_discovered,
  .originate_call = tbsc_originated,
  .terminate_call = tbsc_terminated,
  .hold_call = tbsc_held,
  .accept_call = tbsc_accepted,
  .retrieve_call = tbsc_retrieved,
  .call_state = tbsc_call_states,
  .termination_reason = tbsc_ended,
};

int bleAudioCcpCliInit(void) {
  if (s_roles & ROLE_CCP_CLI) {
    return ESP_OK;
  }
  int err = role_attach();
  if (err == ESP_OK) {
    /* Fails only when the static table is still listed from an earlier session. */
    (void)esp_ble_audio_tbs_client_register_cb(&s_tbsc_cb);
  }
  return role_done(ROLE_CCP_CLI, err);
}

int bleAudioCcpCliDiscover(uint16_t conn_handle) {
  NEED_ROLE(ROLE_CCP_CLI);
  return esp_ble_audio_tbs_client_discover(conn_handle);
}

int bleAudioCcpCliOriginate(uint16_t conn_handle, const char *uri) {
  NEED_ROLE(ROLE_CCP_CLI);
#if CONFIG_BT_TBS_CLIENT_ORIGINATE_CALL
  return esp_ble_audio_tbs_client_originate_call(conn_handle, ESP_BLE_AUDIO_TBS_GTBS_INDEX, uri);
#else
  (void)conn_handle;
  (void)uri;
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

int bleAudioCcpCliCallOp(uint16_t conn_handle, uint8_t op, uint8_t call_index) {
  NEED_ROLE(ROLE_CCP_CLI);
  const uint8_t inst = ESP_BLE_AUDIO_TBS_GTBS_INDEX;
  /* Every operation is optional in the stack (CONFIG_BT_TBS_CLIENT_*); silence unused warnings when none are. */
  (void)conn_handle;
  (void)call_index;
  (void)inst;
  switch (op) {
#if CONFIG_BT_TBS_CLIENT_ACCEPT_CALL
    case BLE_AUDIO_CCP_OP_ACCEPT: return esp_ble_audio_tbs_client_accept_call(conn_handle, inst, call_index);
#endif
#if CONFIG_BT_TBS_CLIENT_TERMINATE_CALL
    case BLE_AUDIO_CCP_OP_TERMINATE: return esp_ble_audio_tbs_client_terminate_call(conn_handle, inst, call_index);
#endif
#if CONFIG_BT_TBS_CLIENT_HOLD_CALL
    case BLE_AUDIO_CCP_OP_HOLD: return esp_ble_audio_tbs_client_hold_call(conn_handle, inst, call_index);
#endif
#if CONFIG_BT_TBS_CLIENT_RETRIEVE_CALL
    case BLE_AUDIO_CCP_OP_RETRIEVE: return esp_ble_audio_tbs_client_retrieve_call(conn_handle, inst, call_index);
#endif
    default: return ESP_ERR_NOT_SUPPORTED;
  }
}

int bleAudioCcpCliReadCalls(uint16_t conn_handle) {
  NEED_ROLE(ROLE_CCP_CLI);
  return esp_ble_audio_tbs_client_read_call_state(conn_handle, ESP_BLE_AUDIO_TBS_GTBS_INDEX);
}

#else

int bleAudioCcpCliInit(void) {
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioCcpCliDiscover(uint16_t conn_handle) {
  (void)conn_handle;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioCcpCliOriginate(uint16_t conn_handle, const char *uri) {
  (void)conn_handle;
  (void)uri;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioCcpCliCallOp(uint16_t conn_handle, uint8_t op, uint8_t call_index) {
  (void)conn_handle;
  (void)op;
  (void)call_index;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioCcpCliReadCalls(uint16_t conn_handle) {
  (void)conn_handle;
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* BLE_AUDIO_CCP_CLIENT_SUPPORTED */

/* ── HAS hearing aid ────────────────────────────────────────────────────── */

#if BLE_AUDIO_HAS_SUPPORTED

/** A client selected a preset; returning 0 lets the stack activate it and notify clients. */
static int has_select(uint8_t index, bool sync) {
  const ble_audio_has_select_t d = {.index = index, .sync = sync};
  bleAudioEngineEmit(BLE_AUDIO_EVT_HAS_SRV_SELECT, CONN_NONE, 0, &d);
  return 0; /* the stack activates the preset */
}

/** A preset name changed (client write or local rename). */
static void has_name_changed(uint8_t index, const char *name) {
  const ble_audio_has_preset_t d = {.index = index, .name = name};
  bleAudioEngineEmit(BLE_AUDIO_EVT_HAS_SRV_NAME, CONN_NONE, 0, &d);
}

static const esp_ble_audio_has_preset_ops_t s_has_ops = {.select = has_select, .name_changed = has_name_changed};

#if HAS_KEEP_NAMES
/**
 * Store @p name in the slot for @p index (reusing it, or taking a free one)
 * so the pointer handed to the stack stays valid. Passing NULL frees the slot.
 * @return the stored copy, or NULL when freed or when every slot is taken.
 */
static const char *has_keep_name(uint8_t index, const char *name) {
  int slot = -1;
  for (int i = 0; i < CONFIG_BT_HAS_PRESET_COUNT; i++) {
    if (s_has_names[i].index == index) {
      slot = i;
      break;
    }
    if (slot < 0 && s_has_names[i].index == 0) {
      slot = i;
    }
  }
  if (slot < 0) {
    return NULL;
  }
  if (!name) {
    s_has_names[slot].index = 0;
    return NULL;
  }
  s_has_names[slot].index = index;
  strlcpy(s_has_names[slot].name, name, sizeof(s_has_names[slot].name));
  return s_has_names[slot].name;
}
#endif

int bleAudioHasSrvInit(uint8_t type, bool preset_sync) {
  if (s_roles & ROLE_HAS_SRV) {
    return ESP_OK;
  }
  int err = role_attach();
  if (err == ESP_OK) {
    /* Presets are shared across a binaural set (not independent per device). */
    const esp_ble_audio_has_features_param_t f = {
      .type = (esp_ble_audio_has_hearing_aid_type_t)type,
      .preset_sync_support = preset_sync,
      .independent_presets = false,
    };
    err = esp_ble_audio_has_register(&f);
  }
  return role_done(ROLE_HAS_SRV, err);
}

int bleAudioHasSrvAddPreset(uint8_t index, const char *name, bool available, bool writable) {
  NEED_ROLE(ROLE_HAS_SRV);
  if (!name) {
    return ESP_ERR_INVALID_ARG;
  }
#if HAS_KEEP_NAMES
  name = has_keep_name(index, name);
  if (!name) {
    ESP_LOGE(TAG, "hearing aid: no name slot for preset %u (CONFIG_BT_HAS_PRESET_COUNT)", index);
    return ESP_ERR_NO_MEM;
  }
#endif
  const esp_ble_audio_has_preset_register_param_t p = {
    .index = index,
    .properties = (esp_ble_audio_has_properties_t)((available ? ESP_BLE_AUDIO_HAS_PROP_AVAILABLE : 0) | (writable ? ESP_BLE_AUDIO_HAS_PROP_WRITABLE : 0)),
    .name = name,
    .ops = &s_has_ops,
  };
  return esp_ble_audio_has_preset_register(&p);
}

int bleAudioHasSrvRemovePreset(uint8_t index) {
  NEED_ROLE(ROLE_HAS_SRV);
  int err = esp_ble_audio_has_preset_unregister(index);
#if HAS_KEEP_NAMES
  if (err == ESP_OK) {
    (void)has_keep_name(index, NULL);
  }
#endif
  return err;
}

int bleAudioHasSrvSetAvailable(uint8_t index, bool available) {
  NEED_ROLE(ROLE_HAS_SRV);
  return available ? esp_ble_audio_has_preset_available(index) : esp_ble_audio_has_preset_unavailable(index);
}

int bleAudioHasSrvRename(uint8_t index, const char *name) {
  NEED_ROLE(ROLE_HAS_SRV);
  return esp_ble_audio_has_preset_name_change(index, name);
}

int bleAudioHasSrvSetActive(uint8_t index) {
  NEED_ROLE(ROLE_HAS_SRV);
  return esp_ble_audio_has_preset_active_set(index);
}

uint8_t bleAudioHasSrvGetActive(void) {
  return (s_roles & ROLE_HAS_SRV) ? esp_ble_audio_has_preset_active_get() : 0;
}

#else

int bleAudioHasSrvInit(uint8_t type, bool preset_sync) {
  (void)type;
  (void)preset_sync;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioHasSrvAddPreset(uint8_t index, const char *name, bool available, bool writable) {
  (void)index;
  (void)name;
  (void)available;
  (void)writable;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioHasSrvRemovePreset(uint8_t index) {
  (void)index;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioHasSrvSetAvailable(uint8_t index, bool available) {
  (void)index;
  (void)available;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioHasSrvRename(uint8_t index, const char *name) {
  (void)index;
  (void)name;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioHasSrvSetActive(uint8_t index) {
  (void)index;
  return ESP_ERR_NOT_SUPPORTED;
}

uint8_t bleAudioHasSrvGetActive(void) {
  return 0;
}

#endif /* BLE_AUDIO_HAS_SUPPORTED */

/* ── HAS hearing aid controller ─────────────────────────────────────────── */

#if BLE_AUDIO_HAS_CLIENT_SUPPORTED

/** Map a stack HAS instance to the tracked peer, CONN_NONE if foreign. */
static uint16_t hasc_conn(const esp_ble_audio_has_t *has) {
  return (has && has == s_hasc.has) ? s_hasc.conn : CONN_NONE;
}

/**
 * Discovery finished. Unlike VCP/MICP the stack hands out the HAS instance
 * only here, so the peer is matched by connection handle instead.
 */
static void hasc_discover(
  esp_ble_conn_t *conn, int err, esp_ble_audio_has_t *has, esp_ble_audio_has_hearing_aid_type_t type, esp_ble_audio_has_capabilities_t caps
) {
  (void)caps;
  const uint16_t h = CONN_OF(conn);
  if (h != s_hasc.conn) {
    return;
  }
  if (err == 0) {
    s_hasc.has = has;
  }
  const uint8_t t = (uint8_t)type;
  bleAudioEngineEmit(BLE_AUDIO_EVT_HAS_CLI_DISCOVERED, h, err, &t);
}

/** The peer's active preset changed (or a set/step request failed). */
static void hasc_switch(esp_ble_audio_has_t *has, int err, uint8_t index) {
  bleAudioEngineEmit(BLE_AUDIO_EVT_HAS_CLI_ACTIVE, hasc_conn(has), err, &index);
}

/** One preset record from a read; @p record is NULL when the read failed. */
static void hasc_preset(esp_ble_audio_has_t *has, int err, const esp_ble_audio_has_preset_record_t *record, bool is_last) {
  ble_audio_has_preset_t d = {.is_last = is_last};
  if (record) {
    d.index = record->index;
    d.available = (record->properties & ESP_BLE_AUDIO_HAS_PROP_AVAILABLE) != 0;
    d.writable = (record->properties & ESP_BLE_AUDIO_HAS_PROP_WRITABLE) != 0;
    d.name = record->name;
  }
  bleAudioEngineEmit(BLE_AUDIO_EVT_HAS_CLI_PRESET, hasc_conn(has), err, &d);
}

/** A preset was added or changed on the peer; reported the same way as a read record. */
static void hasc_update(esp_ble_audio_has_t *has, uint8_t index_prev, const esp_ble_audio_has_preset_record_t *record, bool is_last) {
  (void)index_prev;
  hasc_preset(has, 0, record, is_last);
}

static const esp_ble_audio_has_client_cb_t s_hasc_cb = {
  .discover = hasc_discover,
  .preset_switch = hasc_switch,
  .preset_read_rsp = hasc_preset,
  .preset_update = hasc_update,
};

int bleAudioHasCliInit(void) {
  if (s_roles & ROLE_HAS_CLI) {
    return ESP_OK;
  }
  int err = role_attach();
  if (err == ESP_OK) {
    /* Fails only when the static table is still listed from an earlier session. */
    (void)esp_ble_audio_has_client_cb_register(&s_hasc_cb);
  }
  return role_done(ROLE_HAS_CLI, err);
}

int bleAudioHasCliDiscover(uint16_t conn_handle) {
  NEED_ROLE(ROLE_HAS_CLI);
  PEER_RESET(s_hasc);
  /* Set before the call: hasc_discover matches on it and may run before we return. */
  s_hasc.conn = conn_handle;
  int err = esp_ble_audio_has_client_discover(conn_handle);
  if (err != ESP_OK) {
    s_hasc.conn = CONN_NONE;
  }
  return err;
}

/** HAS instance for @p conn_handle, NULL until the discovered peer's discovery succeeded. */
static esp_ble_audio_has_t *hasc_get(uint16_t conn_handle) {
  return conn_handle == s_hasc.conn ? s_hasc.has : NULL;
}

int bleAudioHasCliReadPresets(uint16_t conn_handle, uint8_t start_index, uint8_t max_count) {
  esp_ble_audio_has_t *has = hasc_get(conn_handle);
  return has ? esp_ble_audio_has_client_presets_read(has, start_index, max_count) : ESP_ERR_INVALID_STATE;
}

int bleAudioHasCliSetActive(uint16_t conn_handle, uint8_t index, bool sync) {
  esp_ble_audio_has_t *has = hasc_get(conn_handle);
  return has ? esp_ble_audio_has_client_preset_set(has, index, sync) : ESP_ERR_INVALID_STATE;
}

int bleAudioHasCliStep(uint16_t conn_handle, bool next, bool sync) {
  esp_ble_audio_has_t *has = hasc_get(conn_handle);
  if (!has) {
    return ESP_ERR_INVALID_STATE;
  }
  return next ? esp_ble_audio_has_client_preset_next(has, sync) : esp_ble_audio_has_client_preset_prev(has, sync);
}

int bleAudioHasCliRename(uint16_t conn_handle, uint8_t index, const char *name) {
  esp_ble_audio_has_t *has = hasc_get(conn_handle);
  return has ? esp_ble_audio_has_client_preset_name_write(has, index, name) : ESP_ERR_INVALID_STATE;
}

#else

int bleAudioHasCliInit(void) {
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioHasCliDiscover(uint16_t conn_handle) {
  (void)conn_handle;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioHasCliReadPresets(uint16_t conn_handle, uint8_t start_index, uint8_t max_count) {
  (void)conn_handle;
  (void)start_index;
  (void)max_count;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioHasCliSetActive(uint16_t conn_handle, uint8_t index, bool sync) {
  (void)conn_handle;
  (void)index;
  (void)sync;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioHasCliStep(uint16_t conn_handle, bool next, bool sync) {
  (void)conn_handle;
  (void)next;
  (void)sync;
  return ESP_ERR_NOT_SUPPORTED;
}

int bleAudioHasCliRename(uint16_t conn_handle, uint8_t index, const char *name) {
  (void)conn_handle;
  (void)index;
  (void)name;
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* BLE_AUDIO_HAS_CLIENT_SUPPORTED */

#endif /* BLE_AUDIO_SUPPORTED */
