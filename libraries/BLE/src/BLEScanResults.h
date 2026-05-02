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
#include "BLEAdvertisedDevice.h"

class BLEScan;

/**
 * @brief Container for BLE scan results. Supports range-based for loops.
 */
class BLEScanResults {
public:
  /**
   * @brief Get the number of devices in the results.
   * @return The device count.
   */
  size_t getCount() const;

  /**
   * @brief Get a device by index.
   * @param index Zero-based index into the results.
   * @return The advertised device at the given index.
   */
  BLEAdvertisedDevice getDevice(size_t index) const;

  /**
   * @brief Check whether the results contain any devices.
   * @return true if at least one device was found.
   */
  explicit operator bool() const {
    return !_devices.empty();
  }

  /**
   * @brief Print all scan results to the log output.
   */
  void dump() const;

  /**
   * @brief Insert a device or replace an existing entry with the same address.
   * @param device The advertised device to add or update.
   */
  void appendOrReplace(const BLEAdvertisedDevice &device);

  /**
   * @brief Iterator begin for range-based for loops.
   * @return Pointer to the first device.
   */
  const BLEAdvertisedDevice *begin() const;

  /**
   * @brief Iterator end for range-based for loops.
   * @return Pointer past the last device.
   */
  const BLEAdvertisedDevice *end() const;

private:
  std::vector<BLEAdvertisedDevice> _devices;
  friend class BLEScan;
};

#endif /* BLE_ENABLED */
