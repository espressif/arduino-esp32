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

#include "soc/soc_caps.h"
#include "sdkconfig.h"

/**
 * @brief Central BLE compilation guards.
 *
 * Include this header instead of manually writing out the long
 * SOC_BLE_SUPPORTED / CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE / CONFIG_NIMBLE_ENABLED
 * preprocessor checks.
 *
 * All feature-availability decisions live here so that the public API layer
 * and backend implementations can stay free of raw Kconfig checks.
 *
 * Stack selection:
 *   BLE_NIMBLE                 – NimBLE backend is active
 *   BLE_BLUEDROID              – Bluedroid backend is active
 *   BLE_ENABLED                – BLE is available AND a stack is enabled
 *
 * Feature guards (derived from Kconfig / SoC caps):
 *   BLE_GATT_SERVER_SUPPORTED  – GATT server (GATTS) is available
 *   BLE_GATT_CLIENT_SUPPORTED  – GATT client (GATTC) is available
 *   BLE_SMP_SUPPORTED          – Security Manager Protocol is available
 *   BLE_SCANNING_SUPPORTED     – BLE scanning (observer / central) is available
 *   BLE_ADVERTISING_SUPPORTED  – BLE advertising (broadcaster / peripheral) is available
 *   BLE5_SUPPORTED             – BLE 5.0 features (ext adv, PHY, periodic adv, …)
 *   BLE_PERIODIC_ADV_SUPPORTED – BLE 5.0 periodic advertising enabled in config
 *   BLE5_ADV_TX_SUPPORTED      – ext-adv TX actually implemented (backend has it)
 *   BLE_PERIODIC_ADV_TX_SUPPORTED – periodic-adv TX actually implemented
 *   BLE_L2CAP_SUPPORTED        – L2CAP CoC channels (NimBLE + config)
 *   BLE_ISO_SUPPORTED          – Isochronous transport (CIS/BIG) host support
 *   BLE_ISO_*_SUPPORTED        – per-role ISO guards (CIS central/peripheral, BIG broadcaster/receiver)
 *   BLE_AUDIO_SUPPORTED        – LE Audio engine (GAF profiles) is compiled in
 *   BLE_AUDIO_*_SUPPORTED      – per-role LE Audio guards (BAP/CAP/CSIP/VCP/…)
 */

/* ── Stack selection ────────────────────────────────────────────────── */

/* NimBLE stack selected on a BLE-capable target (native SoC or ESP-Hosted) */
#if (defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)) && defined(CONFIG_NIMBLE_ENABLED)
#define BLE_NIMBLE 1
#else
#define BLE_NIMBLE 0
#endif

/*
 * Bluedroid stack selected (native SoC only, not available via ESP-Hosted).
 * Bluedroid can run Classic BT without BLE, so we also require
 * CONFIG_BT_BLE_ENABLED to ensure the BLE APIs are actually present.
 */
#if defined(SOC_BLE_SUPPORTED) && defined(CONFIG_BLUEDROID_ENABLED) && defined(CONFIG_BT_BLE_ENABLED)
#define BLE_BLUEDROID 1
#else
#define BLE_BLUEDROID 0
#endif

/* BLE is usable: hardware is present AND a BT stack is configured */
#if BLE_NIMBLE || BLE_BLUEDROID
#define BLE_ENABLED 1
#else
#define BLE_ENABLED 0
#endif

/* ── Feature guards ─────────────────────────────────────────────────── */

/*
 * GATT Server (GATTS).
 * NimBLE:    requires peripheral role + GATT server compiled in.
 * Bluedroid: requires the GATTS module.
 */
#if (BLE_NIMBLE && defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL)) || (BLE_BLUEDROID && defined(CONFIG_BT_GATTS_ENABLE))
#define BLE_GATT_SERVER_SUPPORTED 1
#else
#define BLE_GATT_SERVER_SUPPORTED 0
#endif

/*
 * GATT Client (GATTC).
 * NimBLE:    requires central role + GATT client compiled in.
 * Bluedroid: requires the GATTC module.
 */
