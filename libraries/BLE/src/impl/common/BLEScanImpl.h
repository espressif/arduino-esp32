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

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include "BLEAdvertisedDevice.h"
#include "BLEScan.h"
#include <memory>

/**
 * @brief Backend-agnostic privileged helpers for scan result assembly.
 *
 * Backend scan parsers (@c BLEScan::Impl, in impl/<stack>/) build
 * @c BLEAdvertisedDevice handles and merge scan-response payloads into the result
 * list, both of which touch private members of @c BLEAdvertisedDevice and
 * @c BLEScan::Results. @c BLEScan::Impl is a nested member of @c BLEScan, but C++ does not
 * grant a nested class the enclosing class's friendships, so the parsers cannot reach those
 * privates directly. This friended, agnostically-named helper (mirroring the
 * @c *ImplCommon::makeHandle pattern) provides that access with no backend type in the public headers.
 */
struct BLEScanImplCommon {
  /// Mint a public advertised-device handle from its implementation.
  static BLEAdvertisedDevice makeAdvertisedDeviceHandle(std::shared_ptr<BLEAdvertisedDevice::Impl> impl) {
    return BLEAdvertisedDevice(std::move(impl));
  }

  /**
   * @brief Merge scan-response bytes into an existing result matched by (address, SID).
   * @param results  Result list to search/update in place.
   * @param addr     Advertiser address to match.
   * @param sid      Advertising SID (ADI) of the scan response; 0xFF for legacy PDUs.
   * @param data     Scan-response AD payload.
   * @param len      Payload length in bytes.
   * @param rssi     RSSI reported with the scan response.
   * @param merged   Set to the updated device when a match is found.
   * @return true if a matching device was found and merged; false otherwise.
   *
   * The scan response is matched to the advertising set that solicited it, not
   * merely the address: a single advertiser may run several sets (see
   * BLEScan::Results::appendOrReplace), and each set's response carries its own
   * SID. Legacy responses report SID 0xFF and merge into the legacy entry.
   */
  static bool mergeScanResponse(
    BLEScan::Results &results, const BTAddress &addr, uint8_t sid, const uint8_t *data, size_t len, int8_t rssi, BLEAdvertisedDevice &merged
  ) {
    for (auto &d : results._devices) {
      if (d.getAddress() == addr && d.getAdvSID() == sid) {
        d.mergeScanResponse(data, len, rssi);
        merged = d;
        return true;
      }
    }
    return false;
  }
};

#endif /* BLE_ENABLED */
