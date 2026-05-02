/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2017 Neil Kolban
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

#include "BLEBeacon.h"
#include <cstring>

/**
 * @brief Construct an iBeacon with defaults (Apple company ID 0x004C, -59 dBm).
 */
BLEBeacon::BLEBeacon() {}

/**
 * @brief Get the 128-bit Proximity UUID.
 * @return The stored proximity UUID.
 */
BLEUUID BLEBeacon::getProximityUUID() const {
  return _proximityUUID;
}

/**
 * @brief Set the Proximity UUID, converting to 128-bit if needed.
 * @param uuid The proximity UUID (automatically promoted via to128()).
 */
void BLEBeacon::setProximityUUID(const BLEUUID &uuid) {
  _proximityUUID = uuid.to128();
}

/**
 * @brief Get the iBeacon major value.
 * @return 16-bit major identifier.
 */
uint16_t BLEBeacon::getMajor() const {
  return _major;
}

/**
 * @brief Set the iBeacon major value.
 * @param major 16-bit major value.
 */
void BLEBeacon::setMajor(uint16_t major) {
  _major = major;
}

/**
 * @brief Get the iBeacon minor value.
 * @return 16-bit minor identifier.
 */
uint16_t BLEBeacon::getMinor() const {
  return _minor;
}

/**
 * @brief Set the iBeacon minor value.
 * @param minor 16-bit minor value.
 */
void BLEBeacon::setMinor(uint16_t minor) {
  _minor = minor;
}

/**
 * @brief Get the company identifier used in the manufacturer data.
 * @return 16-bit Bluetooth SIG company ID.
 */
uint16_t BLEBeacon::getManufacturerId() const {
  return _manufacturerId;
}

/**
 * @brief Set the company identifier in the manufacturer data.
 * @param id Bluetooth SIG company identifier.
 */
void BLEBeacon::setManufacturerId(uint16_t id) {
  _manufacturerId = id;
}

/**
 * @brief Get the calibrated signal strength at 1 m (Tx power / measured power).
 * @return Power in dBm.
 */
int8_t BLEBeacon::getSignalPower() const {
  return _signalPower;
}

/**
 * @brief Set the signal strength at 1 m (used in the iBeacon MFG payload).
 * @param power TX power in dBm.
 */
void BLEBeacon::setSignalPower(int8_t power) {
  _signalPower = power;
}

/**
 * @brief Build a BLEAdvertisementData payload from the current iBeacon fields.
 * @return Advertisement data containing a Manufacturer Specific Data field with
 *         iBeacon type (0x02), length (0x15), 16-byte UUID in big-endian,
 *         major/minor in big-endian, and the signal power byte.
 */
BLEAdvertisementData BLEBeacon::getAdvertisementData() const {
  BLEAdvertisementData advData;

  // iBeacon format: 0xFF (manufacturer specific) + company ID (2) + type (0x02) + length (0x15)
  // + UUID (16 big-endian) + major (2 big-endian) + minor (2 big-endian) + TX power (1)
  uint8_t payload[23];
  payload[0] = 0x02;  // iBeacon type
  payload[1] = 0x15;  // iBeacon length (21 bytes)

  BLEUUID uuid128 = _proximityUUID.to128();
  const uint8_t *uuidData = uuid128.data();
  memcpy(payload + 2, uuidData, 16);

  payload[18] = (_major >> 8) & 0xFF;
  payload[19] = _major & 0xFF;
  payload[20] = (_minor >> 8) & 0xFF;
  payload[21] = _minor & 0xFF;
  payload[22] = static_cast<uint8_t>(_signalPower);

  advData.setManufacturerData(_manufacturerId, payload, sizeof(payload));
  return advData;
}

/**
 * @brief Parse iBeacon fields from raw manufacturer-specific data (after company ID).
 * @param payload Pointer to the manufacturer data starting at the iBeacon type byte.
 * @param len Length of the payload in bytes (must be >= 23).
 * @note Silently returns if len < 23 or the type/length header is not 0x02/0x15.
 *       UUID is stored directly (no byte reversal) since iBeacon uses big-endian.
 */
void BLEBeacon::setFromPayload(const uint8_t *payload, size_t len) {
  // Expects manufacturer-specific data (after company ID)
  // Format: type(1) + length(1) + UUID(16) + major(2) + minor(2) + power(1) = 23 bytes
  if (len < 23) {
    return;
  }
  if (payload[0] != 0x02 || payload[1] != 0x15) {
    return;
  }

  _proximityUUID = BLEUUID(payload + 2, 16, false);
  _major = (payload[18] << 8) | payload[19];
  _minor = (payload[20] << 8) | payload[21];
  _signalPower = static_cast<int8_t>(payload[22]);
}

#endif /* BLE_ENABLED */
