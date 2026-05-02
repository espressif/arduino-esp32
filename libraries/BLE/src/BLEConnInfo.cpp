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

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include "BLEConnInfo.h"
#include "BLEAdvTypes.h"
#include <string.h>

#include "impl/BLEConnInfoData.h"

/**
 * @brief Access the internal Data struct (mutable).
 * @return Pointer to the Data overlay within the inline storage buffer.
 * @note Static-asserts that Data fits within kStorageSize at compile time.
 */
BLEConnInfo::Data *BLEConnInfo::data() {
  static_assert(sizeof(Data) <= kStorageSize, "BLEConnInfo::Data exceeds kStorageSize");
  return reinterpret_cast<Data *>(_storage);
}

/**
 * @brief Access the internal Data struct (const).
 * @return Const pointer to the Data overlay within the inline storage buffer.
 */
const BLEConnInfo::Data *BLEConnInfo::data() const {
  return reinterpret_cast<const Data *>(_storage);
}

/**
 * @brief Construct an empty (invalid) connection info with zeroed storage.
 */
BLEConnInfo::BLEConnInfo() {
  memset(_storage, 0, sizeof(_storage));
}

/**
 * @brief Get the connection handle assigned by the controller.
 * @return The connection handle, or 0 if this object is invalid.
 */
uint16_t BLEConnInfo::getHandle() const {
  return _valid ? data()->handle : 0;
}

/**
 * @brief Get the over-the-air peer address.
 * @return The peer's address, or a default-constructed BTAddress if invalid.
 */
BTAddress BLEConnInfo::getAddress() const {
  return _valid ? data()->address : BTAddress();
}

/**
 * @brief Get the identity address (after IRK resolution).
 * @return The resolved identity address, or a default BTAddress if invalid.
 */
BTAddress BLEConnInfo::getIdAddress() const {
  return _valid ? data()->idAddress : BTAddress();
}

/**
 * @brief Get the negotiated ATT MTU for this connection.
 * @return The MTU in bytes, or 0 if invalid.
 */
uint16_t BLEConnInfo::getMTU() const {
  return _valid ? data()->mtu : 0;
}

/**
 * @brief Check if the link is encrypted.
 * @return True if valid and the link is encrypted.
 */
bool BLEConnInfo::isEncrypted() const {
  return _valid && data()->encrypted;
}

/**
 * @brief Check if authenticated pairing was used.
 * @return True if valid and the connection was authenticated.
 */
bool BLEConnInfo::isAuthenticated() const {
  return _valid && data()->authenticated;
}

/**
 * @brief Check if a bond (LTK) exists for this peer.
 * @return True if valid and the peer is bonded.
 */
bool BLEConnInfo::isBonded() const {
  return _valid && data()->bonded;
}

/**
 * @brief Get the encryption key size in bytes.
 * @return Key size (7-16), or 0 if invalid.
 */
uint8_t BLEConnInfo::getSecurityKeySize() const {
  return _valid ? data()->keySize : 0;
}

/**
 * @brief Get the connection interval in units of 1.25 ms.
 * @return The interval value, or 0 if invalid.
 */
uint16_t BLEConnInfo::getInterval() const {
  return _valid ? data()->interval : 0;
}

/**
 * @brief Get the peripheral latency (connection events the peripheral may skip).
 * @return The latency value, or 0 if invalid.
 */
uint16_t BLEConnInfo::getLatency() const {
  return _valid ? data()->latency : 0;
}

/**
 * @brief Get the supervision timeout in units of 10 ms.
 * @return The supervision timeout, or 0 if invalid.
 */
uint16_t BLEConnInfo::getSupervisionTimeout() const {
  return _valid ? data()->supervisionTimeout : 0;
}

/**
 * @brief Get the transmit PHY in use.
 * @return The TX PHY, or PHY_1M if invalid.
 */
BLEPhy BLEConnInfo::getTxPhy() const {
  return _valid ? static_cast<BLEPhy>(data()->txPhy) : BLEPhy::PHY_1M;
}

/**
 * @brief Get the receive PHY in use.
 * @return The RX PHY, or PHY_1M if invalid.
 */
BLEPhy BLEConnInfo::getRxPhy() const {
  return _valid ? static_cast<BLEPhy>(data()->rxPhy) : BLEPhy::PHY_1M;
}

/**
 * @brief Check if this device is the central (initiator) of the connection.
 * @return True if valid and this device is the central.
 */
bool BLEConnInfo::isCentral() const {
  return _valid && data()->central;
}

/**
 * @brief Check if this device is the peripheral (advertiser) of the connection.
 * @return True if valid and this device is the peripheral.
 */
bool BLEConnInfo::isPeripheral() const {
  return _valid && !data()->central;
}

/**
 * @brief Get the last known RSSI for this connection.
 * @return RSSI in dBm, or 0 if invalid.
 */
int8_t BLEConnInfo::getRSSI() const {
  return _valid ? data()->rssi : 0;
}

/**
 * @brief Check if this object holds valid connection data.
 * @return True if the connection info was populated by the backend.
 */
BLEConnInfo::operator bool() const {
  return _valid;
}

#endif /* BLE_ENABLED */
