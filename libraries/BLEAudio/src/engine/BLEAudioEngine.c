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
 * @file BLEAudioEngine.c
 * @brief Host-agnostic engine lifecycle, event bridge, link bring-up and BAP presets.
 *
 * Completely shared across NimBLE and Bluedroid: host-specific GAP/GATT/SMP
 * calls live in `BLEAudio.nimble.cpp` and `BLEAudio.bluedroid.cpp` behind the
 * `bleAudioHost*` / `bleAudioEngine*` C hooks declared in `BLEAudioEngine.h`.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include "BLEAudioValidationHooks.h"
#include "engine/BLEAudioEngine.h"
#include "iso/BLEIsoHost.h"

#include "esp_ble_audio_common_api.h"
#include "esp_ble_audio_defs.h"
#include "esp32-hal-log.h"

#include <errno.h>
#include <stdatomic.h>
#include <string.h>

// --------------------------------------------------------------------------
// State
// --------------------------------------------------------------------------

static ble_audio_evt_sink_fn s_sink; /* BLEAudio::Impl::dispatch trampoline. */
static void *s_sink_ctx;
static _Atomic bool s_initialized;
static _Atomic bool s_started;

#if CONFIG_BT_CSIP_SET_MEMBER
/* CSIS instances registered by CAP acceptor / CSIP member, passed to common_start. */
static esp_ble_audio_start_info_t s_start_info;
static uint8_t s_csis_count;
#endif

/* One slot per engine unit (Bap, Assistant, Cap, Control, Profiles) plus one spare. */
#define MAX_UNITS 6
static const ble_audio_unit_hooks_t *s_units[MAX_UNITS];

// --------------------------------------------------------------------------
// Event bridge and unit hooks
// --------------------------------------------------------------------------

int bleAudioEngineRegisterUnit(const ble_audio_unit_hooks_t *hooks) {
  for (int i = 0; i < MAX_UNITS; i++) {
    if (s_units[i] == hooks) {
      return 0;
    }
  }
  for (int i = 0; i < MAX_UNITS; i++) {
    if (s_units[i] == NULL) {
      s_units[i] = hooks;
      return 0;
    }
  }
  log_e("no free unit hook slot (MAX_UNITS=%d)", MAX_UNITS);
  return ESP_ERR_NO_MEM;
}

void bleAudioEngineEmit(uint16_t type, uint16_t conn_handle, int err, const void *data) {
  if (!s_sink) {
    return;
  }
  const ble_audio_evt_t evt = {.type = type, .conn_handle = conn_handle, .err = err, .data = data};
  s_sink(&evt, s_sink_ctx);
}

// --------------------------------------------------------------------------
// BAP LC3 presets
// --------------------------------------------------------------------------

/** One preset row; the codec and QoS are derived from it by bleAudioPresetGet(). */
typedef struct {
  uint16_t rate_div100;     /* Sampling rate / 100; 441 = 44.1 kHz. */
  uint16_t sdu_interval_us; /* On-air SDU interval (not the LC3 frame length for 44.1 kHz). */
  uint8_t octets;           /* Octets per codec frame. */
  uint8_t rtn_unicast;      /* Retransmissions, BAP Table 5.2. */
  uint8_t rtn_broadcast;    /* Retransmissions, BAP Table 6.4. */
  uint8_t latency_ms;       /* Max transport latency. */
} preset_row_t;

/*
 * Low-latency LC3 presets, BAP v1.0.1 Table 5.2 (unicast) and 6.4 (broadcast).
 * Row order MUST match BLEAudioCodecPreset. 44.1 kHz rows are framed.
 */
static const preset_row_t s_presets[BLE_AUDIO_PRESET_COUNT] = {
  {80, 7500, 26, 2, 2, 8},   {80, 10000, 30, 2, 2, 10},   {160, 7500, 30, 2, 2, 8},   {160, 10000, 40, 2, 2, 10},
  {240, 7500, 45, 2, 2, 8},  {240, 10000, 60, 2, 2, 10},  {320, 7500, 60, 2, 2, 8},   {320, 10000, 80, 2, 2, 10},
  {441, 8163, 97, 5, 4, 24}, {441, 10884, 130, 5, 4, 31}, {480, 7500, 75, 5, 4, 15},  {480, 10000, 100, 5, 4, 20},
  {480, 7500, 90, 5, 4, 15}, {480, 10000, 120, 5, 4, 20}, {480, 7500, 117, 5, 4, 15}, {480, 10000, 155, 5, 4, 20},
};

