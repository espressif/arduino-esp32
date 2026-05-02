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

#include <stdint.h>

/**
 * @brief Characteristic Properties -- Bluetooth Core Spec Vol 3, Part G, 3.3.1.1.
 *
 * Describes what operations a characteristic supports.
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

inline constexpr BLEProperty operator|=(BLEProperty &a, BLEProperty b) {
  return a = a | b;
}

inline constexpr bool operator&(BLEProperty a, BLEProperty b) {
  return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

/**
 * @brief Attribute Access Permissions -- Bluetooth Core Spec Vol 3, Part F, 3.2.5.
 *
 * Controls security requirements for read/write access. In this library the
 * mapping is fail-closed: a characteristic advertises a read or write
 * property only if the matching permission direction is also declared.
 * `BLEPermission::None` means "no access"; use one of the
 * `BLEPermissions::` presets or combine bits manually for specific security
 * levels.
 */
enum class BLEPermission : uint16_t {
  None = 0x0000,                ///< No access permitted
  Read = 0x0001,                ///< Open read (no encryption required)
  ReadEncrypted = 0x0002,       ///< Read requires an encrypted link
  ReadAuthenticated = 0x0004,   ///< Read requires MITM-authenticated pairing
  ReadAuthorized = 0x0008,      ///< Read gated by application authorization callback
  Write = 0x0010,               ///< Open write (no encryption required)
  WriteEncrypted = 0x0020,      ///< Write requires an encrypted link
  WriteAuthenticated = 0x0040,  ///< Write requires MITM-authenticated pairing
  WriteAuthorized = 0x0080,     ///< Write gated by application authorization callback
};

inline constexpr BLEPermission operator|(BLEPermission a, BLEPermission b) {
  return static_cast<BLEPermission>(static_cast<uint16_t>(a) | static_cast<uint16_t>(b));
}

inline constexpr BLEPermission operator|=(BLEPermission &a, BLEPermission b) {
  return a = a | b;
}

inline constexpr bool operator&(BLEPermission a, BLEPermission b) {
  return (static_cast<uint16_t>(a) & static_cast<uint16_t>(b)) != 0;
}

/**
 * @brief Semantic permission presets.
 *
 * Self-documenting combinations for the common cases. Names encode both
 * direction (`Read`, `Write`, `ReadWrite`) and security level
 * (`Open` / `Encrypted` / `Authenticated`). Use these rather than raw
 * bit combinations whenever possible.
 *
 * Example:
 * @code
 *   svc.createCharacteristic(uuid,
 *     BLEProperty::Read | BLEProperty::Write,
 *     BLEPermissions::OpenReadWrite);
 * @endcode
 *
 * Authenticated presets include the Encrypted bit as well, matching the
 * GATT security hierarchy (authenticated pairing implies encryption).
 */
namespace BLEPermissions {

// Open — readable/writable with no security requirement.
inline constexpr BLEPermission OpenRead = BLEPermission::Read;
inline constexpr BLEPermission OpenWrite = BLEPermission::Write;
inline constexpr BLEPermission OpenReadWrite = BLEPermission::Read | BLEPermission::Write;

// Encrypted — pairing required (no MITM).
inline constexpr BLEPermission EncryptedRead = BLEPermission::ReadEncrypted;
inline constexpr BLEPermission EncryptedWrite = BLEPermission::WriteEncrypted;
inline constexpr BLEPermission EncryptedReadWrite = BLEPermission::ReadEncrypted | BLEPermission::WriteEncrypted;

// Authenticated — MITM-protected pairing required (implies encryption).
inline constexpr BLEPermission AuthenticatedRead = BLEPermission::ReadEncrypted | BLEPermission::ReadAuthenticated;
inline constexpr BLEPermission AuthenticatedWrite = BLEPermission::WriteEncrypted | BLEPermission::WriteAuthenticated;
inline constexpr BLEPermission AuthenticatedReadWrite = AuthenticatedRead | AuthenticatedWrite;

// Authorized — application-level authorization callback gates access.
inline constexpr BLEPermission AuthorizedRead = BLEPermission::ReadAuthorized;
inline constexpr BLEPermission AuthorizedWrite = BLEPermission::WriteAuthorized;
inline constexpr BLEPermission AuthorizedReadWrite = BLEPermission::ReadAuthorized | BLEPermission::WriteAuthorized;

}  // namespace BLEPermissions

#endif /* BLE_ENABLED */
