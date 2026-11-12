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
 * @brief Control-profile engine unit: VCP, MICP, MCP, CCP and HAS.
 *
 * Every role has an idempotent init (call between engine init and start).
 * VCP, MICP and HAS clients drive the one peer they last discovered and fail
 * with ESP_ERR_INVALID_STATE for any other link. Results and remote changes
 * come back through the envelope bridge, one group per role. All functions
 * return an esp_err_t value.
 */

#include "audio/BLEAudioEngine.h"
#if BLE_AUDIO_SUPPORTED

#ifdef __cplusplus
extern "C" {
#endif

/* ── Shared payloads ────────────────────────────────────────────────────── */

/** Volume state (VCS), local or remote. */
typedef struct {
  uint8_t volume;  /*!< 0-255. */
  bool mute;
} ble_audio_vcp_state_t;

/** Audio Input Control state of the peer's first AICS instance. */
typedef struct {
  int8_t gain;     /*!< Gain setting, in the instance's gain units. */
  bool mute;
} ble_audio_aics_state_t;

/* ── VCP volume renderer ────────────────────────────────────────────────── */

enum {
  BLE_AUDIO_EVT_VCP_REND_STATE = BLE_AUDIO_EVT(BLE_AUDIO_GRP_VCP_RENDERER, 0), /* ble_audio_vcp_state_t; conn = writer or NONE */
};

/** Operations for bleAudioVcpRendOp() and bleAudioVcpCtlrOp(). */
enum {
  BLE_AUDIO_VCP_OP_SET_VOLUME = 0,
  BLE_AUDIO_VCP_OP_MUTE,
  BLE_AUDIO_VCP_OP_UNMUTE,
  BLE_AUDIO_VCP_OP_UP,
  BLE_AUDIO_VCP_OP_DOWN,
  BLE_AUDIO_VCP_OP_SET_STEP, /*!< Renderer only. */
  BLE_AUDIO_VCP_OP_READ,     /*!< Controller only: re-read the volume state. */
};

/** @brief Register VCS with one VOCS/AICS per Kconfig instance. @p step must be > 0. */
int bleAudioVcpRendInit(uint8_t volume, bool mute, uint8_t step);
/** @param value Volume for SET_VOLUME, step for SET_STEP, ignored otherwise. */
int bleAudioVcpRendOp(uint8_t op, uint8_t value);

/* ── VCP volume controller ──────────────────────────────────────────────── */

enum {
  BLE_AUDIO_EVT_VCP_CTLR_DISCOVERED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_VCP_CONTROLLER, 0), /* ble_audio_vcp_discovered_t */
  BLE_AUDIO_EVT_VCP_CTLR_STATE = BLE_AUDIO_EVT(BLE_AUDIO_GRP_VCP_CONTROLLER, 1),      /* ble_audio_vcp_state_t */
  BLE_AUDIO_EVT_VCP_CTLR_OFFSET = BLE_AUDIO_EVT(BLE_AUDIO_GRP_VCP_CONTROLLER, 2),     /* int16_t volume offset */
  BLE_AUDIO_EVT_VCP_CTLR_INPUT = BLE_AUDIO_EVT(BLE_AUDIO_GRP_VCP_CONTROLLER, 3),      /* ble_audio_aics_state_t */
};

/** Payload of BLE_AUDIO_EVT_VCP_CTLR_DISCOVERED. */
typedef struct {
  uint8_t vocs_count;  /*!< Volume Offset Control instances on the peer. */
  uint8_t aics_count;  /*!< Audio Input Control instances on the peer. */
} ble_audio_vcp_discovered_t;

/** @brief Register the volume controller callbacks. */
int bleAudioVcpCtlrInit(void);
/** @brief DISCOVERED follows the first volume-state read, then STATE. */
int bleAudioVcpCtlrDiscover(uint16_t conn_handle);
/**
 * @brief Run a BLE_AUDIO_VCP_OP_* (not SET_STEP) on the discovered renderer.
 * @param volume Used by SET_VOLUME only.
 */
int bleAudioVcpCtlrOp(uint16_t conn_handle, uint8_t op, uint8_t volume);
/** @brief Write the offset of the peer's first VOCS instance. */
int bleAudioVcpCtlrSetOffset(uint16_t conn_handle, int16_t offset);
/** @brief Write the gain of the peer's first AICS instance. */
int bleAudioVcpCtlrSetGain(uint16_t conn_handle, int8_t gain);

/* ── MICP microphone device ─────────────────────────────────────────────── */

/** Mute states, same values as `ESP_BLE_AUDIO_MICP_MUTE_*`. */
#define BLE_AUDIO_MICP_UNMUTED  0
#define BLE_AUDIO_MICP_MUTED    1
#define BLE_AUDIO_MICP_DISABLED 2