/* Presets are mono (chan_alloc 0) with one frame block; roles set the allocation per stream. */
bool bleAudioPresetGet(uint8_t preset, bool broadcast, ble_audio_codec_t *codec, ble_audio_qos_t *qos) {
  if (preset >= BLE_AUDIO_PRESET_COUNT) {
    return false;
  }
  const preset_row_t *r = &s_presets[preset];
  const bool is441 = (r->rate_div100 == 441);
  if (codec) {
    codec->sample_rate_hz = is441 ? 44100 : (uint32_t)r->rate_div100 * 100;
    /* 44.1 kHz frames last 8163/10884 us on air but are 7.5/10 ms LC3 frames. */
    codec->frame_dur_us = (r->sdu_interval_us == 7500 || r->sdu_interval_us == 8163) ? 7500 : 10000;
    codec->octets_per_frame = r->octets;
    codec->frames_per_sdu = 1;
    codec->chan_alloc = 0;
  }
  if (qos) {
    qos->sdu_interval_us = r->sdu_interval_us;
    qos->max_sdu = r->octets;
    qos->rtn = broadcast ? r->rtn_broadcast : r->rtn_unicast;
    qos->latency_ms = r->latency_ms;
    qos->pd_us = 40000;
    qos->phy = BLE_AUDIO_PHY_2M;
    qos->framed = is441;
  }
  return true;
}

uint8_t bleAudioChannelCount(uint32_t chan_alloc) {
  return chan_alloc ? esp_ble_audio_get_chan_count((esp_ble_audio_location_t)chan_alloc) : 1;
}

// --------------------------------------------------------------------------
// Links opened by bleAudioEngineConnect()
// --------------------------------------------------------------------------

#define MAX_LINKS        BLE_MAX_CONN
#define HCI_ROLE_CENTRAL 0x00
static bool s_connect_pending; /* bleAudioEngineConnect() armed; the next central ACL is ours. */
static struct bt_le_addr s_connect_target;
static bool s_connect_target_valid;
static bool s_peer_discovery;

static uint16_t s_links[MAX_LINKS]; /* Reset to BLE_AUDIO_CONN_NONE in engine_reset_local(); zero-fill + reset covers boot. */
static struct bt_le_addr s_link_peer[MAX_LINKS];
static bool s_link_peer_valid[MAX_LINKS];
static bool s_link_central[MAX_LINKS];
static bool s_link_mtu_ok[MAX_LINKS];
static bool s_link_secured[MAX_LINKS];
static bool s_link_discovered[MAX_LINKS];

static uint16_t *link_find(uint16_t conn) {
  for (int i = 0; i < MAX_LINKS; i++) {
    if (s_links[i] == conn) {
      return &s_links[i];
    }
  }
  return NULL;
}

static int link_index(uint16_t conn) {
  for (int i = 0; i < MAX_LINKS; i++) {
    if (s_links[i] == conn) {
      return i;
    }
  }
  return -1;
}

static void link_maybe_discover(int idx) {
  if (idx < 0 || !s_link_central[idx] || s_link_discovered[idx] || !s_link_mtu_ok[idx] || !s_link_secured[idx]) {
    return;
  }
  s_link_discovered[idx] = true;
  bleAudioEngineGattcDiscStart(s_links[idx]);
}

static void link_on_security(uint16_t conn);

static bool link_matches_target(const struct bt_le_addr *peer) {
  if (!s_connect_target_valid) {
    return true;
  }
  if (peer == NULL) {
    return false;
  }
  struct bt_le_addr addr;
  bleAudioAddrFromGap(addr.val, peer->val);
  return memcmp(addr.val, s_connect_target.val, 6) == 0;
}

static int link_record(uint16_t conn, const struct bt_le_addr *peer) {
  int idx = link_index(conn);
  if (idx < 0) {
    uint16_t *slot = link_find(BLE_AUDIO_CONN_NONE);
    if (slot == NULL) {
      return -1;
    }
    *slot = conn;
    idx = (int)(slot - s_links);
    s_link_central[idx] = false;
    s_link_mtu_ok[idx] = false;
    s_link_secured[idx] = false;
    s_link_discovered[idx] = false;
  }
  s_link_peer_valid[idx] = (peer != NULL);
  if (peer != NULL) {
    /* GAP event addresses are host-order; store the engine's LSB-first form. */
    bleAudioAddrFromGap(s_link_peer[idx].val, peer->val);
    s_link_peer[idx].type = peer->type;
  }
  return idx;
}

