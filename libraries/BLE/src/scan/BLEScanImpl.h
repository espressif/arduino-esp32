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
#include "core/BLEAdmission.h"
#include "core/BLEMutex.h"
#include "core/BLESync.h"
#include <algorithm>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>

/**
 * @brief Shared, stack-agnostic base for @c BLEScan::Impl plus privileged scan-result helpers.
 */
struct BLEScanImplCommon {
  static constexpr uint8_t kMaxPeriodicAdvSid = 0x0F;
  static constexpr uint16_t kMaxPeriodicSyncSkip = 0x01F3;
  static constexpr uint16_t kMinPeriodicSyncTimeoutMs = 100;

  // 0x30 = 48 units x 0.625 ms = 30 ms (continuous scan by default on both stacks).
  uint16_t interval = 0x30;
  uint16_t window = 0x30;
  bool activeScan = true;
  bool filterDuplicates = true;
  std::atomic<bool> isScanning{false};
  BLEAdmissionGate nativeAdmission;

  BLEScan::Results results;
  std::vector<std::pair<BTAddress, uint8_t>> duplicateKeys;
  BLESync scanSync{"scanSync"};
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  BLEScan::ResultHandler onResultCb = nullptr;
  BLEScan::CompleteHandler onCompleteCb = nullptr;
  BLEScan::PeriodicSyncHandler periodicSyncCb = nullptr;
  BLEScan::PeriodicReportHandler periodicReportCb = nullptr;
  BLEScan::PeriodicLostHandler periodicLostCb = nullptr;

#if BLE_PERIODIC_ADV_SUPPORTED
  std::vector<uint16_t> periodicSyncs;
#endif

  ~BLEScanImplCommon() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  bool duplicateKeySeen(const BTAddress &address, uint8_t sid) const {
    return std::any_of(duplicateKeys.begin(), duplicateKeys.end(), [&](const auto &key) {
      return key.first == address && key.second == sid;
    });
  }

  void markDuplicateKey(const BTAddress &address, uint8_t sid) {
    if (!duplicateKeySeen(address, sid)) {
      duplicateKeys.emplace_back(address, sid);
    }
  }

  static bool periodicSyncParamsValid(uint8_t sid, uint16_t skipCount, uint16_t timeoutMs) {
    return sid <= kMaxPeriodicAdvSid && skipCount <= kMaxPeriodicSyncSkip && timeoutMs >= kMinPeriodicSyncTimeoutMs;
  }

  static bool pastParamsValid(uint16_t skipCount, uint16_t timeoutMs) {
    return skipCount <= kMaxPeriodicSyncSkip && timeoutMs >= kMinPeriodicSyncTimeoutMs;
  }

  /// Mint a public advertised-device handle from its implementation.
  static BLEAdvertisedDevice makeAdvertisedDeviceHandle(std::shared_ptr<BLEAdvertisedDevice::Impl> impl) {
    return BLEAdvertisedDevice(std::move(impl));
  }

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

  static bool preserveScanResponse(const BLEAdvertisedDevice &current, const BLEAdvertisedDevice &cached, BLEAdvertisedDevice &merged) {
    if (!current._impl || !cached._impl || current._impl->advPayloadLen < current._impl->payload.size()
        || cached._impl->advPayloadLen >= cached._impl->payload.size()) {
      return false;
    }
    auto impl = std::make_shared<BLEAdvertisedDevice::Impl>(*current._impl);
    merged = makeAdvertisedDeviceHandle(std::move(impl));
    const size_t offset = cached._impl->advPayloadLen;
    merged.mergeScanResponse(cached._impl->payload.data() + offset, cached._impl->payload.size() - offset, current._impl->rssi);
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
    return std::equal(current._impl->payload.begin(), current._impl->payload.end(), cached._impl->payload.begin());
  }

  /** @brief Whether raw scan-response bytes match the cached response suffix. */
  static bool sameScanResponsePayload(const BLEAdvertisedDevice &cached, const uint8_t *data, size_t len) {
    if (!cached._impl || (!data && len != 0) || cached._impl->advPayloadLen > cached._impl->payload.size()) {
      return false;
    }
    const size_t responseLen = cached._impl->payload.size() - cached._impl->advPayloadLen;
    return responseLen == len && std::equal(cached._impl->payload.begin() + cached._impl->advPayloadLen, cached._impl->payload.end(), data);
  }
};

#endif /* BLE_ENABLED */
