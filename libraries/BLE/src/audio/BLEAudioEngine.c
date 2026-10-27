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
 * @brief Engine lifecycle, event bridge, link bring-up and BAP presets.
 *
 * Fully shared across NimBLE and Bluedroid: the ESP-BLE-AUDIO API is
 * host-agnostic (IDF abstracts the host inside the engine), so only the few
 * places that must talk to the host directly carry a `BLE_NIMBLE` /
 * `BLE_BLUEDROID` branch.
 *
 * Responsibilities:
 *  - Lifecycle: `common_init` / `common_start` / `common_deinit`, and the
 *    hand-over of the host with the standalone ISO transport (`BLEIsoHost.h`).
 *  - Event bridge: the single C++ sink and bleAudioEngineEmit().
 *  - Unit hooks: the other engine units see GAP events and reset on deinit.
 *  - Links: the encryption -> MTU -> discovery bring-up of links opened by
 *    bleAudioEngineConnect(), and the generic MTU -> discovery rule.
 *  - Presets: the BAP LC3 preset table behind `BLEAudioCodecPreset`.
 *
 * API contract is documented in `BLEAudioEngine.h`; the definitions below
 * carry implementation notes only.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include "audio/BLEAudioEngine.h"
#include "iso/BLEIsoHost.h"

#include "esp_ble_audio_common_api.h"
#include "esp_ble_audio_defs.h"
#include "esp_log.h"
#if BLE_BLUEDROID
#include "esp_gattc_api.h"
#include "esp_gap_ble_api.h"
#endif

#include <errno.h>
#include <string.h>

static const char *TAG = "BLEAudio";

// --------------------------------------------------------------------------
// State
// --------------------------------------------------------------------------

static ble_audio_evt_sink_fn s_sink;  /* BLEAudio::Impl::dispatch trampoline. */
static void *s_sink_ctx;
static bool s_initialized;
static bool s_started;

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
  ESP_LOGE(TAG, "no free unit hook slot (MAX_UNITS=%d)", MAX_UNITS);
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
  {80, 7500, 26, 2, 2, 8},     {80, 10000, 30, 2, 2, 10},   {160, 7500, 30, 2, 2, 8},    {160, 10000, 40, 2, 2, 10},
  {240, 7500, 45, 2, 2, 8},    {240, 10000, 60, 2, 2, 10},  {320, 7500, 60, 2, 2, 8},    {320, 10000, 80, 2, 2, 10},
  {441, 8163, 97, 5, 4, 24},   {441, 10884, 130, 5, 4, 31}, {480, 7500, 75, 5, 4, 15},   {480, 10000, 100, 5, 4, 20},
  {480, 7500, 90, 5, 4, 15},   {480, 10000, 120, 5, 4, 20}, {480, 7500, 117, 5, 4, 15},  {480, 10000, 155, 5, 4, 20},
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

/*
 * Like the IDF central examples, the engine brings these links up itself:
 *   ACL connected (central) -> start encryption
 *   security changed        -> NimBLE: exchange MTU / Bluedroid: discover
 *   MTU changed             -> discover (gatt_cb, for every link)
 *   discovery complete      -> BLE_AUDIO_EVT_GATT_DISCOVERED -> onLinkReady()
 * Bluedroid exchanges the MTU itself when the engine's GATTC opens the link.
 * Links the peer opens (we are peripheral) are not tracked here; the central
 * drives their security, and the MTU rule still starts discovery.
 */
#define MAX_LINKS 4
#define HCI_ROLE_CENTRAL 0x00
static bool s_connect_pending; /* bleAudioEngineConnect() armed; the next central ACL is ours. */
#if BLE_BLUEDROID
static esp_bd_addr_t s_connect_bda; /* esp_ble_set_encryption() takes the address, not the handle. */
#endif
static uint16_t s_links[MAX_LINKS] = {BLE_AUDIO_CONN_NONE, BLE_AUDIO_CONN_NONE, BLE_AUDIO_CONN_NONE, BLE_AUDIO_CONN_NONE};

