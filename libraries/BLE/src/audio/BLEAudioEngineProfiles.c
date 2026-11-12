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
 * @file BLEAudioEngineProfiles.c
 * @brief Profiles engine unit: TMAP / GMAP identity services and PBP announcements.
 *
 * TMAP and GMAP are thin: register the local service, discover the peer's,
 * and emit one DISCOVERED event with the peer's roles (0 on failure). PBP
 * needs no stack state at all; it only builds and parses the announcement.
 *
 * API contract is documented on the declarations in
 * `BLEAudioEngineProfiles.h`; the definitions below carry implementation
 * notes only.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include "audio/BLEAudioEngineProfiles.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_ble_iso_common_api.h"

#if BLE_AUDIO_TMAP_SUPPORTED
#include "esp_ble_audio_tmap_api.h"
#endif
#if BLE_AUDIO_GMAP_SUPPORTED
#include "esp_ble_audio_gmap_api.h"
#endif
#if BLE_AUDIO_PBP_SUPPORTED
#include "esp_ble_audio_pbp_api.h"
#endif

#define CONN_NONE BLE_AUDIO_CONN_NONE

/* ── Unit hooks ─────────────────────────────────────────────────────────── */

#if BLE_AUDIO_TMAP_SUPPORTED || BLE_AUDIO_GMAP_SUPPORTED

static const char *TAG = "BLEAudioProfiles";

#if BLE_AUDIO_GMAP_SUPPORTED
static bool s_gmap_cb_registered;
/* Survives deinit: a callback still set in the stack makes a second register fail. */
static bool s_gmap_cb_linked;
#endif

/** Engine deinit hook: the next session registers the GMAP callbacks again. */
static void profiles_on_deinit(void) {
#if BLE_AUDIO_GMAP_SUPPORTED
  s_gmap_cb_registered = false;
#endif
}

static const ble_audio_unit_hooks_t s_hooks = {.on_gap = NULL, .on_deinit = profiles_on_deinit};

/** Require an initialised engine and register this unit's hooks (idempotent). */
static int attach(void) {
  return bleAudioEngineIsInitialized() ? bleAudioEngineRegisterUnit(&s_hooks) : ESP_ERR_INVALID_STATE;
}

#endif /* BLE_AUDIO_TMAP_SUPPORTED || BLE_AUDIO_GMAP_SUPPORTED */

/* ── TMAP ───────────────────────────────────────────────────────────────── */

#if BLE_AUDIO_TMAP_SUPPORTED

/** Peer TMAS read finished. */
static void tmap_discovered(esp_ble_audio_tmap_role_t role, esp_ble_conn_t *conn, int err) {
  const ble_audio_tmap_discovered_t out = {.roles = err ? 0 : (uint16_t)role};
  bleAudioEngineEmit(BLE_AUDIO_EVT_TMAP_DISCOVERED, conn ? conn->handle : CONN_NONE, err, &out);
}

static const esp_ble_audio_tmap_cb_t s_tmap_cb = {.discovery_complete = tmap_discovered};

int bleAudioTmapRegister(uint16_t roles) {
  int err = attach();
  if (err != 0) {
    return err;
  }
  if (roles == 0) {
    return ESP_ERR_INVALID_ARG;
  }
  err = esp_ble_audio_tmap_register((esp_ble_audio_tmap_role_t)roles);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "TMAP: registering roles 0x%02x failed: %d", roles, err);
  }
  return err;
}

int bleAudioTmapDiscover(uint16_t conn_handle) {
  int err = attach();
  return err ? err : (int)esp_ble_audio_tmap_discover(conn_handle, &s_tmap_cb);
}

#else

int bleAudioTmapRegister(uint16_t roles) {
  (void)roles;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioTmapDiscover(uint16_t conn_handle) {
  (void)conn_handle;
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* BLE_AUDIO_TMAP_SUPPORTED */

/* ── GMAP ───────────────────────────────────────────────────────────────── */

#if BLE_AUDIO_GMAP_SUPPORTED

/** Peer GMAS read finished; features are only meaningful for the roles set. */
static void gmap_discovered(esp_ble_conn_t *conn, int err, esp_ble_audio_gmap_role_t role, esp_ble_audio_gmap_feat_t features) {
  ble_audio_gmap_discovered_t out = {0};
  if (err == 0) {
    out.roles = (uint8_t)role;
    out.features.ugg = (uint8_t)features.ugg_feat;
    out.features.ugt = (uint8_t)features.ugt_feat;
    out.features.bgs = (uint8_t)features.bgs_feat;
    out.features.bgr = (uint8_t)features.bgr_feat;
  }
  bleAudioEngineEmit(BLE_AUDIO_EVT_GMAP_DISCOVERED, conn ? conn->handle : CONN_NONE, err, &out);
}

static const esp_ble_audio_gmap_cb_t s_gmap_cb = {.discover = gmap_discovered};

int bleAudioGmapRegister(uint8_t roles, const ble_audio_gmap_features_t *features) {
  int err = attach();
  if (err != 0) {
    return err;
  }
  esp_ble_audio_gmap_feat_t feat = {0};
  if (features) {
    feat.ugg_feat = (esp_ble_audio_gmap_ugg_feat_t)features->ugg;
    feat.ugt_feat = (esp_ble_audio_gmap_ugt_feat_t)features->ugt;
    feat.bgs_feat = (esp_ble_audio_gmap_bgs_feat_t)features->bgs;
    feat.bgr_feat = (esp_ble_audio_gmap_bgr_feat_t)features->bgr;
  }
  err = esp_ble_audio_gmap_register((esp_ble_audio_gmap_role_t)roles, feat);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "GMAP: registering roles 0x%02x failed: %d (a role disabled in Kconfig, or features the ASE/stream counts cannot back)",
             roles, err);
  }
  return err;
}