enum {
  BLE_AUDIO_EVT_MICP_DEV_MUTE = BLE_AUDIO_EVT(BLE_AUDIO_GRP_MICP_DEVICE, 0), /* uint8_t BLE_AUDIO_MICP_* */
};

/** @brief Register MICS with one microphone AICS per Kconfig instance. */
int bleAudioMicpDevInit(bool mute);
/** @brief Set the local mute state (BLE_AUDIO_MICP_*); DISABLED prevents clients from unmuting. */
int bleAudioMicpDevSetMute(uint8_t state);

/* ── MICP microphone controller ─────────────────────────────────────────── */

enum {
  BLE_AUDIO_EVT_MICP_CTLR_DISCOVERED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_MICP_CONTROLLER, 0), /* uint8_t aics_count */
  BLE_AUDIO_EVT_MICP_CTLR_MUTE = BLE_AUDIO_EVT(BLE_AUDIO_GRP_MICP_CONTROLLER, 1),       /* uint8_t BLE_AUDIO_MICP_* */
  BLE_AUDIO_EVT_MICP_CTLR_INPUT = BLE_AUDIO_EVT(BLE_AUDIO_GRP_MICP_CONTROLLER, 2),      /* ble_audio_aics_state_t */
};

/** @brief Register the microphone controller callbacks. */
int bleAudioMicpCtlrInit(void);
/** @brief DISCOVERED is followed by a mute read. */
int bleAudioMicpCtlrDiscover(uint16_t conn_handle);
/** @brief Write the peer's mute state; the result arrives as MICP_CTLR_MUTE. */
int bleAudioMicpCtlrSetMute(uint16_t conn_handle, bool mute);
/** @brief Re-read the peer's mute state (MICP_CTLR_MUTE). */
int bleAudioMicpCtlrReadMute(uint16_t conn_handle);
/** @brief Write the gain of the peer's first microphone AICS instance. */
int bleAudioMicpCtlrSetGain(uint16_t conn_handle, int8_t gain);

/* ── MCP media player (local player behind MCS/GMCS) ────────────────────── */

/** Media states, same values as `ESP_BLE_AUDIO_MEDIA_PROXY_STATE_*`. */
enum { BLE_AUDIO_MCP_INACTIVE = 0, BLE_AUDIO_MCP_PLAYING, BLE_AUDIO_MCP_PAUSED, BLE_AUDIO_MCP_SEEKING };

/** Command result code meaning success, same as `ESP_BLE_AUDIO_MEDIA_PROXY_CMD_SUCCESS`. */
#define BLE_AUDIO_MCP_CMD_SUCCESS 1

enum {
  BLE_AUDIO_EVT_MCP_SRV_STATE = BLE_AUDIO_EVT(BLE_AUDIO_GRP_MCP_SERVER, 0),  /* uint8_t BLE_AUDIO_MCP_* */
  BLE_AUDIO_EVT_MCP_SRV_RESULT = BLE_AUDIO_EVT(BLE_AUDIO_GRP_MCP_SERVER, 1), /* ble_audio_mcp_result_t */
};

/** Outcome of one media control point command. */
typedef struct {
  uint8_t opcode; /*!< MCS control point opcode. */
  uint8_t result; /*!< BLE_AUDIO_MCP_CMD_SUCCESS or an MCS result code; 0 when the write failed. */
} ble_audio_mcp_result_t;

/**
 * @brief Initialise the local media player. The player's state and every
 *        command result (local or remote) are reported only when
 *        CONFIG_BT_MCTL_LOCAL_PLAYER_LOCAL_CONTROL is enabled.
 */
int bleAudioMcpSrvInit(void);
/** @param title true = track title, false = player name. */
int bleAudioMcpSrvSetText(bool title, const char *text);
/** @brief Send @p opcode to the local player (needs local control). */
int bleAudioMcpSrvCommand(uint8_t opcode);

/* ── MCP media controller ───────────────────────────────────────────────── */

enum {
  BLE_AUDIO_EVT_MCP_CLI_DISCOVERED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_MCP_CLIENT, 0),
  BLE_AUDIO_EVT_MCP_CLI_STATE = BLE_AUDIO_EVT(BLE_AUDIO_GRP_MCP_CLIENT, 1),       /* uint8_t BLE_AUDIO_MCP_* */
  BLE_AUDIO_EVT_MCP_CLI_RESULT = BLE_AUDIO_EVT(BLE_AUDIO_GRP_MCP_CLIENT, 2),      /* ble_audio_mcp_result_t */
  BLE_AUDIO_EVT_MCP_CLI_PLAYER_NAME = BLE_AUDIO_EVT(BLE_AUDIO_GRP_MCP_CLIENT, 3), /* const char * */
  BLE_AUDIO_EVT_MCP_CLI_TRACK_TITLE = BLE_AUDIO_EVT(BLE_AUDIO_GRP_MCP_CLIENT, 4), /* const char * */
};