static void link_on_connect(uint16_t conn, uint8_t status, uint8_t role, const struct bt_le_addr *peer) {
  int idx = -1;
  if (status == 0) {
    idx = link_record(conn, peer);
  }
  if (!s_connect_pending || role != HCI_ROLE_CENTRAL) {
    return;
  }
  if (status == 0 && !link_matches_target(peer)) {
    log_d("conn %u: central ACL is not the audio target, ignoring", conn);
    return;
  }
  s_connect_pending = false;
  s_connect_target_valid = false;
  if (status != 0) {
    log_w("connect failed (status 0x%02x)", status);
    return;
  }
  if (idx < 0) {
    log_w("link table full (%d links); conn %u will not be brought up", MAX_LINKS, conn);
    return;
  }
  s_link_central[idx] = true;
  s_link_mtu_ok[idx] = false;
  s_link_secured[idx] = false;
  s_link_discovered[idx] = false;
  log_d("conn %u: connected, starting encryption", conn);
  static const uint8_t kZeroAddr[6] = {0};
  int err = bleAudioHostSecureLink(conn, peer ? peer->val : kZeroAddr, peer ? peer->type : 0);
  if (err != 0) {
    log_w("conn %u: encryption failed to start (err=%d), continuing unencrypted", conn, err);
    link_on_security(conn);
  }
}

static void link_on_security(uint16_t conn) {
  int idx = link_index(conn);
  if (idx < 0 || !s_link_central[idx]) {
    return;
  }
  s_link_secured[idx] = true;
  int err = bleAudioHostExchangeMtu(conn);
  if (err != 0) {
    log_w("conn %u: MTU exchange failed to start (err=%d)", conn, err);
  }
  link_maybe_discover(idx);
}

// --------------------------------------------------------------------------
// Engine host callbacks (Bluetooth host task)
// --------------------------------------------------------------------------

static void gap_cb(esp_ble_audio_gap_app_event_t *event) {
  if (event == NULL) {
    return;
  }
  bleIsoOnGapEvent(event);
  for (int i = 0; i < MAX_UNITS; i++) {
    if (s_units[i] && s_units[i]->on_gap) {
      s_units[i]->on_gap(event);
    }
  }

  switch (event->type) {
    case ESP_BLE_AUDIO_GAP_EVENT_ACL_CONNECT:
      link_on_connect(event->acl_connect.conn_handle, event->acl_connect.status, event->acl_connect.role, &event->acl_connect.dst);
      bleAudioEngineEmit(BLE_AUDIO_EVT_ACL_CONNECTED, event->acl_connect.conn_handle, event->acl_connect.status, NULL);
      break;
    case ESP_BLE_AUDIO_GAP_EVENT_ACL_DISCONNECT:
    {
      int idx = link_index(event->acl_disconnect.conn_handle);
      if (idx >= 0) {
        s_links[idx] = BLE_AUDIO_CONN_NONE;
        s_link_central[idx] = false;
        s_link_mtu_ok[idx] = false;
        s_link_secured[idx] = false;
        s_link_discovered[idx] = false;
        s_link_peer_valid[idx] = false;
      }
      bleAudioEngineEmit(BLE_AUDIO_EVT_ACL_DISCONNECTED, event->acl_disconnect.conn_handle, event->acl_disconnect.reason, NULL);
      break;
    }
    case ESP_BLE_AUDIO_GAP_EVENT_SECURITY_CHANGE:
      if (event->security_change.status != 0) {
        log_w("conn %u: encryption failed (status 0x%02x)", event->security_change.conn_handle, event->security_change.status);
      }
      link_on_security(event->security_change.conn_handle);
      bleAudioEngineEmit(BLE_AUDIO_EVT_SECURITY_CHANGED, event->security_change.conn_handle, event->security_change.status, NULL);
      break;
    default: break;
  }
}

