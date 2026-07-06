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

/**
 * @file BluedroidConnInfo.cpp
 * @brief Builds and updates the stack-agnostic BLEConnInfo from Bluedroid GAP/GATT event data.
 */

#include "impl/common/BLEGuards.h"
#if BLE_BLUEDROID

#include "BluedroidConnInfo.h"

#include <esp_gap_ble_api.h>

namespace {

// auth_mode is an esp_ble_auth_req_t bitmask; MITM (authenticated) and BOND are
// independent bits — decode separately to match NimBLE's sec_state fields.
bool authModeIsAuthenticated(uint8_t authMode) {
  return (authMode & ESP_LE_AUTH_REQ_MITM) != 0;
}

bool authModeIsBonded(uint8_t authMode) {
  return (authMode & ESP_LE_AUTH_BOND) != 0;
}

}  // namespace

BLEConnInfo BLEConnInfoImpl::make(uint16_t connId, const uint8_t bda[6], uint16_t mtu, bool central, BTAddress::Type addrType) {
  BLEConnInfo info;
  info._valid = true;
  auto *d = info.data();
  d->handle = connId;
  d->address = BTAddress(bda, addrType);
  d->idAddress = d->address;
  d->mtu = mtu;
  d->central = central;
  d->encrypted = false;
  d->authenticated = false;
  d->bonded = false;
  d->keySize = 0;
  d->interval = 0;
  d->latency = 0;
  d->supervisionTimeout = 0;
  d->txPhy = 1;
  d->rxPhy = 1;
  d->rssi = 0;
  return info;
}

BLEConnInfo BLEConnInfoImpl::make(const uint8_t bda[6], BTAddress::Type addrType) {
  return make(0, bda, 23, false, addrType);
}

void BLEConnInfoImpl::setMTU(BLEConnInfo &info, uint16_t mtu) {
  if (info) {
    info.data()->mtu = mtu;
  }
}

void BLEConnInfoImpl::setConnParams(BLEConnInfo &info, uint16_t interval, uint16_t latency, uint16_t timeout) {
  if (!info) {
    return;
  }
  auto *d = info.data();
  d->interval = interval;
  d->latency = latency;
  d->supervisionTimeout = timeout;
}

void BLEConnInfoImpl::updateSecurityFlags(BLEConnInfo &info, bool encrypted, bool authenticated, bool bonded) {
  if (!info) {
    return;
  }
  auto *d = info.data();
  d->encrypted = encrypted;
  d->authenticated = authenticated;
  d->bonded = bonded;
}

void BLEConnInfoImpl::updateSecurityFromAuthComplete(BLEConnInfo &info, uint8_t authMode) {
  updateSecurityFlags(info, true, authModeIsAuthenticated(authMode), authModeIsBonded(authMode));
}

void BLEConnInfoImpl::setAddressType(BLEConnInfo &info, BTAddress::Type addrType) {
  if (!info) {
    return;
  }
  auto *d = info.data();
  d->address = BTAddress(d->address.data(), addrType);
  d->idAddress = d->address;
}

void BLEConnInfoImpl::setPhy(BLEConnInfo &info, uint8_t txPhy, uint8_t rxPhy) {
  if (!info) {
    return;
  }
  auto *d = info.data();
  d->txPhy = txPhy;
  d->rxPhy = rxPhy;
}

#endif /* BLE_BLUEDROID */
