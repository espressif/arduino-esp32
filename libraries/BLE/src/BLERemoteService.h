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
#include "BTStatus.h"
#include "BLEUUID.h"
#include <memory>

class BLEClient;
class BLERemoteCharacteristic;

/**
 * @brief Remote GATT service discovered on a peer device.
 *
 * Shared handle. Obtained via BLEClient::getService() or BLEClient::getServices().
 */
class BLERemoteService {
public:
  BLERemoteService();
  ~BLERemoteService() = default;
  BLERemoteService(const BLERemoteService &) = default;
  BLERemoteService &operator=(const BLERemoteService &) = default;
  BLERemoteService(BLERemoteService &&) = default;
  BLERemoteService &operator=(BLERemoteService &&) = default;

  /**
   * @brief Check whether this handle refers to a valid remote service.
   * @return True if the underlying service data is present, false otherwise.
   */
  explicit operator bool() const;

  /**
   * @brief Discover and return a characteristic by UUID.
   * @param uuid UUID of the characteristic to find.
   * @return The matching characteristic, or an invalid handle if not found.
   * @note Triggers GATT characteristic discovery on first call.
   */
  BLERemoteCharacteristic getCharacteristic(const BLEUUID &uuid);

  /**
   * @brief Return all discovered characteristics of this service.
   * @return Vector of characteristic handles (may be empty).
   * @note Triggers GATT characteristic discovery on first call.
   */
  std::vector<BLERemoteCharacteristic> getCharacteristics() const;

  /**
   * @brief Get the client that owns this service.
   * @return The parent BLEClient handle.
   */
  BLEClient getClient() const;

  /**
   * @brief Get the UUID of this service.
   * @return The service UUID.
   */
  BLEUUID getUUID() const;

  /**
   * @brief Get the attribute handle of this service.
   * @return The GATT attribute handle.
   */
  uint16_t getHandle() const;

  /**
   * @brief Shorthand: read a characteristic value by UUID.
   * @param charUUID UUID of the characteristic to read.
   * @return The characteristic value as a String, or an empty String on failure.
   */
  String getValue(const BLEUUID &charUUID);

  /**
   * @brief Shorthand: write a characteristic value by UUID.
   * @param charUUID UUID of the characteristic to write.
   * @param value    The value to write.
   * @return BTStatus indicating success or failure.
   */
  BTStatus setValue(const BLEUUID &charUUID, const String &value);

  /**
   * @brief Get a human-readable representation of this service.
   * @return A String describing the service UUID and handle.
   */
  String toString() const;

  struct Impl;

private:
  explicit BLERemoteService(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;
  friend class BLEClient;
  friend class BLERemoteCharacteristic;
};

#endif /* BLE_ENABLED */
