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

#include "core/BLEGuards.h"
#if BLE_ENABLED

#include "scan/BLEAdvertisedDeviceImpl.h"
#include "scan/BLEScan.h"
#include <algorithm>
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

  /**
   * @brief Merge the cached scan-response suffix into a fresh primary report.
   * @param current Fresh primary advertising report.
   * @param cached Previously cached report for the same advertising set.
   * @param merged Receives a deep copy of @p current with the cached scan
   *        response appended.
   * @return true when @p cached contained a merged scan response.
   *
   * The fresh report remains the source of primary payload and metadata
   * (notably RSSI). Only the scan-response suffix is retained.
   */
  static bool preserveScanResponse(
    const BLEAdvertisedDevice &current, const BLEAdvertisedDevice &cached, BLEAdvertisedDevice &merged
  ) {
    if (!current._impl || !cached._impl
        // Some backends deliver a complete fresh ADV+SCAN_RSP report. Its
        // current suffix supersedes the cached response and must not be
        // replaced with stale bytes.
        || current._impl->advPayloadLen < current._impl->payload.size()
        || cached._impl->advPayloadLen >= cached._impl->payload.size()) {
      return false;
    }
    auto impl = std::make_shared<BLEAdvertisedDevice::Impl>(*current._impl);
    merged = makeAdvertisedDeviceHandle(std::move(impl));
    const size_t offset = cached._impl->advPayloadLen;
    merged.mergeScanResponse(
      cached._impl->payload.data() + offset, cached._impl->payload.size() - offset, current._impl->rssi
    );
    return true;
  }

  /** @brief Whether two reports carry the same primary/scan-response split and bytes. */
  static bool samePayload(const BLEAdvertisedDevice &a, const BLEAdvertisedDevice &b) {
    return a._impl && b._impl && a._impl->advPayloadLen == b._impl->advPayloadLen && a._impl->payload == b._impl->payload;
  }

  /** @brief Whether a fresh primary report matches the cached primary bytes. */
  static bool samePrimaryPayload(const BLEAdvertisedDevice &cached, const BLEAdvertisedDevice &current) {
    if (!cached._impl || !current._impl || current._impl->advPayloadLen != current._impl->payload.size()
        || cached._impl->advPayloadLen != current._impl->payload.size()) {
      return false;
    }
    return std::equal(
      current._impl->payload.begin(), current._impl->payload.end(), cached._impl->payload.begin()
    );
  }

  /** @brief Whether raw scan-response bytes match the cached response suffix. */
  static bool sameScanResponsePayload(const BLEAdvertisedDevice &cached, const uint8_t *data, size_t len) {
    if (!cached._impl || (!data && len != 0) || cached._impl->advPayloadLen > cached._impl->payload.size()) {
      return false;
    }
    const size_t responseLen = cached._impl->payload.size() - cached._impl->advPayloadLen;
    return responseLen == len
           && std::equal(
             cached._impl->payload.begin() + cached._impl->advPayloadLen,
             cached._impl->payload.end(),
             data
           );
  }
};

#endif /* BLE_ENABLED */