/** Values for bleAudioMcpCliRead(). */
enum { BLE_AUDIO_MCP_READ_STATE = 0, BLE_AUDIO_MCP_READ_PLAYER_NAME, BLE_AUDIO_MCP_READ_TRACK_TITLE };

/** @brief Register the media controller callbacks. */
int bleAudioMcpCliInit(void);
/** @brief Discover MCS/GMCS and subscribe; DISCOVERED is followed by a state read. */
int bleAudioMcpCliDiscover(uint16_t conn_handle);
/** @brief Send an MCS control point @p opcode (no parameter); the outcome arrives as MCP_CLI_RESULT. */
int bleAudioMcpCliCommand(uint16_t conn_handle, uint8_t opcode);
/** @brief Read one characteristic (BLE_AUDIO_MCP_READ_*); the value arrives as the matching event. */
int bleAudioMcpCliRead(uint16_t conn_handle, uint8_t what);

/* ── CCP call control server (GTBS) ─────────────────────────────────────── */

/** Call operations, shared by both CCP roles. */
enum {
  BLE_AUDIO_CCP_OP_ORIGINATE = 0,
  BLE_AUDIO_CCP_OP_ACCEPT,
  BLE_AUDIO_CCP_OP_TERMINATE,
  BLE_AUDIO_CCP_OP_HOLD,
  BLE_AUDIO_CCP_OP_RETRIEVE,
  BLE_AUDIO_CCP_OP_REMOTE_ANSWER,    /*!< Server only: the far end answered. */
  BLE_AUDIO_CCP_OP_REMOTE_TERMINATE, /*!< Server only: the far end hung up. */
};

enum {
  BLE_AUDIO_EVT_CCP_SRV_ORIGINATE = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CCP_SERVER, 0), /* ble_audio_ccp_originate_t */
  BLE_AUDIO_EVT_CCP_SRV_CALL = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CCP_SERVER, 1),      /* ble_audio_ccp_call_t: a client changed a call */
};

/** A client asked to place an outgoing call (dispatched synchronously so the handler can reject). */
typedef struct {
  uint8_t call_index;  /*!< Index the stack assigned to the new call. */
  const char *uri;     /*!< Target URI; valid during the handler only. */
  bool *accept; /*!< Handler writes false to reject; defaults to true. */
} ble_audio_ccp_originate_t;

/** A call operation, from a client (server side) or its result (client side). */
typedef struct {
  uint8_t op; /*!< BLE_AUDIO_CCP_OP_*. */
  uint8_t call_index;
  uint8_t reason; /*!< Termination reason for TERMINATE. */
} ble_audio_ccp_call_t;

/** @brief Register the GTBS bearer; the strings are copied. */
int bleAudioCcpSrvInit(const char *provider_name, const char *uci, const char *uri_schemes);
/** @brief Raise an incoming call from @p from (a URI such as "tel:+1234"). */
int bleAudioCcpSrvIncoming(const char *from, const char *friendly_name, uint8_t *call_index);
/** @brief Apply a BLE_AUDIO_CCP_OP_* (except ORIGINATE) to a local call. */
int bleAudioCcpSrvCallOp(uint8_t op, uint8_t call_index);
/** @brief Update the bearer provider name shown to clients. */
int bleAudioCcpSrvSetProviderName(const char *name);

/* ── CCP call control client ────────────────────────────────────────────── */

/** Pseudo call state reported once a call is gone. */
#define BLE_AUDIO_CCP_CALL_ENDED 0xFF

enum {
  BLE_AUDIO_EVT_CCP_CLI_DISCOVERED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CCP_CLIENT, 0), /* ble_audio_ccp_discovered_t */
  BLE_AUDIO_EVT_CCP_CLI_RESULT = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CCP_CLIENT, 1),     /* ble_audio_ccp_call_t (reason unused) */
  BLE_AUDIO_EVT_CCP_CLI_CALL = BLE_AUDIO_EVT(BLE_AUDIO_GRP_CCP_CLIENT, 2),       /* ble_audio_ccp_call_state_t, one per call */
};

/** Payload of BLE_AUDIO_EVT_CCP_CLI_DISCOVERED. */
typedef struct {
  uint8_t tbs_count;  /*!< TBS instances found, as reported by the stack. */
  bool gtbs;          /*!< The peer exposes the Generic TBS (the one the client drives). */
} ble_audio_ccp_discovered_t;

