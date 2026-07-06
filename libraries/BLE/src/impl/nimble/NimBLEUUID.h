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

#include "impl/common/BLEGuards.h"
#if BLE_NIMBLE

#include "BLEUUID.h"
#include <host/ble_uuid.h>

/**
 * @brief Convert a public @ref BLEUUID into NimBLE's @c ble_uuid_any_t.
 * @param uuid  Source UUID in the Arduino wrapper format.
 * @param out   NimBLE UUID output; previous contents are replaced.
 */
void nimbleUuidFromPublic(const BLEUUID &uuid, ble_uuid_any_t &out);

/**
 * @brief Convert a NimBLE @c ble_uuid_any_t to the public @ref BLEUUID.
 * @param u  Host UUID in NimBLE's wire representation.
 * @return   Equivalent @ref BLEUUID (16-, 32-, or 128-bit as appropriate).
 */
BLEUUID nimbleUuidToPublic(const ble_uuid_any_t &u);

#endif /* BLE_NIMBLE */
