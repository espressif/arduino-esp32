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
#include "impl/common/BLECharacteristicValidation.h"
#include "impl/common/BLEImplHelpers.h"
#include "esp32-hal-log.h"

BLEService::BLEService() : _impl(nullptr) {}

BLEService::operator bool() const {
  return _impl != nullptr;
}

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

std::vector<BLECharacteristic> BLEService::getCharacteristics() const {
  std::vector<BLECharacteristic> result;
  BLE_CHECK_IMPL(result);
  result.reserve(impl.characteristics.size());
  for (auto &chr : impl.characteristics) {
    result.push_back(BLECharacteristic(chr));
  }
  return result;
}

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

bool BLEService::isStarted() const {
  return _impl && _impl->started;
}

BLEUUID BLEService::getUUID() const {
  return _impl ? _impl->uuid : BLEUUID();
}

uint16_t BLEService::getHandle() const {
  return _impl ? _impl->handle : 0;
}

BLEServer BLEService::getServer() const {
  return _impl && _impl->server ? BLEServer(_impl->server->shared_from_this()) : BLEServer();
}

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