/** State of one call on the peer. */
typedef struct {
  uint8_t call_index;
  uint8_t state; /*!< `ESP_BLE_AUDIO_TBS_CALL_STATE_*` or BLE_AUDIO_CCP_CALL_ENDED. */
} ble_audio_ccp_call_state_t;

/** @brief Register the call control client callbacks. */
int bleAudioCcpCliInit(void);
/** @brief DISCOVERED is followed by a GTBS call-state read. */
int bleAudioCcpCliDiscover(uint16_t conn_handle);
/** @brief Ask the peer to place a call to @p uri; the result arrives as CCP_CLI_RESULT. */
int bleAudioCcpCliOriginate(uint16_t conn_handle, const char *uri);
/** @param op ACCEPT, TERMINATE, HOLD or RETRIEVE. */
int bleAudioCcpCliCallOp(uint16_t conn_handle, uint8_t op, uint8_t call_index);
/** @brief Re-read the peer's call states (one CCP_CLI_CALL per call). */
int bleAudioCcpCliReadCalls(uint16_t conn_handle);

/* ── HAS hearing aid ────────────────────────────────────────────────────── */

enum {
  BLE_AUDIO_EVT_HAS_SRV_SELECT = BLE_AUDIO_EVT(BLE_AUDIO_GRP_HAS_SERVER, 0), /* ble_audio_has_select_t; the preset is activated */
  BLE_AUDIO_EVT_HAS_SRV_NAME = BLE_AUDIO_EVT(BLE_AUDIO_GRP_HAS_SERVER, 1),   /* ble_audio_has_preset_t (name, index) */
};

/** A client selected a preset. */
typedef struct {
  uint8_t index;
  bool sync;  /*!< The client asked to apply it to the whole binaural set. */
} ble_audio_has_select_t;

/** One preset record (server name change or client read/notification). */
typedef struct {
  uint8_t index;
  bool available;
  bool writable;   /*!< The name may be changed remotely. */
  bool is_last;    /*!< Last record of a read. */
  const char *name;  /*!< Valid during the handler only. */
} ble_audio_has_preset_t;

/**
 * @param type        `ESP_BLE_AUDIO_HAS_HEARING_AID_TYPE_*` value.
 * @param preset_sync The device supports synchronized preset changes (binaural sets).
 */
int bleAudioHasSrvInit(uint8_t type, bool preset_sync);
/** @brief Register a preset (index 1-255, unique); the name is copied. */
int bleAudioHasSrvAddPreset(uint8_t index, const char *name, bool available, bool writable);
int bleAudioHasSrvRemovePreset(uint8_t index);
/** @brief Mark a preset (un)available for selection. */
int bleAudioHasSrvSetAvailable(uint8_t index, bool available);
/** @brief Rename a preset locally (clients are notified). */
int bleAudioHasSrvRename(uint8_t index, const char *name);
/** @brief Activate a preset locally (clients are notified). */
int bleAudioHasSrvSetActive(uint8_t index);
/** @return the active preset index, 0 when none. */
uint8_t bleAudioHasSrvGetActive(void);

/* ── HAS hearing aid controller ─────────────────────────────────────────── */

enum {
  BLE_AUDIO_EVT_HAS_CLI_DISCOVERED = BLE_AUDIO_EVT(BLE_AUDIO_GRP_HAS_CLIENT, 0), /* uint8_t hearing aid type */
  BLE_AUDIO_EVT_HAS_CLI_PRESET = BLE_AUDIO_EVT(BLE_AUDIO_GRP_HAS_CLIENT, 1),     /* ble_audio_has_preset_t (read or changed) */
  BLE_AUDIO_EVT_HAS_CLI_ACTIVE = BLE_AUDIO_EVT(BLE_AUDIO_GRP_HAS_CLIENT, 2),     /* uint8_t active preset index */
};

/** @brief Register the hearing aid controller callbacks. */
int bleAudioHasCliInit(void);
/** @brief Discover HAS on the peer (HAS_CLI_DISCOVERED carries its hearing aid type). */
int bleAudioHasCliDiscover(uint16_t conn_handle);
/** @brief Read up to @p max_count presets from @p start_index (one HAS_CLI_PRESET each). */
int bleAudioHasCliReadPresets(uint16_t conn_handle, uint8_t start_index, uint8_t max_count);
/** @param sync Ask the peer to apply the change to its whole binaural set. */
int bleAudioHasCliSetActive(uint16_t conn_handle, uint8_t index, bool sync);
/** @brief Select the next (@p next) or previous available preset. */
int bleAudioHasCliStep(uint16_t conn_handle, bool next, bool sync);
/** @brief Rename a writable preset on the peer. */
int bleAudioHasCliRename(uint16_t conn_handle, uint8_t index, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* BLE_AUDIO_SUPPORTED */
