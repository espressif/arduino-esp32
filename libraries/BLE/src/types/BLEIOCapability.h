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

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include <stdint.h>

/**
 * @brief Pairing input and output capability -- Bluetooth Core Spec Vol 3, Part H, 2.3.2.
 *
 * Describes what this device can show to a user and what it can accept from
 * one. The two peers exchange this during pairing and the result decides which
 * pairing method they end up using: Just Works, Passkey Entry, or Numeric
 * Comparison.
 *
 * This only describes the hardware. How strict the pairing has to be is a
 * separate question, answered by @c BLESecurity::setAuthenticationMode. A
 * board with a display and buttons can still choose to pair with Just Works,
 * but a board that reports @ref BLEIOCapability::NoInputNoOutput can never do
 * better than Just Works no matter what else it asks for.
 *
 * Values match the SMP I/O Capability field encoding, so they carry straight
 * through to both host stacks.
 */
enum class BLEIOCapability : uint8_t {
  DisplayOnly = 0,      ///< Can show a passkey, cannot take input.
  DisplayYesNo = 1,     ///< Can show a passkey and accept a yes/no confirmation.
  KeyboardOnly = 2,     ///< Can take a typed passkey, cannot show one.
  NoInputNoOutput = 3,  ///< No usable I/O, so pairing falls back to Just Works.
  KeyboardDisplay = 4,  ///< Both a keypad and a display, so every pairing method is available.
};

#endif /* BLE_ENABLED */
