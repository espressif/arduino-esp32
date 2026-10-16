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
 * @brief Single backend selector for the whole BLE library.
 *
 * This is the ONE place that maps the active stack (NimBLE or Bluedroid) to the
 * concrete per-type @c Impl definitions. A public translation unit that needs
 * the full layout of any backend @c Impl includes just this header instead of a
 * per-type @c *Backend.h selector. Each per-type header is @c #pragma @c once and
 * internally guarded, so pulling the full set in is cheap and order-independent.
 *
 * Keep this header PURE SELECTION: no function declarations, no shim classes,
 * no logic. Backend operations live in their own headers (e.g.
 * @c bleServerRemoveService in @c server/BLEServerImpl.h; the @c BLESecurity
 * pairing hooks on @c BLESecurityImplCommon in @c security/BLESecurityImpl.h).
 */

#include "core/BLEGuards.h"

#if BLE_NIMBLE

#include "server/BLEServer.nimble.h"
#include "gatt/BLEGattAttributes.nimble.h"
#include "client/BLEClient.nimble.h"
#include "remote/BLERemoteGatt.nimble.h"
#include "scan/BLEScan.nimble.h"
#include "advertising/BLEAdvertising.nimble.h"
#include "security/BLESecurity.nimble.h"

#elif BLE_BLUEDROID

#include "server/BLEServer.bluedroid.h"
#include "gatt/BLEGattAttributes.bluedroid.h"
#include "client/BLEClient.bluedroid.h"
#include "remote/BLERemoteGatt.bluedroid.h"
#include "scan/BLEScan.bluedroid.h"
#include "advertising/BLEAdvertising.bluedroid.h"
#include "security/BLESecurity.bluedroid.h"

#endif

// After the active backend has defined every Foo::Impl, verify the backend contract
// (two-layer shape) at compile time. See libraries/BLE/DESIGN.md.
#include "core/BLEBackendContract.h"
