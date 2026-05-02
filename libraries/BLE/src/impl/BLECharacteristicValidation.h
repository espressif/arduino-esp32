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

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include "BLEProperty.h"
#include "BLEUUID.h"
#include "BLECharacteristic.h"  // for nested `BLECharacteristic::Impl`

/**
 * @brief GATT characteristic validation helpers.
 *
 * Validates a characteristic's properties, permissions and descriptor set
 * against the Bluetooth Core Spec. Hard errors prevent registration;
 * warnings are surfaced via `log_w` for developer visibility.
 *
 * Two validation stages are exposed:
 *   - Construction time: only properties & permissions are known
 *   - Registration time: descriptors are also known (runs after any
 *     backend-specific auto-creation, e.g. Bluedroid CCCD)
 *
 * The registration-time helper takes `BLECharacteristic::Impl` directly
 * because that's the type both backend registration paths already have
 * in hand; it also avoids round-tripping through the public handle just
 * to read fields that the validator needs. Callers must include the
 * appropriate backend header (or `impl/BLECharacteristicBackend.h`) so
 * the full `Impl` definition is visible.
 */

/**
 * @brief Validate properties/permissions combination at construction time.
 *
 * @param uuid  Characteristic UUID (used only for log context)
 * @param props Characteristic properties bitmask
 * @param perms Characteristic permissions bitmask
 * @return true if valid (no hard errors), false otherwise
 */
bool bleValidateCharProps(const BLEUUID &uuid, BLEProperty props, BLEPermission perms);

/**
 * @brief Validate the full characteristic (props/perms/descriptors) before
 *        registration with the native stack.
 *
 * @param chr          Characteristic implementation to validate
 * @param stackIsNimble true if NimBLE backend (stack auto-creates the CCCD;
 *                     a manually-added CCCD becomes a hard error)
 * @return true if valid (no hard errors), false otherwise
 */
bool bleValidateCharFinal(const BLECharacteristic::Impl &chr, bool stackIsNimble);

/**
 * @brief Validate a descriptor's permission combination at construction time.
 *
 * Applies two sets of rules:
 *   1. Generic — the descriptor must declare at least one read-or-write
 *      direction (fail-closed: `BLEPermission::None` means inaccessible),
 *      authorization requires a matching base direction, authenticated
 *      implies encrypted (warning), plain-open mixed with encrypted is
 *      flagged (warning).
 *   2. Reserved UUIDs (Bluetooth Core Spec Vol 3, Part G §3.3.3) — each
 *      reserved descriptor UUID has a required access profile (e.g. 0x2904
 *      must be read-only, 0x2902 must be read+write). Violations are hard
 *      errors except for 0x2901 where writability is conditional on the
 *      characteristic's Extended Properties descriptor.
 *
 * @param descUuid Descriptor UUID being created
 * @param chrUuid  Parent characteristic UUID (for log context only)
 * @param perms    Requested permissions
 * @return true if valid (no hard errors), false otherwise
 */
bool bleValidateDescProps(const BLEUUID &descUuid, const BLEUUID &chrUuid, BLEPermission perms);

#endif /* BLE_ENABLED */
