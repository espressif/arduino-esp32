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
 * @brief Characteristic Properties -- Bluetooth Core Spec Vol 3, Part G, 3.3.1.1.
 *
 * The list of ATT operations a characteristic supports. These are independent
 * flags rather than a hierarchy: none of them implies any other, so declare
 * exactly the ones you want clients to be able to use.
 *
 * @ref BLEProperty::Write and @ref BLEProperty::WriteNR in particular are two
 * different ATT operations, not two spellings of the same one. `Write` is the
 * acknowledged Write Request, where the client learns whether the write
 * succeeded. `WriteNR` is the unacknowledged Write Command, which is cheaper
 * and lower latency but reports nothing back. Declaring one does not enable
 * the other, and declaring both is perfectly normal when you want the client
 * to pick per write, which is what the Nordic UART RX characteristic and HID
 * output reports do.
 *
 * Combine with bitwise OR: `BLEProperty::Read | BLEProperty::Notify`.
 */
enum class BLEProperty : uint8_t {
  Broadcast = 0x01,      ///< Value can be included in advertising data
  Read = 0x02,           ///< Value can be read
  WriteNR = 0x04,        ///< Value can be written without response
  Write = 0x08,          ///< Value can be written with response
  Notify = 0x10,         ///< Value can be notified (server-initiated, no ACK)
  Indicate = 0x20,       ///< Value can be indicated (server-initiated, with ACK)
  SignedWrite = 0x40,    ///< Value supports authenticated signed writes
  ExtendedProps = 0x80,  ///< Extended properties descriptor is present
};

inline constexpr BLEProperty operator|(BLEProperty a, BLEProperty b) {
  return static_cast<BLEProperty>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline constexpr BLEProperty &operator|=(BLEProperty &a, BLEProperty b) {
  return a = a | b;
}

/**
 * @brief Test whether @p a contains every bit of @p b.
 */
inline constexpr bool operator&(BLEProperty a, BLEProperty b) {
  return static_cast<uint8_t>(b) != 0 && (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) == static_cast<uint8_t>(b);
}

/**
 * @brief Attribute Access Permissions -- Bluetooth Core Spec Vol 3, Part F, 3.2.5.
 *
 * A permission says what a peer may do with an attribute and how well
 * protected the link has to be before it may do it. Every name is a direction
 * (`Read`, `Write`, or `ReadWrite`) followed by a security level, always in
 * that order, so each permission has exactly one spelling.
 *
 * The levels, from weakest to strongest:
 *
 * - `Open`: no protection at all, any peer may access it without pairing.
 * - `Encrypted`: any pairing will do, including Just Works.
 * - `Authenticated`: MITM-protected pairing, which also requires encryption.
 * - `Authorized`: the application decides per access through
 *   @c BLESecurity::onAuthorization.
 *
 * The level is always spelled out, including `Open`, so granting unprotected
 * access is something you have to ask for by name rather than something you
 * get by reaching for the shortest value.
 *
 * Use `|` when the two directions need different levels:
 *
 * @code
 *   // Anyone may read the value, only a bonded peer may change it.
 *   svc.createCharacteristic(uuid, BLEProperty::Read | BLEProperty::Write, BLEPermission::ReadOpen | BLEPermission::WriteEncrypted);
 * @endcode
 *
 * The mapping to the native stacks is fail-closed: a characteristic advertises
 * a read or write property only when the matching permission direction is
 * declared. Notify-only and indicate-only characteristics therefore take
 * @ref BLEPermission::None, which grants no direct access.
 */
enum class BLEPermission : uint16_t {
  None = 0x0000,  ///< No access permitted.

  ReadOpen = 0x0001,                           ///< Read with no security requirement at all.
  ReadEncrypted = 0x0002,                      ///< Read requires an encrypted link.
  ReadAuthenticated = 0x0004 | ReadEncrypted,  ///< Read requires MITM-authenticated pairing, which implies encryption.
  ReadAuthorized = 0x0008,                     ///< Read gated by the application authorization callback.

  WriteOpen = 0x0010,                            ///< Write with no security requirement at all.
  WriteEncrypted = 0x0020,                       ///< Write requires an encrypted link.
  WriteAuthenticated = 0x0040 | WriteEncrypted,  ///< Write requires MITM-authenticated pairing, which implies encryption.
  WriteAuthorized = 0x0080,                      ///< Write gated by the application authorization callback.

  ReadWriteOpen = ReadOpen | WriteOpen,                             ///< Read and write, neither with any security requirement.
  ReadWriteEncrypted = ReadEncrypted | WriteEncrypted,              ///< Read and write both require an encrypted link.
  ReadWriteAuthenticated = ReadAuthenticated | WriteAuthenticated,  ///< Read and write both require MITM-authenticated pairing.
  ReadWriteAuthorized = ReadAuthorized | WriteAuthorized,           ///< Read and write both gated by the authorization callback.
};

/**
 * @brief Combine permissions, typically to give each direction its own level.
 */
inline constexpr BLEPermission operator|(BLEPermission a, BLEPermission b) {
  return static_cast<BLEPermission>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

inline constexpr BLEPermission &operator|=(BLEPermission &a, BLEPermission b) {
  return a = a | b;
}

/**
 * @brief Test whether @p a grants everything @p b asks for.
 *
 * This is a containment test, not a plain bitwise AND, because the
 * `Authenticated` values carry the encryption bit as well. Testing against
 * @ref BLEPermission::ReadAuthenticated is therefore true only for a
 * permission that really is authenticated, and not for one that is merely
 * encrypted, while testing a merely encrypted requirement against an
 * authenticated permission still succeeds.
 */
inline constexpr bool operator&(BLEPermission a, BLEPermission b) {
  return static_cast<uint16_t>(b) != 0 && (static_cast<uint16_t>(a) & static_cast<uint16_t>(b)) == static_cast<uint16_t>(b);
}

// ---------------------------------------------------------------------------
// GATT characteristic descriptors: 16-bit UUIDs and value bit-fields.
// Bluetooth Core Spec v5.x, Vol 3, Part G, §3.3.3 (characteristic descriptors)
// and §3.3.3.3 (Client Characteristic Configuration Descriptor value). Shared by
// both backends and the characteristic validator so there is one source instead
// of scattered magic numbers.
// ---------------------------------------------------------------------------

// GATT characteristic descriptor UUIDs (16-bit). §3.3.3.1-.4.
constexpr uint16_t BLE_DSC_UUID16_EXT_PROPS = 0x2900;  // Characteristic Extended Properties
constexpr uint16_t BLE_DSC_UUID16_USER_DESC = 0x2901;  // Characteristic User Description
constexpr uint16_t BLE_DSC_UUID16_CCCD = 0x2902;       // Client Characteristic Configuration
constexpr uint16_t BLE_DSC_UUID16_SCCD = 0x2903;       // Server Characteristic Configuration
constexpr uint16_t BLE_DSC_UUID16_PRES_FMT = 0x2904;   // Characteristic Presentation Format

// Extended Properties (0x2900) value bit-field. §3.3.3.1.
constexpr uint16_t BLE_EXT_PROP_RELIABLE_WRITE = 0x0001;  // Bit 0: reliable write
constexpr uint16_t BLE_EXT_PROP_WRITABLE_AUX = 0x0002;    // Bit 1: writable auxiliaries

// CCCD (0x2902) value bit-field. §3.3.3.3.
constexpr uint16_t BLE_CCCD_NOTIFY = 0x0001;    // Bit 0: notifications enabled
constexpr uint16_t BLE_CCCD_INDICATE = 0x0002;  // Bit 1: indications enabled

#endif /* BLE_ENABLED */