int bleAudioGmapDiscover(uint16_t conn_handle) {
  int err = attach();
  if (err != 0) {
    return err;
  }
  /* Registered on first use; after a re-init the stack may still hold the table (s_gmap_cb_linked). */
  if (!s_gmap_cb_registered) {
    err = esp_ble_audio_gmap_cb_register(&s_gmap_cb);
    if (err != ESP_OK && !s_gmap_cb_linked) {
      ESP_LOGE(TAG, "GMAP: callback registration failed: %d", err);
      return err;
    }
    s_gmap_cb_linked = true;
    s_gmap_cb_registered = true;
  }
  return (int)esp_ble_audio_gmap_discover(conn_handle);
}

#else

int bleAudioGmapRegister(uint8_t roles, const ble_audio_gmap_features_t *features) {
  (void)roles;
  (void)features;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioGmapDiscover(uint16_t conn_handle) {
  (void)conn_handle;
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* BLE_AUDIO_GMAP_SUPPORTED */

/* ── PBP ────────────────────────────────────────────────────────────────── */

#if BLE_AUDIO_PBP_SUPPORTED

#define AD_TYPE_SERVICE_DATA16 0x16

_Static_assert(BLE_AUDIO_PBA_HDR_LEN == ESP_BLE_AUDIO_PBP_MIN_PBA_SIZE, "PBA header size mismatch");

int bleAudioPbpBuild(uint8_t features, const uint8_t *meta, size_t meta_len, uint8_t *out, size_t cap, size_t *out_len) {
  if (out == NULL || out_len == NULL || (meta == NULL && meta_len) || meta_len > UINT8_MAX) {
    return ESP_ERR_INVALID_ARG;
  }
  if (cap < BLE_AUDIO_PBA_HDR_LEN + meta_len) {
    return ESP_ERR_NO_MEM;
  }
  /* The stack writes straight into the caller's buffer. */
  struct net_buf_simple buf;
  net_buf_simple_init_with_data(&buf, out, cap);
  net_buf_simple_reset(&buf);
  esp_err_t err = esp_ble_audio_pbp_get_announcement(meta_len ? meta : NULL, meta_len, (esp_ble_audio_pbp_announcement_feature_t)features, &buf);
  if (err != ESP_OK) {
    return (int)err;
  }
  *out_len = buf.len;
  return 0;
}

int bleAudioPbpParse(const uint8_t *data, size_t len, uint8_t *features, const uint8_t **meta, size_t *meta_len) {
  if (data == NULL || len > UINT8_MAX || features == NULL || meta == NULL || meta_len == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  /* The stack parser takes the AD type too; it rejects anything but a PBA service data value. */
  esp_ble_audio_pbp_announcement_feature_t feat = 0;
  uint8_t *m = NULL;
  uint8_t mlen = 0;
  esp_err_t err = esp_ble_audio_pbp_parse_announcement(AD_TYPE_SERVICE_DATA16, data, (uint8_t)len, &feat, &m, &mlen);
  if (err != ESP_OK) {
    return (int)err;
  }
  *features = (uint8_t)feat;
  *meta = mlen ? m : NULL;
  *meta_len = mlen;
  return 0;
}

#else

int bleAudioPbpBuild(uint8_t features, const uint8_t *meta, size_t meta_len, uint8_t *out, size_t cap, size_t *out_len) {
  (void)features;
  (void)meta;
  (void)meta_len;
  (void)out;
  (void)cap;
  (void)out_len;
  return ESP_ERR_NOT_SUPPORTED;
}
int bleAudioPbpParse(const uint8_t *data, size_t len, uint8_t *features, const uint8_t **meta, size_t *meta_len) {
  (void)data;
  (void)len;
  (void)features;
  (void)meta;
  (void)meta_len;
  return ESP_ERR_NOT_SUPPORTED;
}

#endif /* BLE_AUDIO_PBP_SUPPORTED */

#endif /* BLE_AUDIO_SUPPORTED */