/** @return The slot holding @p conn (pass BLE_AUDIO_CONN_NONE to find a free one), or NULL. */
static uint16_t *link_find(uint16_t conn) {
  for (int i = 0; i < MAX_LINKS; i++) {
    if (s_links[i] == conn) {
      return &s_links[i];
    }
  }
  return NULL;
}

static void link_on_security(uint16_t conn);

/**
 * @brief ACL up: adopt the first central link after bleAudioEngineConnect() and encrypt it.
 *
 * PACS/ASCS require an encrypted link. When encryption cannot even start the
 * bring-up continues unencrypted, so peers without that requirement still work.
 */
static void link_on_connect(uint16_t conn, uint8_t status, uint8_t role) {
  if (!s_connect_pending || role != HCI_ROLE_CENTRAL) {
    return;
  }
  s_connect_pending = false;
  uint16_t *slot = link_find(BLE_AUDIO_CONN_NONE);
  if (status != 0) {
    ESP_LOGW(TAG, "connect failed (status 0x%02x)", status);
    return;
  }
  if (slot == NULL) {
    ESP_LOGW(TAG, "link table full (%d links); conn %u will not be brought up", MAX_LINKS, conn);
    return;
  }
  *slot = conn;
  ESP_LOGD(TAG, "conn %u: connected, starting encryption", conn);
#if BLE_BLUEDROID
  esp_err_t err = esp_ble_set_encryption(s_connect_bda, ESP_BLE_SEC_ENCRYPT_NO_MITM);
#else
  int err = bleAudioNimbleSecure(conn);
#endif
  if (err != 0) {
    ESP_LOGW(TAG, "conn %u: encryption failed to start (err=%d), continuing unencrypted", conn, err);
    link_on_security(conn);
  }
}

/** @brief Security settled on a tracked link: next step toward discovery. */
static void link_on_security(uint16_t conn) {
  if (link_find(conn) == NULL) {
    return;
  }
#if BLE_BLUEDROID
  bleAudioEngineGattcDiscStart(conn);
#else
  int err = bleAudioNimbleExchangeMtu(conn);
  if (err != 0) {
    ESP_LOGW(TAG, "conn %u: MTU exchange failed to start (err=%d)", conn, err);
  }
#endif
}

// --------------------------------------------------------------------------
// Engine host callbacks (Bluetooth host task)
// --------------------------------------------------------------------------

/**
 * @brief Every engine GAP event.
 *
 * Fan-out order: the ISO transport first (it tracks CIS/BIG state the audio
 * units rely on), then each unit's hook, then the link bring-up and the
 * CORE events for C++.
 */
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
      link_on_connect(event->acl_connect.conn_handle, event->acl_connect.status, event->acl_connect.role);
      bleAudioEngineEmit(BLE_AUDIO_EVT_ACL_CONNECTED, event->acl_connect.conn_handle, event->acl_connect.status, NULL);
      break;
    case ESP_BLE_AUDIO_GAP_EVENT_ACL_DISCONNECT: {
      uint16_t *link = link_find(event->acl_disconnect.conn_handle);
      if (link) {
        *link = BLE_AUDIO_CONN_NONE;
      }
      bleAudioEngineEmit(BLE_AUDIO_EVT_ACL_DISCONNECTED, event->acl_disconnect.conn_handle, event->acl_disconnect.reason, NULL);
      break;
    }
    case ESP_BLE_AUDIO_GAP_EVENT_SECURITY_CHANGE:
      /* Discovery continues even if encryption failed: some peers do not require it. */
      if (event->security_change.status != 0) {
        ESP_LOGW(TAG, "conn %u: encryption failed (status 0x%02x)", event->security_change.conn_handle, event->security_change.status);
      }
      link_on_security(event->security_change.conn_handle);
      bleAudioEngineEmit(BLE_AUDIO_EVT_SECURITY_CHANGED, event->security_change.conn_handle, event->security_change.status, NULL);
      break;
    default: break;
  }
}

