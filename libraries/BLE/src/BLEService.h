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
#include "BLEUUID.h"
#include "BLEProperty.h"
#include <memory>

class BLEServer;
class BLECharacteristic;

/**
 * @brief GATT Service handle.
 *
 * Lightweight shared handle wrapping a BLEService::Impl.
 * Create via server.createService(uuid).
 */
class BLEService {
public:
  BLEService();
  ~BLEService() = default;
  BLEService(const BLEService &) = default;
  BLEService &operator=(const BLEService &) = default;
  BLEService(BLEService &&) = default;
  BLEService &operator=(BLEService &&) = default;

  /**
   * @brief Check whether this handle references a valid service implementation.
   * @return true if backed by a live Impl; false if default-constructed or moved-from.
   */
  explicit operator bool() const;

  /**
   * @brief Add a characteristic to this service.
   *
   * Properties and permissions are declared separately (Bluetooth Core Spec
   * Vol 3 §3.3.1.1 and §3.2.5). Permissions are required — the mapping is
   * fail-closed: a read or write property is only exposed if the matching
   * permission direction is declared. Use a preset from the
   * `BLEPermissions::` namespace for the common cases, e.g.
   * `BLEPermissions::OpenReadWrite` or `BLEPermissions::EncryptedRead`.
   *
   * For notify- or indicate-only characteristics (which don't involve a GATT
   * read/write) pass `BLEPermission::None`.
   *
   * @param uuid        UUID of the characteristic to create.
   * @param properties  Characteristic properties (read, write, notify, etc.).
   * @param permissions ATT permission flags controlling access security.
   * @return A BLECharacteristic handle for the new characteristic.
   */
  BLECharacteristic createCharacteristic(const BLEUUID &uuid, BLEProperty properties, BLEPermission permissions);

  /**
   * @brief Declare another service as included by this service.
   *
   * An Include Declaration (Core Spec Vol 3, Part G, §3.2) creates a GATT
   * attribute that tells clients the included service is logically part of
   * this service. HIDS requires this for the Battery Service.
   *
   * @param service The service to include.
   */
  void addIncludedService(const BLEService &service);

  /**
   * @brief Look up a characteristic in this service by UUID.
   * @param uuid UUID of the characteristic to find.
   * @return The matching BLECharacteristic handle, or an invalid handle if not found.
   */
  BLECharacteristic getCharacteristic(const BLEUUID &uuid);

  /**
   * @brief Get all characteristics belonging to this service.
   * @return A vector of BLECharacteristic handles.
   */
  std::vector<BLECharacteristic> getCharacteristics() const;

  /**
   * @brief Remove a characteristic from this service.
   * @param chr The characteristic to remove.
   */
  void removeCharacteristic(const BLECharacteristic &chr);

  /**
   * @brief Check whether this service has been started.
   * @return true after the service is registered in the GATT database
   *         (via @ref BLEServer::start()); false otherwise.
   */
  bool isStarted() const;

  /**
   * @brief Get the UUID of this service.
   * @return The service UUID.
   */
  BLEUUID getUUID() const;

  /**
   * @brief Get the attribute handle assigned to this service.
   * @return The GATT attribute handle.
   */
  uint16_t getHandle() const;

  /**
   * @brief Get the server that owns this service.
   * @return A BLEServer handle for the parent server.
   */
  BLEServer getServer() const;

  struct Impl;

private:
  explicit BLEService(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;
  friend class BLEServer;
  friend class BLECharacteristic;
};

#endif /* BLE_ENABLED */