#if (BLE_NIMBLE && defined(CONFIG_BT_NIMBLE_ROLE_CENTRAL)) || (BLE_BLUEDROID && defined(CONFIG_BT_GATTC_ENABLE))
#define BLE_GATT_CLIENT_SUPPORTED 1
#else
#define BLE_GATT_CLIENT_SUPPORTED 0
#endif

/*
 * Security Manager Protocol (SMP / pairing / bonding).
 * NimBLE:    requires CONFIG_BT_NIMBLE_SECURITY_ENABLE.
 * Bluedroid: requires CONFIG_BT_BLE_SMP_ENABLE.
 */
#if (BLE_NIMBLE && defined(CONFIG_BT_NIMBLE_SECURITY_ENABLE)) || (BLE_BLUEDROID && defined(CONFIG_BT_BLE_SMP_ENABLE))
#define BLE_SMP_SUPPORTED 1
#else
#define BLE_SMP_SUPPORTED 0
#endif

/*
 * Scanning (observer / central).
 * NimBLE:    requires observer or central role.
 * Bluedroid: always available when BLE is enabled.
 */
#if (BLE_NIMBLE && (defined(CONFIG_BT_NIMBLE_ROLE_OBSERVER) || defined(CONFIG_BT_NIMBLE_ROLE_CENTRAL))) || BLE_BLUEDROID
#define BLE_SCANNING_SUPPORTED 1
#else
#define BLE_SCANNING_SUPPORTED 0
#endif

/*
 * Advertising (broadcaster / peripheral).
 * NimBLE:    requires broadcaster or peripheral role.
 * Bluedroid: always available when BLE is enabled.
 */
#if (BLE_NIMBLE && (defined(CONFIG_BT_NIMBLE_ROLE_BROADCASTER) || defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL))) || BLE_BLUEDROID
#define BLE_ADVERTISING_SUPPORTED 1
#else
#define BLE_ADVERTISING_SUPPORTED 0
#endif

/* BLE 5.0 features (extended advertising, 2M/Coded PHY, periodic adv, …) */
#if (BLE_NIMBLE && defined(CONFIG_BT_NIMBLE_EXT_ADV)) || (BLE_BLUEDROID && defined(SOC_BLE_50_SUPPORTED) && defined(CONFIG_BT_BLE_50_FEATURES_SUPPORTED))
#define BLE5_SUPPORTED 1
#else
#define BLE5_SUPPORTED 0
#endif

/*
 * BLE 5.0 periodic advertising is enabled in the stack/config.
 * NimBLE: CONFIG_BT_NIMBLE_ENABLE_PERIODIC_ADV (depends on EXT_ADV).
 * Bluedroid: CONFIG_BT_BLE_50_PERIODIC_ADV_EN (depends on BLE50 + EXTEND_ADV).
 * This is the stack-capability flag; the "is a TX impl actually compiled" flag
 * is BLE_PERIODIC_ADV_TX_SUPPORTED below.
 */
#if (BLE_NIMBLE && defined(CONFIG_BT_NIMBLE_ENABLE_PERIODIC_ADV)) || (BLE_BLUEDROID && defined(CONFIG_BT_BLE_50_PERIODIC_ADV_EN))
#define BLE_PERIODIC_ADV_SUPPORTED 1
#else
#define BLE_PERIODIC_ADV_SUPPORTED 0
#endif

/* Periodic Advertising Sync Transfer (PAST) receiver support. */
#if BLE_PERIODIC_ADV_SUPPORTED && ((BLE_NIMBLE && defined(CONFIG_BT_NIMBLE_PERIODIC_ADV_SYNC_TRANSFER)) || (BLE_BLUEDROID && defined(CONFIG_BT_BLE_FEAT_PERIODIC_ADV_SYNC_TRANSFER)))
#define BLE_PAST_SUPPORTED 1
#else
#define BLE_PAST_SUPPORTED 0
#endif

