/*
 * Copyright 2026 Espressif Systems (Shanghai) PTE LTD
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

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include <stdint.h>
#include "types/BLEProperty.h"

/**
 * @brief Which server-initiated deliveries a GATT client subscribes to,
 *        and which ATT PDU arrived in a subscribe callback.
 *
 * The two values are independent bits of the Client Characteristic
 * Configuration Descriptor (0x2902; Bluetooth Core Spec Vol 3, Part G,
 * §3.3.3.3): a characteristic that declares both the Notify and Indicate
 * properties may be sent either way, and the server chooses per
 * transmission. Combine with bitwise OR to receive both. A subscribe
 * callback reports exactly one of @ref Notify or @ref Indicate:
 *
 * @code
 * chr.subscribe(BLESubscription::Notify | BLESubscription::Indicate, cb);
 * @endcode
 *
 * There is deliberately no "both" value: the combination is spelled as the
 * OR of its parts, exactly like @ref BLEProperty.
 */
enum class BLESubscription : uint8_t {
  None = 0x00,      ///< Subscribed to nothing (see `unsubscribe()`).
  Notify = 0x01,    ///< Notifications enabled (CCCD bit 0).
  Indicate = 0x02,  ///< Indications enabled (CCCD bit 1).
};

static_assert(static_cast<uint8_t>(BLESubscription::Notify) == BLE_CCCD_NOTIFY, "BLESubscription::Notify must be CCCD bit 0");
static_assert(static_cast<uint8_t>(BLESubscription::Indicate) == BLE_CCCD_INDICATE, "BLESubscription::Indicate must be CCCD bit 1");

inline constexpr BLESubscription operator|(BLESubscription a, BLESubscription b) {
  return static_cast<BLESubscription>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline constexpr BLESubscription &operator|=(BLESubscription &a, BLESubscription b) {
  return a = a | b;
}

/**
 * @brief Test whether @p a contains every bit of @p b.
 */
inline constexpr bool operator&(BLESubscription a, BLESubscription b) {
  return static_cast<uint8_t>(b) != 0 && (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) == static_cast<uint8_t>(b);
}

#endif /* BLE_ENABLED */
