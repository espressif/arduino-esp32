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

#include "impl/common/BLEGuards.h"
#if BLE_NIMBLE

#include "NimBLEClient.h"
#include "BLERemoteService.h"
#include "BLERemoteCharacteristic.h"
#include "impl/common/BLERemoteGattImpl.h"
#include "BLERemoteDescriptor.h"
#include "impl/common/BLESync.h"

#include <host/ble_hs.h>
#include <host/ble_gatt.h>
#include <host/ble_att.h>
#include <vector>
#include <memory>

/**
 * @brief Return whether @p connHandle still refers to an active GAP link.
 * @param connHandle  ACL connection handle from the stack.
 * @return            true if the link exists, false on invalid or disconnected.
 */
inline bool nimbleIsGattConnected(uint16_t connHandle) {
  struct ble_gap_conn_desc desc;
  return connHandle != BLE_HS_CONN_HANDLE_NONE && ble_gap_conn_find(connHandle, &desc) == 0;
}

/**
 * @brief NimBLE remote-service implementation (@c BLERemoteService::Impl).
 *
 * Defined in @c impl/nimble/, so everything here is NimBLE-specific; it inherits the
 * stack-agnostic @c BLERemoteServiceImplCommon, giving one uniform @c impl.member type.
 */
struct BLERemoteService::Impl : BLERemoteServiceImplCommon {
  uint16_t connHandle = BLE_HS_CONN_HANDLE_NONE;
  BLESync chrDiscoverSync;

  static int chrDiscoveryCb(uint16_t connHandle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg);
};

/**
 * @brief NimBLE remote-characteristic implementation (@c BLERemoteCharacteristic::Impl).
 *
 * Defined in @c impl/nimble/, so everything here is NimBLE-specific; it inherits the
 * stack-agnostic @c BLERemoteCharacteristicImplCommon, giving one uniform @c impl.member type.
 * The GATT-client callback statics reach per-connection state through the passed impl pointer or
 * the connHandle+attrHandle lookup.
 */
struct BLERemoteCharacteristic::Impl : BLERemoteCharacteristicImplCommon {
  uint16_t connHandle = BLE_HS_CONN_HANDLE_NONE;
  // Last ATT error from the most recent read or write GATT callback (read and
  // write are sequential blocking ops, so a single field is sufficient).
  int lastAttError = 0;
  BLESync readSync;
  BLESync writeSync;
  BLESync dscDiscoverSync;

  static int readCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg);
  static int writeCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg);
  static int dscDiscoveryCb(uint16_t connHandle, const struct ble_gatt_error *error, uint16_t chrHandle, const struct ble_gatt_dsc *dsc, void *arg);
  static int onNotifyCb_static(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg);

  static void registerForNotify(uint16_t connHandle, uint16_t attrHandle, const std::shared_ptr<BLERemoteCharacteristic::Impl> &impl);
  static void unregisterForNotify(uint16_t connHandle, uint16_t attrHandle);
  static void handleNotifyRx(uint16_t connHandle, uint16_t attrHandle, struct os_mbuf *om, bool isNotify);
};

/**
 * @brief NimBLE remote-descriptor implementation (@c BLERemoteDescriptor::Impl).
 *
 * Defined in @c impl/nimble/, so everything here is NimBLE-specific; it inherits the
 * stack-agnostic @c BLERemoteDescriptorImplCommon, giving one uniform @c impl.member type.
 */
struct BLERemoteDescriptor::Impl : BLERemoteDescriptorImplCommon {
  uint16_t connHandle = BLE_HS_CONN_HANDLE_NONE;
  BLESync readSync;
  BLESync writeSync;

  static int readCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg);
  static int writeCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg);
};

#endif /* BLE_NIMBLE */