/** @brief Engine GATT events: MTU drives discovery, discovery completion reaches C++. */
static void gatt_cb(esp_ble_audio_gatt_app_event_t *event) {
  if (event == NULL) {
    return;
  }
  switch (event->type) {
    case ESP_BLE_AUDIO_GATT_EVENT_GATT_MTU_CHANGE:
      /* Bluedroid exchanges the MTU itself on connect; both hosts report it here. */
      if (event->gatt_mtu_change.mtu >= ESP_BLE_AUDIO_ATT_MTU_MIN) {
        bleAudioEngineGattcDiscStart(event->gatt_mtu_change.conn_handle);
      } else {
        ESP_LOGW(TAG, "conn %u: MTU %u below the LE Audio minimum %u, not discovering", event->gatt_mtu_change.conn_handle,
                 event->gatt_mtu_change.mtu, ESP_BLE_AUDIO_ATT_MTU_MIN);
      }
      break;
    case ESP_BLE_AUDIO_GATT_EVENT_GATTC_DISC_CMPL:
      if (event->gattc_disc_cmpl.status != 0) {
        ESP_LOGW(TAG, "conn %u: GATT discovery failed (status %d)", event->gattc_disc_cmpl.conn_handle, event->gattc_disc_cmpl.status);
      } else {
        ESP_LOGD(TAG, "conn %u: GATT discovery complete", event->gattc_disc_cmpl.conn_handle);
      }
      bleAudioEngineEmit(BLE_AUDIO_EVT_GATT_DISCOVERED, event->gattc_disc_cmpl.conn_handle, event->gattc_disc_cmpl.status, NULL);
      break;
    default: break;
  }
}

// --------------------------------------------------------------------------
// Lifecycle
// --------------------------------------------------------------------------

/**
 * `common_init` runs the engine's GAP/GATT service init and stages the audio
 * profiles, but defers the GATT commit to common_start. On NimBLE the
 * library's GATT coordinator observes bleAudioEngineIsInitialized() to know
 * that `BLEServer` services must only be staged until then.
 */
int bleAudioEngineInit(ble_audio_evt_sink_fn sink, void *ctx) {
  if (s_initialized) {
    return 0;
  }
  if (bleIsoOwnsHost()) {
    ESP_LOGE(TAG, "a standalone BLEIso session owns the host; call BLEIso end() before audio begin()");
    return ESP_ERR_INVALID_STATE;
  }
  s_sink = sink;
  s_sink_ctx = ctx;
  esp_ble_audio_init_info_t info = {.gap_cb = gap_cb, .gatt_cb = gatt_cb};
  esp_err_t err = esp_ble_audio_common_init(&info);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_audio_common_init failed: %d", err);
    s_sink = NULL;
    s_sink_ctx = NULL;
    return (int)err;
  }
  /* ISO now rides on the audio-owned host instead of initializing its own. */
  bleIsoAttachShared(true);
#if CONFIG_BT_CSIP_SET_MEMBER
  memset(&s_start_info, 0, sizeof(s_start_info));
  s_csis_count = 0;
#endif
  s_initialized = true;
  ESP_LOGD(TAG, "engine initialized");
  return 0;
}

