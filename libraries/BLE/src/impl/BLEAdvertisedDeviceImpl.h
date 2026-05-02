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

struct BLEAdvertisedDevice::Impl {
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

  void parsePayload(const uint8_t *data, size_t len);
  void clearParsedFields();
};
