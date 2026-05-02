/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
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

#include <vector>
#include "WString.h"
#include "BTAddress.h"
#include "BLEUUID.h"
#include "BLEAdvTypes.h"
#include <memory>

class BLEScan;

/**
 * @brief Represents a single advertised BLE device discovered during scanning.
 *
 * Shared handle. Obtained via BLEScan callbacks or BLEScanResults.
 * Contains parsed advertisement data (name, UUIDs, manufacturer data, etc.).
 */
class BLEAdvertisedDevice {
public:
  BLEAdvertisedDevice();
  ~BLEAdvertisedDevice() = default;
  BLEAdvertisedDevice(const BLEAdvertisedDevice &) = default;
  BLEAdvertisedDevice &operator=(const BLEAdvertisedDevice &) = default;
  BLEAdvertisedDevice(BLEAdvertisedDevice &&) = default;
  BLEAdvertisedDevice &operator=(BLEAdvertisedDevice &&) = default;

  /**
   * @brief Check whether this handle refers to a valid advertised device.
   * @return True if the underlying device data is present, false otherwise.
   */
  explicit operator bool() const;

  /**
   * @brief Get the advertiser's BLE address.
   * @return The device address.
   */
  BTAddress getAddress() const;

  /**
   * @brief Get the address type (public, random, etc.).
   * @return The address type.
   */
  BTAddress::Type getAddressType() const;

  /**
   * @brief Get the advertised local name.
   * @return The device name, or an empty String if not present.
   */
  String getName() const;

  /**
   * @brief Get the received signal strength.
   * @return RSSI in dBm, or 0 if unavailable.
   */
  int8_t getRSSI() const;

  /**
   * @brief Get the advertised TX power level.
   * @return TX power in dBm, or 0 if not present.
   */
  int8_t getTXPower() const;

  /**
   * @brief Get the advertised appearance value.
   * @return The GAP appearance code, or 0 if not present.
   */
  uint16_t getAppearance() const;

  /**
   * @brief Get the advertisement PDU type.
   * @return The advertisement type enum value.
   */
  BLEAdvType getAdvType() const;

  /**
   * @brief Get the raw manufacturer-specific data.
   * @param len If non-null, receives the data length in bytes (including the company ID).
   * @return Pointer to the manufacturer data, or nullptr if not present.
   */
  const uint8_t *getManufacturerData(size_t *len) const;

  /**
   * @brief Get the manufacturer-specific data as a hex-encoded String.
   * @return Hex string of the manufacturer data, or an empty String if not present.
   */
  String getManufacturerDataString() const;

  /**
   * @brief Get the company identifier from the manufacturer-specific data.
   * @return The 16-bit company ID, or 0 if manufacturer data is not present.
   */
  uint16_t getManufacturerCompanyId() const;

  /**
   * @brief Get the number of service UUIDs in the advertisement.
   * @return The service UUID count.
   */
  size_t getServiceUUIDCount() const;

  /**
   * @brief Get a service UUID by index.
   * @param index Zero-based index into the advertised service UUID list.
   * @return The service UUID, or an invalid UUID if index is out of range.
   */
  BLEUUID getServiceUUID(size_t index = 0) const;

  /**
   * @brief Check if the advertisement contains any service UUIDs.
   * @return True if at least one service UUID is present.
   */
  bool haveServiceUUID() const;

  /**
   * @brief Check if a specific service UUID is advertised.
   * @param uuid The service UUID to search for.
   * @return True if the UUID is in the advertisement.
   */
  bool isAdvertisingService(const BLEUUID &uuid) const;

  /**
   * @brief Get the number of service data entries in the advertisement.
   * @return The service data entry count.
   */
  size_t getServiceDataCount() const;

  /**
   * @brief Get raw service data by index.
   * @param index Zero-based index into the service data list.
   * @param len   If non-null, receives the data length in bytes.
   * @return Pointer to the service data, or nullptr if index is out of range.
   */
  const uint8_t *getServiceData(size_t index, size_t *len) const;