int bleAudioEngineAddCsis(void *svc_inst, bool included_by_cas) {
#if CONFIG_BT_CSIP_SET_MEMBER
  if (svc_inst == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  if (s_csis_count >= CONFIG_BT_CSIP_SET_MEMBER_MAX_INSTANCE_COUNT) {
    ESP_LOGE(TAG, "more CSIS instances than BT_CSIP_SET_MEMBER_MAX_INSTANCE_COUNT (%d)", CONFIG_BT_CSIP_SET_MEMBER_MAX_INSTANCE_COUNT);
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

/* common_start performs the single GATT commit (audio + staged BLEServer services). */
int bleAudioEngineStart(void) {
  if (!s_initialized) {
    ESP_LOGE(TAG, "bleAudioEngineStart() called before bleAudioEngineInit()");
    return ESP_ERR_INVALID_STATE;
  }
  if (s_started) {
    return 0;
  }
#if CONFIG_BT_CSIP_SET_MEMBER
  esp_err_t err = esp_ble_audio_common_start(&s_start_info);
#else
  esp_ble_audio_start_info_t info = {0};
  esp_err_t err = esp_ble_audio_common_start(&info);
#endif
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_audio_common_start failed: %d", err);
    return (int)err;
  }
  s_started = true;
  ESP_LOGD(TAG, "engine started (GATT committed)");
  return 0;
}

/**
 * `common_deinit` releases every profile, ISO and the host callbacks, and
 * refuses while a stream is up (nothing is released then). On success the
 * units reset their statics and are unregistered, so the next init starts
 * from a clean slate.
 */
int bleAudioEngineDeinit(void) {
  if (!s_initialized) {
    return 0;
  }
  esp_err_t err = esp_ble_audio_common_deinit(NULL);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_audio_common_deinit failed: %d (stop every stream first)", err);
    return (int)err;
  }
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
  for (int i = 0; i < MAX_LINKS; i++) {
    s_links[i] = BLE_AUDIO_CONN_NONE;
  }
  s_initialized = false;
  s_started = false;
  ESP_LOGD(TAG, "engine deinitialized");
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

/* NimBLE only: Bluedroid delivers GATT events to the engine's own GATT interfaces. */
void bleAudioEngineGattPostEvent(uint8_t type, void *event) {
#if BLE_NIMBLE
  if (s_initialized) {
    esp_ble_audio_gatt_app_post_event(type, event);
  }
#else
  (void)type;
  (void)event;
#endif
}

void bleAudioEngineGattcDiscStart(uint16_t conn_handle) {
  if (s_initialized) {
    esp_err_t err = esp_ble_audio_gattc_disc_start(conn_handle);
    /* A repeated start (MTU reported twice) is harmless. */
    if (err != ESP_OK && err != -EALREADY) {
      ESP_LOGW(TAG, "conn %u: esp_ble_audio_gattc_disc_start failed: %d", conn_handle, err);
    }
  }
}

/*
 * Only one connection attempt is tracked: a second call before the first ACL
 * comes up re-arms the same pending flag.
 */
int bleAudioEngineConnect(uint8_t addr_type, const uint8_t addr[6]) {
  if (!s_initialized) {
    return ESP_ERR_INVALID_STATE;
  }
#if BLE_BLUEDROID
  /* OPEN/CONNECT must reach the engine's GATTC interface, not the library's. */
  esp_gatt_if_t gattc_if = esp_ble_audio_bluedroid_get_gattc_if();
  if (gattc_if == ESP_GATT_IF_NONE) {
    ESP_LOGE(TAG, "engine GATTC interface not registered; cannot open the link");
    return ESP_ERR_INVALID_STATE;
  }
  memcpy(s_connect_bda, addr, sizeof(s_connect_bda));
  esp_err_t err = esp_ble_gattc_aux_open(gattc_if, s_connect_bda, (esp_ble_addr_type_t)addr_type, true);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gattc_aux_open failed: %d", err);
  }
  s_connect_pending = (err == ESP_OK);
  return err;
#else
  /* NimBLE: the caller opens the ACL with the host GAP; arm the bring-up for it. */
  (void)addr_type;
  (void)addr;
  s_connect_pending = true;
  return ESP_ERR_NOT_SUPPORTED;
#endif
}

void bleAudioEngineCancelConnect(void) {
  s_connect_pending = false;
}

/* Bluedroid only: the library owns the GAP callback and forwards AUTH_CMPL here. */
void bleAudioEngineGapPostEvent(uint16_t type, void *param) {
  if (s_initialized) {
    esp_ble_audio_gap_app_post_event(type, param);
  }
}

#endif /* BLE_AUDIO_SUPPORTED */