/*
 * BLE 5.0 extended advertising *TX* is actually implemented in this build.
 * Distinct from BLE5_SUPPORTED (controller/stack capability): both backends
 * implement the ext-adv transmit path (NimBLE: advertising/BLEAdvertising.nimble.cpp;
 * Bluedroid: advertising/BLEAdvertising.bluedroid.cpp), but only when the
 * advertising role is compiled in AND the controller/stack exposes BLE5. A
 * central-only NimBLE build, or a Bluedroid build without BLE5 silicon/config
 * (the case for today's esp32 Bluedroid libs), therefore reports 0 here and
 * falls back to the single shared NotSupported definition in BLEAdvertising.cpp.
 * These flags must match exactly the condition under which a backend compiles
 * its real definitions, so that every build has exactly one definition of each
 * method.
 */
#if (BLE_NIMBLE || BLE_BLUEDROID) && BLE_ADVERTISING_SUPPORTED && BLE5_SUPPORTED
#define BLE5_ADV_TX_SUPPORTED 1
#else
#define BLE5_ADV_TX_SUPPORTED 0
#endif

#if BLE5_ADV_TX_SUPPORTED && BLE_PERIODIC_ADV_SUPPORTED
#define BLE_PERIODIC_ADV_TX_SUPPORTED 1
#else
#define BLE_PERIODIC_ADV_TX_SUPPORTED 0
#endif

/* L2CAP Connection-Oriented Channels (CoC) — NimBLE only, requires config */
#if BLE_NIMBLE && defined(CONFIG_BT_NIMBLE_L2CAP_COC_MAX_NUM) && (CONFIG_BT_NIMBLE_L2CAP_COC_MAX_NUM > 0)
#define BLE_L2CAP_SUPPORTED 1
#else
#define BLE_L2CAP_SUPPORTED 0
#endif

/*
 * Validation-suite injectors. Off in production. The suite passes
 * -DBLE_VALIDATION_HOOKS via tests/validation/ble/{client,server}/build_opt.h.
 */
#if defined(BLE_VALIDATION_HOOKS)
#undef BLE_VALIDATION_HOOKS
#define BLE_VALIDATION_HOOKS 1
#else
#define BLE_VALIDATION_HOOKS 0
#endif

/* ── LE Audio / Isochronous feature guards ──────────────────────────── */

/*
 * The LE Audio and Isochronous features are provided by the host-agnostic
 * esp_ble_iso / esp_ble_audio components (IDF abstracts NimBLE vs Bluedroid
 * *inside* the engine). These guards therefore key on the engine's own
 * Kconfig symbols rather than on stack-specific ones. Both are hidden symbols
 * selected by the concrete role options below (BAP/CAP/…), so they are only
 * defined when at least one audio/ISO role is compiled in.
 *
 * BLE_ISO_SUPPORTED   – host ISO transport (CIS/BIG) is compiled in
 * BLE_AUDIO_SUPPORTED  – the LE Audio engine (GAF profiles) is compiled in
 */

/* Isochronous channels: host support (CONFIG_BT_ISO) on a BLE-capable build. */
#if BLE_ENABLED && defined(CONFIG_BT_ISO)
#define BLE_ISO_SUPPORTED 1
#else
#define BLE_ISO_SUPPORTED 0
#endif

/*
 * Per-role ISO guards. Each maps 1:1 to the esp_ble_iso role Kconfig, which
 * decides whether the matching esp_ble_iso_* entry points are compiled at all
 * (CIG create/CIS connect, ISO server, BIG create, BIG sync). The LE Audio
 * roles select them, so an audio build only carries the ISO roles it needs.
 */
#if BLE_ISO_SUPPORTED && defined(CONFIG_BT_ISO_CENTRAL)
#define BLE_ISO_CIS_CENTRAL_SUPPORTED 1
#else
#define BLE_ISO_CIS_CENTRAL_SUPPORTED 0
#endif

#if BLE_ISO_SUPPORTED && defined(CONFIG_BT_ISO_PERIPHERAL)
#define BLE_ISO_CIS_PERIPHERAL_SUPPORTED 1
#else
#define BLE_ISO_CIS_PERIPHERAL_SUPPORTED 0
#endif

#if BLE_ISO_SUPPORTED && defined(CONFIG_BT_ISO_BROADCASTER)
#define BLE_ISO_BROADCASTER_SUPPORTED 1
#else
#define BLE_ISO_BROADCASTER_SUPPORTED 0
#endif

