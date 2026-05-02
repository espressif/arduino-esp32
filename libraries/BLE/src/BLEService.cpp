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

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include "impl/BLEServiceBackend.h"
#include "impl/BLEServerBackend.h"
#include "impl/BLECharacteristicBackend.h"
#include "impl/BLECharacteristicValidation.h"
#include "impl/BLEImplHelpers.h"
#include "esp32-hal-log.h"

// --------------------------------------------------------------------------
// BLEService common API (stack-agnostic)
// --------------------------------------------------------------------------

/**
 * @brief Construct a default (invalid) BLEService handle.
 * @note The handle must be initialized via BLEServer::createService() before use.
 */
BLEService::BLEService() : _impl(nullptr) {}

/**
 * @brief Check whether this handle references a valid service implementation.
 * @return true if the internal Impl pointer is non-null; false otherwise.
 */
BLEService::operator bool() const {
  return _impl != nullptr;
}

/**
 * @brief Declare another service as included by this service.
 * @param service The service to include.
 * @note The included service must be created on the same server. The include
 *       declaration is registered when the server is started.
 */
void BLEService::addIncludedService(const BLEService &service) {
  if (!_impl || !service._impl) {
    return;
  }
  for (auto &inc : _impl->includedServices) {
    if (inc == service._impl) {
      return;
    }
  }
  log_d("Service %s: addIncludedService %s", _impl->uuid.toString().c_str(), service._impl->uuid.toString().c_str());
  _impl->includedServices.push_back(service._impl);
}

/**
 * @brief Look up a characteristic in this service by UUID.
 * @param uuid UUID of the characteristic to find.
 * @return The first matching BLECharacteristic handle, or an invalid handle if not found.
 * @note Performs a linear search over the characteristic list.
 */
BLECharacteristic BLEService::getCharacteristic(const BLEUUID &uuid) {
  BLE_CHECK_IMPL(BLECharacteristic());
  for (auto &chr : impl.characteristics) {
    if (chr->uuid == uuid) {
      return BLECharacteristic(chr);
    }
  }
  log_d("Service %s: getCharacteristic %s - not found", impl.uuid.toString().c_str(), uuid.toString().c_str());
  return BLECharacteristic();
}

/**
 * @brief Get all characteristics belonging to this service.
 * @return A vector of BLECharacteristic handles (shared-ownership copies).
 */
std::vector<BLECharacteristic> BLEService::getCharacteristics() const {
  std::vector<BLECharacteristic> result;
  BLE_CHECK_IMPL(result);
  result.reserve(impl.characteristics.size());
  for (auto &chr : impl.characteristics) {
    result.push_back(BLECharacteristic(chr));
  }
  return result;
}

/**
 * @brief Remove a characteristic from this service.
 * @param chr The characteristic to remove.
 * @note No-ops silently if either this service handle or the characteristic
 *       handle is invalid. Only the first matching shared_ptr is erased.
 */
void BLEService::removeCharacteristic(const BLECharacteristic &chr) {
  if (!_impl || !chr._impl) {
    return;
  }
  auto &chars = _impl->characteristics;
  for (auto it = chars.begin(); it != chars.end(); ++it) {
    if (*it == chr._impl) {
      log_d("Service %s: removeCharacteristic %s", _impl->uuid.toString().c_str(), chr.getUUID().toString().c_str());
      chars.erase(it);
      break;
    }
  }
}

/**
 * @brief Check whether this service has been started.
 * @return true after the service is registered in the GATT database; false otherwise.
 */
bool BLEService::isStarted() const {
  return _impl && _impl->started;
}

/**
 * @brief Get the UUID of this service.
 * @return The service UUID, or a default-constructed BLEUUID if the handle is invalid.
 */
BLEUUID BLEService::getUUID() const {
  return _impl ? _impl->uuid : BLEUUID();
}

/**
 * @brief Get the attribute handle assigned to this service.
 * @return The GATT attribute handle, or 0 if the handle is invalid.
 */
uint16_t BLEService::getHandle() const {
  return _impl ? _impl->handle : 0;
}

/**
 * @brief Get the server that owns this service.
 * @return A BLEServer handle backed by the parent server's shared_ptr, or an invalid
 *         handle if this service is not attached to a server.
 */
BLEServer BLEService::getServer() const {
  return _impl && _impl->server ? BLEServer(_impl->server->shared_from_this()) : BLEServer();
}

/**
 * @brief Add a characteristic to this service.
 * @param uuid        UUID of the characteristic to create.
 * @param properties  Characteristic properties (read, write, notify, etc.).
 * @param permissions ATT permission flags controlling access security.
 * @return A BLECharacteristic handle, or an invalid handle on validation failure.
 * @note Multiple characteristics with the same UUID are allowed per the GATT
 *       spec (Core Spec Vol 3, Part G, §3.3) and are differentiated by handle.
 *       HID Report characteristics (all UUID 0x2A4D) require this.
 *       Property/permission combinations are validated at construction time via
 *       bleValidateCharProps(); hard errors (e.g. empty properties, mismatched
 *       read/write permissions) cause the call to return an invalid handle.
 */
BLECharacteristic BLEService::createCharacteristic(const BLEUUID &uuid, BLEProperty properties, BLEPermission permissions) {
  BLE_CHECK_IMPL(BLECharacteristic());

  log_d(
    "Service %s: createCharacteristic %s props=0x%02x perms=0x%04x", impl.uuid.toString().c_str(), uuid.toString().c_str(), static_cast<uint8_t>(properties),
    static_cast<uint16_t>(permissions)
  );

  // Construction-time validation. Rejects hard errors (empty props, bad
  // permission/property combinations) before the characteristic enters the
  // service. Permissions are taken at face value — there is no auto-fill —
  // so rule 3.1 (open access) and related warnings can fire honestly.
  if (!bleValidateCharProps(uuid, properties, permissions)) {
    log_e("Service %s: createCharacteristic %s rejected by validation", impl.uuid.toString().c_str(), uuid.toString().c_str());
    return BLECharacteristic();
  }

  auto chr = std::make_shared<BLECharacteristic::Impl>();
  chr->uuid = uuid;
  chr->properties = properties;
  chr->service = _impl.get();
  chr->permissions = permissions;

  impl.characteristics.push_back(chr);
  return BLECharacteristic(chr);
}

#endif /* BLE_ENABLED */
