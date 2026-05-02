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

#pragma once

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include "BLEUUID.h"
#include "BLEAdvertisementData.h"

/**
 * @brief Apple iBeacon helper.
 *
 * Builds/parses iBeacon advertisement data. The manufacturer-specific data
 * follows the Apple iBeacon format (company ID 0x004C, type 0x0215).
 */
class BLEBeacon {
public:
  /**
   * @brief Construct an iBeacon with default values (Apple company ID 0x004C, signal power -59 dBm).
   */
  BLEBeacon();

  /**
   * @brief Get the 128-bit Proximity UUID.
   * @return The proximity UUID that identifies the beacon group.
   */
  BLEUUID getProximityUUID() const;

  /**
   * @brief Set the 128-bit Proximity UUID.
   * @param uuid The proximity UUID identifying this beacon group.
   */
  void setProximityUUID(const BLEUUID &uuid);

  /**
   * @brief Get the major identifier value.
   * @return The 16-bit major value.
   */
  uint16_t getMajor() const;

  /**
   * @brief Set the major identifier value.
   * @param major 16-bit major value for grouping beacons.
   */
  void setMajor(uint16_t major);

  /**
   * @brief Get the minor identifier value.
   * @return The 16-bit minor value.
   */
  uint16_t getMinor() const;

  /**
   * @brief Set the minor identifier value.
   * @param minor 16-bit minor value for identifying individual beacons.
   */
  void setMinor(uint16_t minor);

  /**
   * @brief Get the manufacturer ID (company code).
   * @return The 16-bit manufacturer ID (default: 0x004C for Apple).
   */
  uint16_t getManufacturerId() const;

  /**
   * @brief Set the manufacturer ID (company code).
   * @param id 16-bit Bluetooth SIG company identifier.
   */
  void setManufacturerId(uint16_t id);

  /**
   * @brief Get the measured signal power at 1 meter.
   * @return Calibrated TX power in dBm.
   */
  int8_t getSignalPower() const;

  /**
   * @brief Set the measured signal power at 1 meter.
   * @param power Calibrated TX power in dBm (typically -40 to -80).
   */
  void setSignalPower(int8_t power);

  /**
   * @brief Build a BLEAdvertisementData payload from the current iBeacon fields.
   * @return Advertisement data ready to pass to BLEAdvertising.
   */
  BLEAdvertisementData getAdvertisementData() const;

  /**
   * @brief Parse iBeacon fields from a raw manufacturer-specific payload.
   * @param payload Pointer to the manufacturer-specific data bytes.
   * @param len     Length of the payload in bytes.
   */
  void setFromPayload(const uint8_t *payload, size_t len);

private:
  uint16_t _manufacturerId = 0x004C;
  BLEUUID _proximityUUID;
  uint16_t _major = 0;
  uint16_t _minor = 0;
  int8_t _signalPower = -59;
};

#endif /* BLE_ENABLED */