#if BLE_ISO_SUPPORTED && defined(CONFIG_BT_ISO_SYNC_RECEIVER)
#define BLE_ISO_SYNC_RECEIVER_SUPPORTED 1
#else
#define BLE_ISO_SYNC_RECEIVER_SUPPORTED 0
#endif

/* LE Audio engine (GAF): PACS/ASCS/BAP/CAP/… present. Always implies ISO. */
#if BLE_ENABLED && defined(CONFIG_BT_AUDIO) && BLE_ISO_SUPPORTED
#define BLE_AUDIO_SUPPORTED 1
#else
#define BLE_AUDIO_SUPPORTED 0
#endif

/*
 * Per-role LE Audio guards. Each maps 1:1 to the engine role Kconfig so a
 * role handle / factory / example self-excludes when its role is not built.
 * They are only ever 1 when BLE_AUDIO_SUPPORTED is 1. Written as explicit
 * conditional blocks (not `defined()` inside a macro body, which is UB when
 * the macro is later used in an `#if`).
 */

/* Basic Audio Profile (BAP) */
#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_BAP_UNICAST_SERVER)
#define BLE_AUDIO_UNICAST_SERVER_SUPPORTED 1
#else
#define BLE_AUDIO_UNICAST_SERVER_SUPPORTED 0
#endif

#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_BAP_UNICAST_CLIENT)
#define BLE_AUDIO_UNICAST_CLIENT_SUPPORTED 1
#else
#define BLE_AUDIO_UNICAST_CLIENT_SUPPORTED 0
#endif

#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_BAP_BROADCAST_SOURCE)
#define BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED 1
#else
#define BLE_AUDIO_BROADCAST_SOURCE_SUPPORTED 0
#endif

#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_BAP_BROADCAST_SINK)
#define BLE_AUDIO_BROADCAST_SINK_SUPPORTED 1
#else
#define BLE_AUDIO_BROADCAST_SINK_SUPPORTED 0
#endif

#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_BAP_SCAN_DELEGATOR)
#define BLE_AUDIO_SCAN_DELEGATOR_SUPPORTED 1
#else
#define BLE_AUDIO_SCAN_DELEGATOR_SUPPORTED 0
#endif

#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_BAP_BROADCAST_ASSISTANT)
#define BLE_AUDIO_BROADCAST_ASSISTANT_SUPPORTED 1
#else
#define BLE_AUDIO_BROADCAST_ASSISTANT_SUPPORTED 0
#endif

/* Common Audio Profile (CAP) */
// The CAP acceptor registration entry point (esp_ble_audio_cap_acceptor_register,
// which instantiates CAS + an included CSIS) is compiled only when the acceptor
// is also a coordinated-set member, i.e. CONFIG_BT_CAP_ACCEPTOR_SET_MEMBER.
// Guard on that symbol so the acceptor role only exists when it can link.
#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_CAP_ACCEPTOR_SET_MEMBER)
#define BLE_AUDIO_CAP_ACCEPTOR_SUPPORTED 1
#else
#define BLE_AUDIO_CAP_ACCEPTOR_SUPPORTED 0
#endif

#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_CAP_INITIATOR)
#define BLE_AUDIO_CAP_INITIATOR_SUPPORTED 1
#else
#define BLE_AUDIO_CAP_INITIATOR_SUPPORTED 0
#endif

#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_CAP_COMMANDER)
#define BLE_AUDIO_CAP_COMMANDER_SUPPORTED 1
#else
#define BLE_AUDIO_CAP_COMMANDER_SUPPORTED 0
#endif

// Unicast <-> broadcast handover. Kconfig makes it depend on the commander,
// initiator, broadcast assistant, broadcast source and a unicast client sink ASE.
#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_CAP_HANDOVER)
#define BLE_AUDIO_CAP_HANDOVER_SUPPORTED 1
#else
#define BLE_AUDIO_CAP_HANDOVER_SUPPORTED 0
#endif

/* Coordinated Set Identification Profile (CSIP) */
#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_CSIP_SET_MEMBER)
#define BLE_AUDIO_CSIP_MEMBER_SUPPORTED 1
#else
#define BLE_AUDIO_CSIP_MEMBER_SUPPORTED 0
#endif

