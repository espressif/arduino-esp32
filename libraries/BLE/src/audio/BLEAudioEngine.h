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
 * @brief C boundary of the ESP-BLE-AUDIO engine: lifecycle, event bridge, presets.
 *
 * The `esp_ble_audio_*` headers do not parse as C++: every one of them pulls
 * in the IDF's `esp_ble_audio/host/lib/include/audio.h` (via `common/init.h`),
 * which forward-declares enums without an underlying type (a GNU C extension)
 * and uses two of them (`bt_cap_common_proc_type`, `bt_cap_common_subproc_type`)
 * that no shipped header defines. So all engine calls live in C units that
 * share this header:
 *
 *  - `BLEAudioEngine.c`           lifecycle, GAP/GATT plumbing, links, presets
 *  - `BLEAudioEngineBap.c`        stream pool, PACS, unicast server/client,
 *                                 broadcast source/sink, scan delegator
 *  - `BLEAudioEngineAssistant.c`  BAP broadcast assistant (BASS client)
 *  - `BLEAudioEngineCap.c`        CAP acceptor/initiator/commander/handover, CSIP
 *  - `BLEAudioEngineControl.c`    VCP, MICP, MCP, CCP, HAS
 *  - `BLEAudioEngineProfiles.c`   TMAP, GMAP, PBP
 *
 * Host-specific glue that must call NimBLE or Bluedroid headers lives in
 * `BLEAudioEngine.nimble.cpp` / `BLEAudioEngine.bluedroid.cpp`.
 *
 * Control-plane events reach C++ through one sink: every unit fills a
 * `ble_audio_evt_t` whose `data` points at a payload struct declared in that
 * unit's header, and `BLEAudio::Impl` routes it by group to the role that
 * registered for it. Stream data (the hot path) bypasses the bridge; see
 * `BLEAudioEngineBap.h`.
 *
 * Threading: every engine callback, and so every emitted event, runs on the
 * Bluetooth host task. Payloads are only valid during the sink call.
 */

#include "core/BLEGuards.h"
#if BLE_AUDIO_SUPPORTED

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/** No ACL connection (broadcast streams, unbound slots, events without a link). */
#define BLE_AUDIO_CONN_NONE 0xFFFF

/* ── Event bridge ───────────────────────────────────────────────────────── */

/**
 * One group per role, so each role handle registers exactly one handler.
 * CORE events are delivered to every registered handler.
 */
typedef enum {
  BLE_AUDIO_GRP_CORE = 0,
  BLE_AUDIO_GRP_UNICAST_CLIENT,
  BLE_AUDIO_GRP_BROADCAST_SOURCE,
  BLE_AUDIO_GRP_BROADCAST_SINK,
  BLE_AUDIO_GRP_BROADCAST_ASSISTANT,
  BLE_AUDIO_GRP_CAP_ACCEPTOR,
  BLE_AUDIO_GRP_CAP_INITIATOR,
  BLE_AUDIO_GRP_CAP_COMMANDER,
  BLE_AUDIO_GRP_CAP_HANDOVER,
  BLE_AUDIO_GRP_CSIP_MEMBER,
  BLE_AUDIO_GRP_CSIP_COORDINATOR,
  BLE_AUDIO_GRP_VCP_RENDERER,
  BLE_AUDIO_GRP_VCP_CONTROLLER,
  BLE_AUDIO_GRP_MICP_DEVICE,
  BLE_AUDIO_GRP_MICP_CONTROLLER,
  BLE_AUDIO_GRP_MCP_SERVER,
  BLE_AUDIO_GRP_MCP_CLIENT,
  BLE_AUDIO_GRP_CCP_SERVER,
  BLE_AUDIO_GRP_CCP_CLIENT,
  BLE_AUDIO_GRP_HAS_SERVER,
  BLE_AUDIO_GRP_HAS_CLIENT,
  BLE_AUDIO_GRP_TMAP,
  BLE_AUDIO_GRP_GMAP,
  BLE_AUDIO_GRP_COUNT
} ble_audio_evt_group_t;

/** Event type: group in the high byte, event number within the group in the low byte. */
#define BLE_AUDIO_EVT(grp, n)      ((uint16_t)(((grp) << 8) | (n)))
/** Group of an event type (index into the handler table). */
#define BLE_AUDIO_EVT_GROUP(type)  ((uint8_t)((type) >> 8))

