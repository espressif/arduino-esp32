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
 * @file BLEIso.bluedroid.h
 * @brief Bluedroid host helpers for the shared IDF ISO transport.
 *
 * Bluedroid's ISO adapter hooks BTA directly for GAP events, so only public
 * GATTC connection-ID to HCI ACL handle resolution lives here.
 */

#include "core/BLEGuards.h"

#if BLE_BLUEDROID && BLE_ISO_SUPPORTED

#include "iso/BLEIsoHost.h"

#endif /* BLE_BLUEDROID && BLE_ISO_SUPPORTED */
