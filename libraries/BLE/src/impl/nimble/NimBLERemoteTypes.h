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

#pragma once

#include "impl/BLEGuards.h"
#if BLE_NIMBLE

#include "NimBLEClient.h"
#include "BLERemoteService.h"
#include "BLERemoteCharacteristic.h"
#include "BLERemoteDescriptor.h"
#include "impl/BLESync.h"

#include <host/ble_hs.h>
#include <host/ble_gatt.h>
#include <host/ble_att.h>
#include <vector>
#include <memory>

/**
 * @brief Map a NimBLE GATT UUID to a public @ref BLEUUID.
 * @param u  Host-side UUID as returned by discovery or attribute reads.
 * @return   Public UUID value suitable for application comparisons.
 */
BLEUUID nimbleUuidToPublic(const ble_uuid_any_t &u);
/**
 * @brief Write a @ref BLEUUID into NimBLE's @c ble_uuid_any_t form.
 * @param uuid  Public UUID to convert.
 * @param out   Output written for NimBLE GATT calls.
 */
void publicUuidToNimble(const BLEUUID &uuid, ble_uuid_any_t &out);

/**
 * @brief Return whether @p connHandle still refers to an active GAP link.
 * @param connHandle  ACL connection handle from the stack.
 * @return            true if the link exists, false on invalid or disconnected.
 */
inline bool isGattConnected(uint16_t connHandle) {
  struct ble_gap_conn_desc desc;
  return connHandle != BLE_HS_CONN_HANDLE_NONE && ble_gap_conn_find(connHandle, &desc) == 0;
}

struct BLERemoteService::Impl : std::enable_shared_from_this<BLERemoteService::Impl> {
  BLEUUID uuid;
  uint16_t startHandle = 0;
  uint16_t endHandle = 0;
  uint16_t connHandle = BLE_HS_CONN_HANDLE_NONE;
  BLEClient::Impl *client = nullptr;

  std::vector<std::shared_ptr<BLERemoteCharacteristic::Impl>> characteristics;
  bool charsDiscovered = false;
  BLESync chrDiscoverSync;

  static int chrDiscoveryCb(uint16_t connHandle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg);
};

struct BLERemoteCharacteristic::Impl : std::enable_shared_from_this<BLERemoteCharacteristic::Impl> {
  BLEUUID uuid;
  uint16_t defHandle = 0;
  uint16_t valHandle = 0;
  uint8_t properties = 0;
  uint16_t connHandle = BLE_HS_CONN_HANDLE_NONE;
  BLERemoteService::Impl *service = nullptr;

  std::vector<uint8_t> lastValue;
  int lastReadRC = 0;
  int lastWriteRC = 0;
  BLESync readSync;
  BLESync writeSync;
  BLERemoteCharacteristic::NotifyCallback notifyCb = nullptr;

  std::vector<std::shared_ptr<BLERemoteDescriptor::Impl>> descriptors;
  bool descsDiscovered = false;
  BLESync dscDiscoverSync;

  static int readCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg);
  static int writeCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg);
  static int dscDiscoveryCb(uint16_t connHandle, const struct ble_gatt_error *error, uint16_t chrHandle, const struct ble_gatt_dsc *dsc, void *arg);
  static int notifyCb_static(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg);

  static void registerForNotify(uint16_t connHandle, uint16_t attrHandle, const std::shared_ptr<BLERemoteCharacteristic::Impl> &impl);
  static void unregisterForNotify(uint16_t connHandle, uint16_t attrHandle);
  static void handleNotifyRx(uint16_t connHandle, uint16_t attrHandle, struct os_mbuf *om, bool isNotify);
};

struct BLERemoteDescriptor::Impl {
  BLEUUID uuid;
  uint16_t handle = 0;
  uint16_t connHandle = BLE_HS_CONN_HANDLE_NONE;
  BLERemoteCharacteristic::Impl *chr = nullptr;

  std::vector<uint8_t> lastValue;
  BLESync readSync;
  BLESync writeSync;

  static int readCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg);
  static int writeCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg);
};

#endif /* BLE_NIMBLE */
