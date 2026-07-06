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
 * @brief Bridge that builds and updates the stack-agnostic BLEConnInfo from
 *        Bluedroid GAP/GATT event data. Shared by the Bluedroid client, server,
 *        and security event handlers so a single builder feeds every code path.
 */

#include "impl/common/BLEGuards.h"
#if BLE_BLUEDROID

#include "impl/common/BLEConnInfoData.h"

#include <stdint.h>

struct BLEConnInfoImpl {
  /**
   * @brief Build a BLEConnInfo snapshot for a Bluedroid link.
   * @param connId GATT connection identifier (0 when unknown, e.g. on a bare
   *               security event before a GATT connection exists).
   * @param bda Six-byte peer Bluetooth device address in Bluedroid wire order.
   * @param mtu Negotiated ATT MTU (defaults to the 23-byte minimum).
   * @param central True when this device is the central (client), false when it
   *                is the peripheral (server).
   * @param addrType Peer address type from the stack event (defaults to Public).
   * @return A valid BLEConnInfo describing the link.
   * @note Bluedroid does not surface a resolved identity address on connect or
   *       security events, so the identity address falls back to the OTA address
   *       per the getIdAddress() contract, matching NimBLE for an unresolved peer.
   */
  static BLEConnInfo make(
    uint16_t connId, const uint8_t bda[6], uint16_t mtu = 23, bool central = false, BTAddress::Type addrType = BTAddress::Type::Public
  );

  /**
   * @brief Build a minimal peripheral-role BLEConnInfo from a peer address only.
   * @param bda Six-byte peer Bluetooth device address in Bluedroid wire order.
   * @param addrType Peer address type (defaults to Public when the event omits it).
   * @return A valid BLEConnInfo with a zero handle and default link parameters.
   */
  static BLEConnInfo make(const uint8_t bda[6], BTAddress::Type addrType = BTAddress::Type::Public);

  /**
   * @brief Overwrite the MTU field on an existing BLEConnInfo if it is valid.
   * @param info Connection info to update in place; no-op if invalid.
   * @param mtu New negotiated MTU value.
   */
  static void setMTU(BLEConnInfo &info, uint16_t mtu);

  /**
   * @brief Update connection interval, latency, and supervision timeout if valid.
   * @param info Connection info to update in place; no-op if invalid.
   * @param interval Connection interval in 1.25 ms units.
   * @param latency Peripheral latency in connection events.
   * @param timeout Supervision timeout in 10 ms units.
   */
  static void setConnParams(BLEConnInfo &info, uint16_t interval, uint16_t latency, uint16_t timeout);

  /**
   * @brief Update the security flags after pairing/encryption completes.
   * @param info Connection info to update in place; no-op if invalid.
   * @param encrypted Link is now encrypted.
   * @param authenticated Pairing produced an authenticated (MITM) key.
   * @param bonded Keys were stored (bonded).
   */
  static void updateSecurityFlags(BLEConnInfo &info, bool encrypted, bool authenticated, bool bonded);

  /**
   * @brief Update security flags from a successful Bluedroid AUTH_CMPL event.
   * @param info Connection info to update in place; no-op if invalid.
   * @param authMode esp_ble_auth_req_t bitmask (ble_security.auth_cmpl.auth_mode).
   * @note Sets encrypted=true and decodes authenticated (MITM bit) and bonded
   *       (BOND bit) independently, matching NimBLE's sec_state fields.
   */
  static void updateSecurityFromAuthComplete(BLEConnInfo &info, uint8_t authMode);

  /**
   * @brief Overwrite the peer address type on an existing BLEConnInfo if valid.
   * @param info Connection info to update in place; no-op if invalid.
   * @param addrType Peer address type from a Bluedroid GAP/GATT event.
   * @note Also refreshes @c idAddress to match the OTA address (Bluedroid has no
   *       separate resolved-identity field).
   */
  static void setAddressType(BLEConnInfo &info, BTAddress::Type addrType);

  /**
   * @brief Cache the live TX/RX PHY reported by a Bluedroid BLE5 PHY event.
   * @param info Connection info to update in place; no-op if invalid.
   * @param txPhy Transmitter PHY from @c READ_PHY / @c PHY_UPDATE complete.
   * @param rxPhy Receiver PHY from the same event.
   * @note Single-PHY event values map 1:1 onto @c BLEPhy (1M=1, 2M=2, Coded=3).
   */
  static void setPhy(BLEConnInfo &info, uint8_t txPhy, uint8_t rxPhy);
};

#endif /* BLE_BLUEDROID */
