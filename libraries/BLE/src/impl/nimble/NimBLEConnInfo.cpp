/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
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
 * @file NimBLEConnInfo.cpp
 * @brief Bridge from a NimBLE ble_gap_conn_desc to the public BLEConnInfo.
 */

#include "impl/common/BLEGuards.h"
#if BLE_NIMBLE

#include "NimBLEConnInfo.h"
#include "impl/common/BLEConnInfoData.h"
#include <host/ble_att.h>

/**
 * @brief Fills a @ref BLEConnInfo from a NimBLE connection descriptor.
 * @param desc NimBLE GAP connection descriptor.
 * @return Valid @ref BLEConnInfo for the link (MTU, PHY, RSSI when available).
 */
BLEConnInfo BLEConnInfoImpl::fromDesc(const struct ble_gap_conn_desc &desc) {
  BLEConnInfo info;
  info._valid = true;

  auto *d = info.data();
  d->handle = desc.conn_handle;
  d->mtu = ble_att_mtu(desc.conn_handle);
  d->address = BTAddress(desc.peer_ota_addr.val, static_cast<BTAddress::Type>(desc.peer_ota_addr.type));
  d->idAddress = BTAddress(desc.peer_id_addr.val, static_cast<BTAddress::Type>(desc.peer_id_addr.type));
  d->interval = desc.conn_itvl;
  d->latency = desc.conn_latency;
  d->supervisionTimeout = desc.supervision_timeout;
  d->encrypted = desc.sec_state.encrypted;
  d->authenticated = desc.sec_state.authenticated;
  d->bonded = desc.sec_state.bonded;
  d->keySize = desc.sec_state.key_size;
  d->central = (desc.role == BLE_GAP_ROLE_MASTER);
  d->txPhy = 1;
  d->rxPhy = 1;
  d->rssi = 0;

#if BLE5_SUPPORTED
  BLEPhy tx, rx;
  int rc = ble_gap_read_le_phy(desc.conn_handle, reinterpret_cast<uint8_t *>(&tx), reinterpret_cast<uint8_t *>(&rx));
  if (rc == 0) {
    d->txPhy = static_cast<uint8_t>(tx);
    d->rxPhy = static_cast<uint8_t>(rx);
  }
#endif

  int8_t rssi;
  if (ble_gap_conn_rssi(desc.conn_handle, &rssi) == 0) {
    d->rssi = rssi;
  }

  return info;
}

#endif /* BLE_NIMBLE */