static void gatt_cb(esp_ble_audio_gatt_app_event_t *event) {
  if (event == NULL) {
    return;
  }
  switch (event->type) {
    case ESP_BLE_AUDIO_GATT_EVENT_GATT_MTU_CHANGE:
      if (event->gatt_mtu_change.mtu >= ESP_BLE_AUDIO_ATT_MTU_MIN) {
        int idx = link_index(event->gatt_mtu_change.conn_handle);
        if (idx >= 0 && s_link_central[idx]) {
          s_link_mtu_ok[idx] = true;
          link_maybe_discover(idx);
        } else if (s_peer_discovery) {
          bleAudioEngineGattcDiscStart(event->gatt_mtu_change.conn_handle);
        } else {
          bleAudioEngineEmit(BLE_AUDIO_EVT_GATT_DISCOVERED, event->gatt_mtu_change.conn_handle, 0, NULL);
        }
      } else {
        log_w(
          "conn %u: MTU %u below the LE Audio minimum %u, not discovering", event->gatt_mtu_change.conn_handle, event->gatt_mtu_change.mtu,
          ESP_BLE_AUDIO_ATT_MTU_MIN
        );
      }
      break;
    case ESP_BLE_AUDIO_GATT_EVENT_GATTC_DISC_CMPL:
      if (event->gattc_disc_cmpl.status != 0) {
        log_w("conn %u: GATT discovery failed (status %d)", event->gattc_disc_cmpl.conn_handle, event->gattc_disc_cmpl.status);
      } else {
        log_d("conn %u: GATT discovery complete", event->gattc_disc_cmpl.conn_handle);
      }
      bleAudioEngineEmit(BLE_AUDIO_EVT_GATT_DISCOVERED, event->gattc_disc_cmpl.conn_handle, event->gattc_disc_cmpl.status, NULL);
      break;
    default: break;
  }
}

// --------------------------------------------------------------------------
// Lifecycle
// --------------------------------------------------------------------------

static void engine_reset_local(void);

int bleAudioEngineInit(ble_audio_evt_sink_fn sink, void *ctx) {
  if (s_initialized) {
    return 0;
  }
  engine_reset_local();
  if (bleIsoOwnsHost()) {
    log_e("a standalone BLEIso session owns the host; call BLEIso end() before audio begin()");
    return ESP_ERR_INVALID_STATE;
  }
  int host_err = bleAudioHostPrepareInit();
  if (host_err != 0) {
    return host_err;
  }
  s_sink = sink;
  s_sink_ctx = ctx;
  esp_ble_audio_init_info_t info = {.gap_cb = gap_cb, .gatt_cb = gatt_cb};
  esp_err_t err = esp_ble_audio_common_init(&info);
  if (err != ESP_OK) {
    log_e("esp_ble_audio_common_init failed: %d", err);
    s_sink = NULL;
    s_sink_ctx = NULL;
    return (int)err;
  }
  bleIsoAttachShared(true);
#if CONFIG_BT_CSIP_SET_MEMBER
  memset(&s_start_info, 0, sizeof(s_start_info));
  s_csis_count = 0;
#endif
  s_initialized = true;
  log_d("engine initialized");
  return 0;
}