/** Core events (no payload; `err` carries the HCI/engine status). */
enum {
  BLE_AUDIO_EVT_ACL_CONNECTED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CORE, 0),
  BLE_AUDIO_EVT_ACL_DISCONNECTED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CORE, 1),  /*!< err = HCI reason. */
  BLE_AUDIO_EVT_SECURITY_CHANGED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CORE, 2),  /*!< err = 0 when encrypted. */
  /** GATT client discovery of the peer finished; profile discovery may start. */
  BLE_AUDIO_EVT_GATT_DISCOVERED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CORE, 3),
};

typedef struct {
  uint16_t type;        /*!< BLE_AUDIO_EVT(group, n). */
  uint16_t conn_handle; /*!< ACL the event belongs to, or BLE_AUDIO_CONN_NONE. */
  int err;              /*!< 0 on success, engine/ATT error otherwise. */
  const void *data;     /*!< Group payload declared in the unit header, or NULL. */
} ble_audio_evt_t;

/** @brief The single C++ event sink (`BLEAudio::Impl::dispatch`); @p ctx is the value given to init. */
typedef void (*ble_audio_evt_sink_fn)(const ble_audio_evt_t *evt, void *ctx);

/**
 * @brief Deliver an event to C++ (engine units only; host task context).
 *
 * Synchronous: @p data only needs to live for the duration of the call, so
 * units pass stack payloads. Dropped silently before init / after deinit.
 */
void bleAudioEngineEmit(uint16_t type, uint16_t conn_handle, int err, const void *data);

/* ── Codec / QoS mirrors of the public value types ──────────────────────── */

/** Mirror of `BLEAudioCodecConfig` (see `BLEAudioTypes.h` for field meanings). */
typedef struct {
  uint32_t sample_rate_hz;
  uint16_t frame_dur_us;     /*!< 7500 or 10000. */
  uint16_t octets_per_frame; /*!< Per channel, per frame. */
  uint8_t frames_per_sdu;    /*!< Codec frame blocks per SDU (0 is read as 1). */
  uint32_t chan_alloc;       /*!< Audio Location bitfield, 0 = mono. */
} ble_audio_codec_t;

/** Mirror of `BLEAudioQos`. */
typedef struct {
  uint32_t sdu_interval_us;
  uint16_t max_sdu;    /*!< Octets, all channels and blocks. */
  uint8_t rtn;         /*!< Retransmission number. */
  uint16_t latency_ms; /*!< Max transport latency. */
  uint32_t pd_us;      /*!< Presentation delay. */
  uint8_t phy;         /*!< BLE_AUDIO_PHY_* bitmask. */
  bool framed;
} ble_audio_qos_t;

#define BLE_AUDIO_PHY_1M    0x01
#define BLE_AUDIO_PHY_2M    0x02
#define BLE_AUDIO_PHY_CODED 0x04

/** Number of BAP LC3 presets; indexes match `BLEAudioCodecPreset`. */
#define BLE_AUDIO_PRESET_COUNT 16

/**
 * @brief Expand a BAP LC3 preset (BAP Tables 5.2 / 6.4, low-latency variants).
 * @param preset    Index in [0, BLE_AUDIO_PRESET_COUNT).
 * @param broadcast Use the broadcast QoS (RTN differs from unicast).
 * @return false if @p preset is out of range.
 */
bool bleAudioPresetGet(uint8_t preset, bool broadcast, ble_audio_codec_t *codec, ble_audio_qos_t *qos);

/** @brief LTV channel count of an Audio Location bitfield (mono = 1). */
uint8_t bleAudioChannelCount(uint32_t chan_alloc);

/* ── Addresses ──────────────────────────────────────────────────────────── */

/**
 * @brief Copy an address from an engine GAP event into LSB-first order.
 *
 * The ISO host adapters copy the host's address bytes verbatim into
 * `esp_ble_audio_gap_app_event_t`, so they are MSB-first on Bluedroid and
 * LSB-first on NimBLE. Addresses inside profile structures (BASS receive
 * states, `esp_ble_conn_t`) are always LSB-first. Every address handed to
 * C++ is LSB-first, the order `BTAddress(const uint8_t *, Type)` expects.
 */
