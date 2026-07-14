/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
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

#include "BLEAdvertisedDevice.h"

struct ServiceDataEntry {
  BLEUUID uuid;
  std::vector<uint8_t> data;
};

struct BLEAdvertisedDeviceImplCommon {
  BTAddress address;
  BTAddress::Type addrType = BTAddress::Type::Public;
  String name;
  int8_t rssi = -128;
  int8_t txPower = -128;
  uint16_t appearance = 0;
  BLEAdvType advType = BLEAdvType::ConnectableScannable;

  std::vector<uint8_t> mfgData;
  std::vector<BLEUUID> serviceUUIDs;
  std::vector<ServiceDataEntry> serviceData;
  std::vector<uint8_t> payload;

  bool hasName = false;
  bool hasRSSI = false;
  bool hasTXPower = false;
  bool hasAppearance = false;
  bool hasMfgData = false;
  bool connectable = false;
  bool scannable = false;
  bool directed = false;
  bool legacy = true;

  BLEPhy primaryPhy = BLEPhy::PHY_1M;
  BLEPhy secondaryPhy = BLEPhy::PHY_1M;
  uint8_t sid = 0xFF;
  uint16_t periodicInterval = 0;

  /**
   * @brief Parse a raw BLE advertisement payload into the structured fields above.
   * @param data Pointer to the raw AD structure bytes.
   * @param len  Total length of the payload in bytes.
   * @note Iterates AD structures (length-type-value) populating name, UUIDs, service
   *       data, manufacturer data, TX power, and appearance. 128-bit UUIDs are
   *       byte-reversed from the over-the-air little-endian form to the BLEUUID
   *       big-endian internal representation.
   */
  void parsePayload(const uint8_t *data, size_t len);

  /**
   * @brief Reset all parsed advertisement fields to their default (empty) state.
   * @note Called before re-parsing when merging scan response data.
   */
  void clearParsedFields();
};

// BLEAdvertisedDevice is fully stack-agnostic: the neutral combiner just adopts
// the shared base so all state/behaviour is disclosed as common by its type.
struct BLEAdvertisedDevice::Impl : BLEAdvertisedDeviceImplCommon {};
