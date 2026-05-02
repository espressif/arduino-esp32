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

#include "impl/BLERemoteTypesBackend.h"
#include "impl/BLEImplHelpers.h"

// --------------------------------------------------------------------------
// BLERemoteService common API (stack-agnostic)
// --------------------------------------------------------------------------

/**
 * @brief Construct an invalid (empty) remote service handle.
 * @note The handle must be populated via BLEClient before use.
 */
BLERemoteService::BLERemoteService() : _impl(nullptr) {}

/**
 * @brief Check whether this handle refers to a valid remote service.
 * @return True if the underlying implementation is present, false otherwise.
 */
BLERemoteService::operator bool() const {
  return _impl != nullptr;
}

/**
 * @brief Get the UUID of this service.
 * @return The service UUID, or a default-constructed (empty) BLEUUID if the handle is invalid.
 */
BLEUUID BLERemoteService::getUUID() const {
  return _impl ? _impl->uuid : BLEUUID();
}

/**
 * @brief Get the starting attribute handle of this service.
 * @return The GATT start handle, or 0 if the handle is invalid.
 */
uint16_t BLERemoteService::getHandle() const {
  return _impl ? _impl->startHandle : 0;
}

/**
 * @brief Read a characteristic value by its UUID within this service.
 * @param charUUID UUID of the characteristic to read.
 * @return The characteristic value as a String, or an empty String if the
 *         characteristic is not found or the read fails.
 * @note Performs characteristic discovery via getCharacteristic() before reading.
 *       This is a blocking call that waits for the GATT read response.
 */
String BLERemoteService::getValue(const BLEUUID &charUUID) {
  BLERemoteCharacteristic chr = getCharacteristic(charUUID);
  if (!chr) {
    return "";
  }
  return chr.readValue();
}

/**
 * @brief Write a characteristic value by its UUID within this service.
 * @param charUUID UUID of the characteristic to write.
 * @param value    The value to write.
 * @return BTStatus::NotFound if the characteristic does not exist in this service,
 *         otherwise the status returned by the underlying write operation.
 * @note Performs characteristic discovery via getCharacteristic() before writing.
 */
BTStatus BLERemoteService::setValue(const BLEUUID &charUUID, const String &value) {
  BLERemoteCharacteristic chr = getCharacteristic(charUUID);
  if (!chr) {
    return BTStatus::NotFound;
  }
  return chr.writeValue(value);
}

/**
 * @brief Get a human-readable representation of this service.
 * @return A String describing the service UUID, or a placeholder if the handle is invalid.
 */
String BLERemoteService::toString() const {
  BLE_CHECK_IMPL("BLERemoteService(empty)");
  return "BLERemoteService(uuid=" + impl.uuid.toString() + ")";
}

#endif /* BLE_ENABLED */