#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_CSIP_SET_COORDINATOR)
#define BLE_AUDIO_CSIP_COORDINATOR_SUPPORTED 1
#else
#define BLE_AUDIO_CSIP_COORDINATOR_SUPPORTED 0
#endif

/* Volume Control Profile (VCP) */
#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_VCP_VOL_REND)
#define BLE_AUDIO_VCP_RENDERER_SUPPORTED 1
#else
#define BLE_AUDIO_VCP_RENDERER_SUPPORTED 0
#endif

#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_VCP_VOL_CTLR)
#define BLE_AUDIO_VCP_CONTROLLER_SUPPORTED 1
#else
#define BLE_AUDIO_VCP_CONTROLLER_SUPPORTED 0
#endif

/* Microphone Control Profile (MICP) */
#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_MICP_MIC_DEV)
#define BLE_AUDIO_MICP_DEVICE_SUPPORTED 1
#else
#define BLE_AUDIO_MICP_DEVICE_SUPPORTED 0
#endif

#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_MICP_MIC_CTLR)
#define BLE_AUDIO_MICP_CONTROLLER_SUPPORTED 1
#else
#define BLE_AUDIO_MICP_CONTROLLER_SUPPORTED 0
#endif

/* Media Control Profile (MCP) */
#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_MCS)
#define BLE_AUDIO_MCP_SERVER_SUPPORTED 1
#else
#define BLE_AUDIO_MCP_SERVER_SUPPORTED 0
#endif

#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_MCC)
#define BLE_AUDIO_MCP_CLIENT_SUPPORTED 1
#else
#define BLE_AUDIO_MCP_CLIENT_SUPPORTED 0
#endif

/* Call Control Profile (CCP) */
#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_CCP_CALL_CONTROL_SERVER)
#define BLE_AUDIO_CCP_SERVER_SUPPORTED 1
#else
#define BLE_AUDIO_CCP_SERVER_SUPPORTED 0
#endif

#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_CCP_CALL_CONTROL_CLIENT)
#define BLE_AUDIO_CCP_CLIENT_SUPPORTED 1
#else
#define BLE_AUDIO_CCP_CLIENT_SUPPORTED 0
#endif

/* Top-level profiles */
#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_TMAP)
#define BLE_AUDIO_TMAP_SUPPORTED 1
#else
#define BLE_AUDIO_TMAP_SUPPORTED 0
#endif

#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_GMAP)
#define BLE_AUDIO_GMAP_SUPPORTED 1
#else
#define BLE_AUDIO_GMAP_SUPPORTED 0
#endif

#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_PBP)
#define BLE_AUDIO_PBP_SUPPORTED 1
#else
#define BLE_AUDIO_PBP_SUPPORTED 0
#endif

#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_HAS)
#define BLE_AUDIO_HAS_SUPPORTED 1
#else
#define BLE_AUDIO_HAS_SUPPORTED 0
#endif

#if BLE_AUDIO_SUPPORTED && defined(CONFIG_BT_HAS_CLIENT)
#define BLE_AUDIO_HAS_CLIENT_SUPPORTED 1
#else
#define BLE_AUDIO_HAS_CLIENT_SUPPORTED 0
#endif

/*
 * Turnkey LC3 data plane (BLEAudioPlayer / BLEAudioRecorder / BLEAudioPipeline).
 *
 * The LC3 encoder/decoder lives in the managed `espressif/esp_audio_codec`
 * component, which is only pulled into the LE-Audio-capable targets by the
 * lib-builder. Detect it by header presence so the whole audio component still
 * compiles on builds where the codec is absent (the player/recorder headers
 * then compile to nothing). Requires the LE Audio engine (for the
 * BLEAudioStream it binds to).
 */
#if BLE_AUDIO_SUPPORTED && defined(__has_include)
#if __has_include(<esp_lc3_dec.h>) && __has_include(<esp_lc3_enc.h>)
#define BLE_AUDIO_LC3_SUPPORTED 1
#else
#define BLE_AUDIO_LC3_SUPPORTED 0
#endif
#else
#define BLE_AUDIO_LC3_SUPPORTED 0
#endif
