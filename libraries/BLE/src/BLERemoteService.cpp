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

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include "impl/BLEBackend.h"
#include "impl/common/BLEImplHelpers.h"

BLERemoteService::BLERemoteService() : _impl(nullptr) {}

BLERemoteService::operator bool() const {
  return _impl != nullptr;
}

BLEUUID BLERemoteService::getUUID() const {
  return _impl ? _impl->uuid : BLEUUID();
}

uint16_t BLERemoteService::getHandle() const {
  return _impl ? _impl->startHandle : 0;
}

BLEClient BLERemoteService::getClient() const {
  // makeHandle rebuilds the owning shared_ptr from the raw BLEClient::Impl* via
  // shared_from_this(); the client is owned by whoever established the connection.
  return (_impl && _impl->client) ? BLEClient::Impl::makeHandle(_impl->client) : BLEClient();
}

String BLERemoteService::getValue(const BLEUUID &charUUID) {
  BLERemoteCharacteristic chr = getCharacteristic(charUUID);
  if (!chr) {
    return "";
  }
  return chr.readValue();
}

BTStatus BLERemoteService::setValue(const BLEUUID &charUUID, const String &value) {
  BLERemoteCharacteristic chr = getCharacteristic(charUUID);
  if (!chr) {
    return BTStatus::NotFound;
  }
  return chr.writeValue(value);
}

String BLERemoteService::toString() const {
  BLE_CHECK_IMPL("BLERemoteService(empty)");
  return "BLERemoteService(uuid=" + impl.uuid.toString() + ")";
}

#endif /* BLE_ENABLED */
