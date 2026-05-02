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
 *   BLE_L2CAP_SUPPORTED        – L2CAP CoC channels (NimBLE + config)
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

/* L2CAP Connection-Oriented Channels (CoC) — NimBLE only, requires config */
#if BLE_NIMBLE && defined(CONFIG_BT_NIMBLE_L2CAP_COC_MAX_NUM) && (CONFIG_BT_NIMBLE_L2CAP_COC_MAX_NUM > 0)
#define BLE_L2CAP_SUPPORTED 1
#else
#define BLE_L2CAP_SUPPORTED 0
#endif