static inline void bleAudioAddrFromGap(uint8_t out[6], const uint8_t in[6]) {
#if BLE_BLUEDROID
  for (int i = 0; i < 6; i++) {
    out[i] = in[5 - i];
  }
#else
  memcpy(out, in, 6);
#endif
}

/* ── Lifecycle ──────────────────────────────────────────────────────────── */

/**
 * @brief `esp_ble_audio_common_init` + event sink install.
 *
 * Refuses (ESP_ERR_INVALID_STATE) while the standalone ISO transport owns the
 * host; on success the ISO transport attaches to the audio-owned host.
 */
int bleAudioEngineInit(ble_audio_evt_sink_fn sink, void *ctx);

/** @brief `esp_ble_audio_common_start` with the registered CSIS instances. */
int bleAudioEngineStart(void);

/**
 * @brief `esp_ble_audio_common_deinit` (also releases ISO and every profile).
 * @return ESP_ERR_INVALID_STATE while a stream is still up; nothing is released.
 */
int bleAudioEngineDeinit(void);

/** @brief Whether bleAudioEngineInit() succeeded and deinit has not run (host forwarders check it). */
bool bleAudioEngineIsInitialized(void);
/** @brief Whether bleAudioEngineStart() succeeded (GATT committed). */
bool bleAudioEngineIsStarted(void);

/** @brief Record a CSIS instance for `common_start` (CAP acceptor / CSIP member). */
int bleAudioEngineAddCsis(void *svc_inst, bool included_by_cas);

/**
 * @brief Hooks an engine unit installs when its first role is initialized.
 *
 * `on_gap` sees every engine GAP event (`esp_ble_audio_gap_app_event_t *`);
 * `on_deinit` resets the unit's statics after `common_deinit` succeeded, so a
 * later re-init starts clean. The table is cleared on deinit.
 */
typedef struct {
  void (*on_gap)(const void *event);
  void (*on_deinit)(void);
} ble_audio_unit_hooks_t;

/** @brief Install @p hooks (static storage; installing the same table twice is a no-op). */
int bleAudioEngineRegisterUnit(const ble_audio_unit_hooks_t *hooks);

/* ── Host glue ──────────────────────────────────────────────────────────── */

/** @brief NimBLE: forward a GATT event (MTU/notify/subscribe). */
void bleAudioEngineGattPostEvent(uint8_t type, void *event);

/** @brief Start client-side profile discovery on a link (run on every MTU update). */
void bleAudioEngineGattcDiscStart(uint16_t conn_handle);

/** @brief Bluedroid: forward `ESP_GAP_BLE_AUTH_CMPL_EVT` (event id + param). */
void bleAudioEngineGapPostEvent(uint16_t type, void *param);

/**
 * @brief Open an audio link as central.
 *
 * The next central ACL is then encrypted and, once secured, discovered (after
 * an MTU exchange on NimBLE). On Bluedroid the connection is opened through
 * the engine's GATTC interface so its GATT events reach the audio profiles.
 *
 * @param addr Bluedroid (big-endian) address.
 * @return ESP_ERR_NOT_SUPPORTED on NimBLE: open the link with the host GAP
 *         (BLEClient); the engine still secures and discovers it.
 */
int bleAudioEngineConnect(uint8_t addr_type, const uint8_t addr[6]);

/** @brief Forget a pending bleAudioEngineConnect() whose host connect failed. */
void bleAudioEngineCancelConnect(void);

#if BLE_NIMBLE
/*
 * NimBLE host calls used by the link bring-up in BLEAudioEngine.c. NimBLE
 * headers clash with the Zephyr headers of the C units, so they are
 * implemented in BLEAudioEngine.nimble.cpp.
 */

/** @brief Start pairing/encryption on a central link (`ble_gap_security_initiate`). */
int bleAudioNimbleSecure(uint16_t conn_handle);
/**
 * @brief Start the ATT MTU exchange (`ble_gattc_exchange_mtu`); discovery runs on the MTU event.
 * @return 0 also when an exchange is already in progress.
 */
int bleAudioNimbleExchangeMtu(uint16_t conn_handle);
#endif

#ifdef __cplusplus
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
