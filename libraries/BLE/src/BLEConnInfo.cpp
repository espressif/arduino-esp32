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

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include "BLEConnInfo.h"
#include "BLEAdvTypes.h"
#include <string.h>

#include "impl/common/BLEConnInfoData.h"

BLEConnInfo::Data *BLEConnInfo::data() {
  static_assert(sizeof(Data) <= kStorageSize, "BLEConnInfo::Data exceeds kStorageSize");
  return reinterpret_cast<Data *>(_storage);
}

const BLEConnInfo::Data *BLEConnInfo::data() const {
  return reinterpret_cast<const Data *>(_storage);
}

BLEConnInfo::BLEConnInfo() {
  memset(_storage, 0, sizeof(_storage));
}

uint16_t BLEConnInfo::getHandle() const {
  return _valid ? data()->handle : 0;
}

BTAddress BLEConnInfo::getAddress() const {
  return _valid ? data()->address : BTAddress();
}

BTAddress BLEConnInfo::getIdAddress() const {
  return _valid ? data()->idAddress : BTAddress();
}

uint16_t BLEConnInfo::getMTU() const {
  return _valid ? data()->mtu : 0;
}

bool BLEConnInfo::isEncrypted() const {
  return _valid && data()->encrypted;
}

bool BLEConnInfo::isAuthenticated() const {
  return _valid && data()->authenticated;
}

bool BLEConnInfo::isBonded() const {
  return _valid && data()->bonded;
}

uint8_t BLEConnInfo::getSecurityKeySize() const {
  return _valid ? data()->keySize : 0;
}

uint16_t BLEConnInfo::getInterval() const {
  return _valid ? data()->interval : 0;
}

uint16_t BLEConnInfo::getLatency() const {
  return _valid ? data()->latency : 0;
}

uint16_t BLEConnInfo::getSupervisionTimeout() const {
  return _valid ? data()->supervisionTimeout : 0;
}

BLEPhy BLEConnInfo::getTxPhy() const {
  return _valid ? static_cast<BLEPhy>(data()->txPhy) : BLEPhy::PHY_1M;
}

BLEPhy BLEConnInfo::getRxPhy() const {
  return _valid ? static_cast<BLEPhy>(data()->rxPhy) : BLEPhy::PHY_1M;
}

bool BLEConnInfo::isCentral() const {
  return _valid && data()->central;
}

bool BLEConnInfo::isPeripheral() const {
  return _valid && !data()->central;
}

int8_t BLEConnInfo::getRSSI() const {
  return _valid ? data()->rssi : 0;
}

BLEConnInfo::operator bool() const {
  return _valid;
}

#endif /* BLE_ENABLED */
