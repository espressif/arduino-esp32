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

/**
 * @file
 * @brief Shared (stack-agnostic) implementation-layer definitions for
 *        @c BLEAdvertisedDevice::Impl. Public @c BLEAdvertisedDevice:: methods
 *        live in src/BLEAdvertisedDevice.cpp; this file holds the Impl helpers
 *        that backend scan parsers call. API contract is documented on the
 *        declarations in impl/common/BLEAdvertisedDeviceImpl.h.
 */

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include "impl/common/BLEAdvertisedDeviceImpl.h"

void BLEAdvertisedDeviceImplCommon::parsePayload(const uint8_t *data, size_t len) {
  payload.assign(data, data + len);
  size_t pos = 0;
  while (pos < len) {
    uint8_t adLen = data[pos];
    if (adLen == 0 || pos + 1 + adLen > len) {
      break;
    }
    uint8_t adType = data[pos + 1];
    const uint8_t *adData = data + pos + 2;
    uint8_t dataLen = adLen - 1;

    switch (adType) {
      case 0x01:  // Flags
        break;
      case 0x02:  // Incomplete 16-bit UUIDs
      case 0x03:  // Complete 16-bit UUIDs
        for (size_t i = 0; i + 1 < dataLen; i += 2) {
          uint16_t uuid16 = adData[i] | (adData[i + 1] << 8);
          serviceUUIDs.push_back(BLEUUID(uuid16));
        }
        break;
      case 0x04:  // Incomplete 32-bit UUIDs
      case 0x05:  // Complete 32-bit UUIDs
        for (size_t i = 0; i + 3 < dataLen; i += 4) {
          uint32_t uuid32 = adData[i] | (adData[i + 1] << 8) | (adData[i + 2] << 16) | (adData[i + 3] << 24);
          serviceUUIDs.push_back(BLEUUID(uuid32));
        }
        break;
      case 0x06:  // Incomplete 128-bit UUIDs
      case 0x07:  // Complete 128-bit UUIDs
        for (size_t i = 0; i + 15 < dataLen; i += 16) {
          serviceUUIDs.push_back(BLEUUID(adData + i, 16, true));
        }
        break;
      case 0x08:  // Shortened Local Name
      case 0x09:  // Complete Local Name
        name = String(reinterpret_cast<const char *>(adData), dataLen);
        hasName = true;
        break;
      case 0x0A:  // TX Power Level
        if (dataLen >= 1) {
          txPower = static_cast<int8_t>(adData[0]);
          hasTXPower = true;
        }
        break;
      case 0x19:  // Appearance
        if (dataLen >= 2) {
          appearance = adData[0] | (adData[1] << 8);
          hasAppearance = true;
        }
        break;
      case 0x14:  // 16-bit Service Solicitation UUIDs
      case 0x15:  // 128-bit Service Solicitation UUIDs
        break;
      case 0x16:  // Service Data - 16-bit UUID
        if (dataLen >= 2) {
          ServiceDataEntry entry;
          entry.uuid = BLEUUID(static_cast<uint16_t>(adData[0] | (adData[1] << 8)));
          if (dataLen > 2) {
            entry.data.assign(adData + 2, adData + dataLen);
          }
          serviceData.push_back(std::move(entry));
        }
        break;
      case 0x20:  // Service Data - 32-bit UUID
        if (dataLen >= 4) {
          ServiceDataEntry entry;
          entry.uuid = BLEUUID(static_cast<uint32_t>(adData[0] | (adData[1] << 8) | (adData[2] << 16) | (adData[3] << 24)));
          if (dataLen > 4) {
            entry.data.assign(adData + 4, adData + dataLen);
          }
          serviceData.push_back(std::move(entry));
        }
        break;
      case 0x21:  // Service Data - 128-bit UUID
        if (dataLen >= 16) {
          ServiceDataEntry entry;
          entry.uuid = BLEUUID(adData, 16, true);
          if (dataLen > 16) {
            entry.data.assign(adData + 16, adData + dataLen);
          }
          serviceData.push_back(std::move(entry));
        }
        break;
      case 0xFF:  // Manufacturer Specific Data
        mfgData.assign(adData, adData + dataLen);
        hasMfgData = true;
        break;
      default: break;
    }
    pos += 1 + adLen;
  }
}

void BLEAdvertisedDeviceImplCommon::clearParsedFields() {
  name = "";
  txPower = -128;
  appearance = 0;
  mfgData.clear();
  serviceUUIDs.clear();
  serviceData.clear();
  hasName = false;
  hasTXPower = false;
  hasAppearance = false;
  hasMfgData = false;
}

#endif /* BLE_ENABLED */
