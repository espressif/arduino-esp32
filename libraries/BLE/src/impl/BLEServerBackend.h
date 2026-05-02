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

#pragma once

#include "impl/BLEGuards.h"
#include <memory>

#if BLE_NIMBLE
#include "impl/nimble/NimBLEServer.h"
#elif BLE_BLUEDROID
#include "impl/bluedroid/BluedroidServer.h"
#endif

#if BLE_GATT_SERVER_SUPPORTED
namespace ble_server_dispatch {
/**
 * @brief Invoke the server @c onConnect callback outside the object mutex.
 * @param impl      Server implementation receiving the new central connection.
 * @param connInfo  Stack-agnostic link snapshot for the connection.
 */
void dispatchConnect(BLEServer::Impl *impl, const BLEConnInfo &connInfo);
/**
 * @brief Invoke the server @c onDisconnect callback outside the object mutex.
 * @param impl      Server implementation whose link dropped.
 * @param connInfo  Link snapshot at disconnect time.
 * @param reason    Host/GAP disconnect reason code.
 */
void dispatchDisconnect(BLEServer::Impl *impl, const BLEConnInfo &connInfo, uint8_t reason);
/**
 * @brief Invoke the server @c onMtuChanged callback outside the object mutex.
 * @param impl      Server implementation for the affected link.
 * @param connInfo  Link metadata after the MTU exchange.
 * @param mtu       Negotiated ATT MTU for the connection.
 */
void dispatchMtuChanged(BLEServer::Impl *impl, const BLEConnInfo &connInfo, uint16_t mtu);
/**
 * @brief Invoke the server @c onConnParamsUpdate callback outside the object mutex.
 * @param impl      Server implementation for the link whose parameters changed.
 * @param connInfo  Link snapshot after the connection update.
 */
void dispatchConnParamsUpdate(BLEServer::Impl *impl, const BLEConnInfo &connInfo);
}  // namespace ble_server_dispatch

/**
 * @brief Unregister a GATT service from the native stack.
 * @param impl  Server implementation that owns the GATT database.
 * @param svc   Service implementation to remove.
 * @return      @c BTStatus::OK on success, or an error if removal/rebuild failed.
 * @note        Bluedroid deletes the service in place; NimBLE may rebuild the full GATT table.
 */
BTStatus bleServerRemoveService(BLEServer::Impl *impl, std::shared_ptr<BLEService::Impl> svc);
#endif