  /**
   * @brief Get service data as a hex-encoded String.
   * @param index Zero-based index into the service data list.
   * @return Hex string, or an empty String if index is out of range.
   */
  String getServiceDataString(size_t index = 0) const;

  /**
   * @brief Get the UUID associated with a service data entry.
   * @param index Zero-based index into the service data list.
   * @return The service data UUID, or an invalid UUID if index is out of range.
   */
  BLEUUID getServiceDataUUID(size_t index = 0) const;

  /**
   * @brief Check if the advertisement contains any service data.
   * @return True if at least one service data entry is present.
   */
  bool haveServiceData() const;

  /**
   * @brief Get a pointer to the raw advertisement payload.
   * @return Pointer to the AD structure bytes.
   */
  const uint8_t *getPayload() const;

  /**
   * @brief Get the total length of the raw advertisement payload.
   * @return Payload length in bytes.
   */
  size_t getPayloadLength() const;

  /**
   * @brief Check if a local name was present in the advertisement.
   * @return True if the name field is present.
   */
  bool haveName() const;

  /**
   * @brief Check if an RSSI value is available.
   * @return True if RSSI was recorded.
   */
  bool haveRSSI() const;

  /**
   * @brief Check if a TX power level was present in the advertisement.
   * @return True if the TX power field is present.
   */
  bool haveTXPower() const;

  /**
   * @brief Check if an appearance value was present in the advertisement.
   * @return True if the appearance field is present.
   */
  bool haveAppearance() const;

  /**
   * @brief Check if manufacturer-specific data was present in the advertisement.
   * @return True if manufacturer data is present.
   */
  bool haveManufacturerData() const;

  /**
   * @brief Check if the advertiser accepts connections.
   * @return True if the advertisement is connectable.
   */
  bool isConnectable() const;

  /**
   * @brief Check if the advertiser accepts scan requests.
   * @return True if the advertisement is scannable.
   */
  bool isScannable() const;

  /**
   * @brief Check if this is a directed advertisement.
   * @return True if the advertisement is directed to a specific device.
   */
  bool isDirected() const;

  /**
   * @brief Check if this is a legacy (non-extended) advertisement.
   * @return True for legacy ADV_IND / ADV_SCAN_IND / ADV_NONCONN_IND PDUs.
   */
  bool isLegacyAdvertisement() const;

  /**
   * @brief Get the primary advertising PHY (extended advertising only).
   * @return The primary PHY.
   */
  BLEPhy getPrimaryPhy() const;

  /**
   * @brief Get the secondary advertising PHY (extended advertising only).
   * @return The secondary PHY.
   */
  BLEPhy getSecondaryPhy() const;

  /**
   * @brief Get the advertising set ID (SID) for extended advertising.
   * @return The SID value (0-15), or 0xFF if not available.
   */
  uint8_t getAdvSID() const;

  /**
   * @brief Get the periodic advertising interval.
   * @return Interval in units of 1.25 ms, or 0 if not a periodic advertisement.
   */
  uint16_t getPeriodicInterval() const;

  /**
   * @brief Beacon/frame type detected in the advertisement payload.
   */
  enum FrameType {
    Unknown,
    IBeacon,
    EddystoneUUID,
    EddystoneURL,
    EddystoneTLM
  };

  /**
   * @brief Detect the beacon frame type of this advertisement.
   * @return The detected FrameType, or Unknown if no known beacon format matches.
   */
  FrameType getFrameType() const;

  /**
   * @brief Get a human-readable representation of this advertised device.
   * @return A String describing the device address, name, RSSI, and services.
   */
  String toString() const;

  /**
   * @brief Merge a SCAN_RSP payload into this device's existing ADV_IND data.
   * @param data Pointer to the scan response payload.
   * @param len  Length of the scan response data in bytes.
   * @param rssi RSSI of the scan response.
   * @note Concatenates the raw bytes, re-parses all AD structures, and updates RSSI.
   */
  void mergeScanResponse(const uint8_t *data, size_t len, int8_t rssi);

  struct Impl;

private:
  explicit BLEAdvertisedDevice(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;
  friend class BLEScan;
};

#include "BLEScanResults.h"

#endif /* BLE_ENABLED */
