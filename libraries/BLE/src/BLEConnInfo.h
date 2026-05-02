/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
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
#include "BTAddress.h"

enum class BLEPhy : uint8_t;

/**
 * @brief Stack-agnostic connection descriptor.
 *
 * Replaces all backend-specific parameter types (esp_ble_gatts_cb_param_t,
 * ble_gap_conn_desc, etc.) in callback signatures. This is a pure value type
 * with no heap allocation -- connection data is stored inline.
 *
 * Backend code creates instances via the friend struct BLEConnInfoImpl.
 */
class BLEConnInfo {
public:
  /**
   * @brief Construct an empty (invalid) connection info object.
   */
  BLEConnInfo();

  /**
   * @brief Connection handle assigned by the controller.
   * @return The connection handle.
   */
  uint16_t getHandle() const;

  /**
   * @brief Over-the-air (OTA) peer address.
   * @return The peer's advertised address.
   */
  BTAddress getAddress() const;

  /**
   * @brief Identity address (after IRK resolution).
   * @return The resolved identity address, or the OTA address if no IRK is available.
   */
  BTAddress getIdAddress() const;

  /**
   * @brief Negotiated ATT MTU for this connection.
   * @return The current MTU in bytes.
   */
  uint16_t getMTU() const;

  /**
   * @brief Check if the link is encrypted.
   * @return True if the link is encrypted.
   */
  bool isEncrypted() const;

  /**
   * @brief Check if authenticated pairing was used.
   * @return True if the connection was authenticated.
   */
  bool isAuthenticated() const;

  /**
   * @brief Check if a bond (LTK) exists for this peer.
   * @return True if the peer is bonded.
   */
  bool isBonded() const;

  /**
   * @brief Encryption key size.
   * @return Key size in bytes (7-16).
   */
  uint8_t getSecurityKeySize() const;

  /**
   * @brief Connection interval in units of 1.25 ms.
   * @return The connection interval.
   */
  uint16_t getInterval() const;

  /**
   * @brief Peripheral latency (number of connection events the peripheral may skip).
   * @return The latency value.
   */
  uint16_t getLatency() const;

  /**
   * @brief Supervision timeout in units of 10 ms.
   * @return The supervision timeout.
   */
  uint16_t getSupervisionTimeout() const;

  /**
   * @brief TX PHY in use.
   * @return The transmit PHY.
   * @note Returns PHY_1M on non-BLE5 chips.
   */
  BLEPhy getTxPhy() const;

  /**
   * @brief RX PHY in use.
   * @return The receive PHY.
   * @note Returns PHY_1M on non-BLE5 chips.
   */
  BLEPhy getRxPhy() const;

  /**
   * @brief Check if this device is the central (initiator) of the connection.
   * @return True if this device is the central.
   */
  bool isCentral() const;

  /**
   * @brief Check if this device is the peripheral (advertiser) of the connection.
   * @return True if this device is the peripheral.
   */
  bool isPeripheral() const;

  /**
   * @brief Last known RSSI for this connection.
   * @return RSSI in dBm (-127 to +20), or 0 if unavailable.
   */
  int8_t getRSSI() const;

  /**
   * @brief Check if this object holds valid connection data.
   * @return True if the connection info is valid, false otherwise.
   */
  explicit operator bool() const;

  static constexpr size_t kStorageSize = 56;

private:
  friend struct BLEConnInfoImpl;
  struct Data;
  alignas(8) uint8_t _storage[kStorageSize]{};
  bool _valid = false;

  Data *data();
  const Data *data() const;
};

#endif /* BLE_ENABLED */
