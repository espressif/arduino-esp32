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
 * @brief Backend-aware conversion between the public @ref BTAddress and the
 *        active BLE stack's native address layout.
 *
 * `BTAddress` stores addresses LSB-first (the Bluetooth on-air order). Bluedroid
 * public APIs take `esp_bd_addr_t`, which is MSB-first; NimBLE APIs take
 * LSB-first bytes. Backend code uses these helpers at the native-call boundary
 * instead of spelling `toEspBdAddr()` / `data()` at every call site.
 *
 * The declarations are stack-agnostic; the active backend's implementation is
 * selected by the build (BLEAddressUtil.nimble.cpp / BLEAddressUtil.bluedroid.cpp).
 */

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "BTAddress.h"

namespace BLEAddressUtil {

/**
 * @brief Write @p address in the active BLE backend's native byte order.
 * @param address Public LSB-first address.
 * @param out Receives 6 bytes in the backend's native order.
 */
void toNative(const BTAddress &address, uint8_t out[6]);

/**
 * @brief Build a @ref BTAddress from bytes in the active backend's native order.
 * @param in 6 bytes as returned by the native stack.
 * @param type Address type.
 */
BTAddress fromNative(const uint8_t in[6], BTAddress::Type type = BTAddress::Type::Public);

}  // namespace BLEAddressUtil

#endif /* BLE_ENABLED */