int bleAudioEngineAddCsis(void *svc_inst, bool included_by_cas) {
#if CONFIG_BT_CSIP_SET_MEMBER
  if (svc_inst == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (s_csis_count >= CONFIG_BT_CSIP_SET_MEMBER_MAX_INSTANCE_COUNT) {
    log_e("more CSIS instances than BT_CSIP_SET_MEMBER_MAX_INSTANCE_COUNT (%d)", CONFIG_BT_CSIP_SET_MEMBER_MAX_INSTANCE_COUNT);
    return ESP_ERR_NO_MEM;
  }
  s_start_info.csis_insts[s_csis_count].svc_inst = (esp_ble_audio_csip_set_member_svc_inst_t *)svc_inst;
  s_start_info.csis_insts[s_csis_count].included_by_cas = included_by_cas;
  s_csis_count++;
  return 0;
#else
  (void)svc_inst;
  (void)included_by_cas;
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

int bleAudioEngineStart(void) {
  if (!s_initialized) {
    log_e("bleAudioEngineStart() called before bleAudioEngineInit()");
    return ESP_ERR_INVALID_STATE;
  }
  if (s_started) {
    return 0;
  }
  int prepare_err = bleAudioHostPrepareGatt();
  if (prepare_err != 0) {
    log_e("host GATT preparation failed: %d", prepare_err);
    return prepare_err;
  }
#if CONFIG_BT_CSIP_SET_MEMBER
  esp_err_t err = esp_ble_audio_common_start(&s_start_info);
#else
  esp_ble_audio_start_info_t info = {0};
  esp_err_t err = esp_ble_audio_common_start(&info);
#endif
  if (err != ESP_OK) {
    bleAudioHostFinishGatt(false);
    log_e("esp_ble_audio_common_start failed: %d", err);
    return (int)err;
  }
  bleAudioHostFinishGatt(true);
  s_started = true;
  log_d("engine started (GATT committed)");
  return 0;
}

static void engine_reset_local(void) {
  bleAudioHostResetGatt();
  bleIsoAttachShared(false);
  for (int i = 0; i < MAX_UNITS; i++) {
    if (s_units[i] && s_units[i]->on_deinit) {
      s_units[i]->on_deinit();
    }
    s_units[i] = NULL;
  }
  s_sink = NULL;
  s_sink_ctx = NULL;
  s_connect_pending = false;
  s_connect_target_valid = false;
  s_peer_discovery = false;
  for (int i = 0; i < MAX_LINKS; i++) {
    s_links[i] = BLE_AUDIO_CONN_NONE;
    s_link_central[i] = false;
    s_link_peer_valid[i] = false;
  }
  s_initialized = false;
  s_started = false;
}

int bleAudioEngineDeinit(void) {
  if (!s_initialized) {
    return 0;
  }
  esp_err_t err = esp_ble_audio_common_deinit(NULL);
  if (err != ESP_OK) {
    log_e("esp_ble_audio_common_deinit failed: %d (stop every stream first)", err);
    return (int)err;
  }
  engine_reset_local();
  log_d("engine deinitialized");
  return 0;
}

bool bleAudioEngineIsInitialized(void) {
  return s_initialized;
}

bool bleAudioEngineIsStarted(void) {
  return s_started;
}

// --------------------------------------------------------------------------
// Host glue
// --------------------------------------------------------------------------

void bleAudioEngineGattcDiscStart(uint16_t conn_handle) {
  if (s_initialized) {
    esp_err_t err = esp_ble_audio_gattc_disc_start(conn_handle);
    if (err != ESP_OK && err != -EALREADY) {
      log_w("conn %u: esp_ble_audio_gattc_disc_start failed: %d", conn_handle, err);
    }
  }
}

int bleAudioEngineConnect(uint8_t addr_type, const uint8_t addr[6]) {
  if (!s_initialized) {
    return ESP_ERR_INVALID_STATE;
  }
  memcpy(s_connect_target.val, addr, sizeof(s_connect_target.val));
  s_connect_target.type = addr_type;
  s_connect_target_valid = true;
  int err = bleAudioHostOpenLink(addr_type, addr);
  if (err == ESP_OK || err == ESP_ERR_NOT_SUPPORTED) {
    s_connect_pending = true;
  } else {
    s_connect_target_valid = false;
    s_connect_pending = false;
  }
  return err;
}

void bleAudioEngineCancelConnect(void) {
  s_connect_pending = false;
  s_connect_target_valid = false;
}

void bleAudioEngineEnablePeerDiscovery(void) {
  s_peer_discovery = true;
}

#if BLE_AUDIO_VALIDATION_HOOKS
bool bleAudioValidationArmConnect(const uint8_t addr[6], uint8_t addr_type) {
  if (addr == NULL) {
    return false;
  }
  memcpy(s_connect_target.val, addr, sizeof(s_connect_target.val));
  s_connect_target.type = addr_type;
  s_connect_target_valid = true;
  s_connect_pending = true;
  return true;
}

bool bleAudioValidationAcceptPeer(const uint8_t addr[6], uint8_t addr_type) {
  (void)addr_type;
  if (addr == NULL || !s_connect_target_valid) {
    return false;
  }
  return memcmp(addr, s_connect_target.val, 6) == 0;
}

void bleAudioValidationCancelConnect(void) {
  bleAudioEngineCancelConnect();
}
#endif

bool bleAudioEngineLinkAddress(uint16_t conn_handle, uint8_t addr[6]) {
  int idx = link_index(conn_handle);
  if (idx < 0 || !s_link_peer_valid[idx] || addr == NULL) {
    return false;
  }
  memcpy(addr, s_link_peer[idx].val, 6);
  return true;
}

void bleAudioEngineDisconnectAll(void) {
  for (int i = 0; i < MAX_LINKS; i++) {
    if (s_links[i] != BLE_AUDIO_CONN_NONE) {
      (void)bleAudioEngineDisconnect(s_links[i]);
    }
  }
}

void bleAudioEngineGapPostEvent(uint16_t type, void *param) {
  if (s_initialized) {
    esp_ble_audio_gap_app_post_event(type, param);
  }
}

#endif /* BLE_AUDIO_SUPPORTED */
