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

/**
 * @file
 * @brief Stack-agnostic shared (common) implementation state for @c BLEServer::Impl.
 *
 * Holds every member that is identical across NimBLE and Bluedroid. The active
 * backend defines @c BLEServer::Impl in @c impl/<stack>/ inheriting this base,
 * adding only the stack-specific state. This header must stay stack-agnostic: it
 * may not include any NimBLE/Bluedroid or native BT header.
 */

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include "BLEServer.h"
#include "impl/common/BLEMutex.h"
#include <vector>
#include <memory>
#include <utility>

/**
 * @brief Shared, stack-agnostic state and helpers for a GATT server.
 *
 * Dispatch helpers and connection bookkeeping live here so both backends share
 * one implementation (items 4, 15). @c BLEServer::Impl inherits this base, so
 * backend code accesses these members cast-free as @c impl.member.
 */
struct BLEServerImplCommon : std::enable_shared_from_this<BLEServer::Impl> {
  bool started = false;
  bool advertiseOnDisconnect = true;

  std::vector<std::shared_ptr<BLEService::Impl>> services;

  BLEServer::ConnectHandler onConnectCb = nullptr;
  BLEServer::DisconnectHandler onDisconnectCb = nullptr;
  BLEServer::MtuChangedHandler onMtuChangedCb = nullptr;
  BLEServer::ConnParamsHandler onConnParamsCb = nullptr;
  BLEServer::IdentityHandler onIdentityCb = nullptr;

  std::vector<std::pair<uint16_t, BLEConnInfo>> connections;

  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  ~BLEServerImplCommon() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  // Connection table helpers. Caller must hold mtx.

  /**
   * @brief Insert or update a connection entry, keyed by connection handle.
   * @param connHandle Connection handle used as the lookup key.
   * @param connInfo   Connection information to store.
   * @note Updates in place if connHandle already exists, otherwise appends.
   */
  void connSet(uint16_t connHandle, const BLEConnInfo &connInfo);

  /**
   * @brief Remove a connection entry by handle (no-op if absent).
   * @param connHandle Connection handle to remove.
   */
  void connErase(uint16_t connHandle);

  /**
   * @brief Find a connection entry by handle.
   * @param connHandle Connection handle to look up.
   * @return Pointer to the stored BLEConnInfo, or nullptr if not found.
   * @note The pointer is invalidated by any later mutation of the connection list.
   */
  BLEConnInfo *connFind(uint16_t connHandle);

  // Handle factory for backend code that holds a raw Impl* (e.g. a GAP callback's
  // void* arg) and is not a friend of BLEServer: rebuilds the owning shared_ptr via
  // shared_from_this() and mints the handle through the private ctor, which only this
  // befriended ImplCommon can call. Keeps the friend surface to one type per class.
  // See DESIGN.md "Handle construction: makeHandle vs. the private constructor".
  static BLEServer makeHandle(BLEServer::Impl *impl);

  // Stack-agnostic callback dispatch (copies the handler under mtx, invokes it
  // outside the lock). Shared by both backends.
  void dispatchConnect(const BLEConnInfo &connInfo);
  void dispatchDisconnect(const BLEConnInfo &connInfo, uint8_t reason);
  void dispatchMtuChanged(const BLEConnInfo &connInfo, uint16_t mtu);
  void dispatchConnParamsUpdate(const BLEConnInfo &connInfo);
  void dispatchIdentityResolved(const BLEConnInfo &connInfo);

  // Forward a GAP event to the server's private handler on behalf of backend
  // advertising code, which is not itself a friend of BLEServer. Keeps the
  // friend surface to this one agnostically-named type (see makeHandle above).
  static int forwardGapEvent(BLEServer &server, void *event) {
    return server.handleGapEvent(event);
  }
};

#if BLE_GATT_SERVER_SUPPORTED
/**
 * @brief Unregister a GATT service from the native stack.
 * @param impl  Server implementation that owns the GATT database.
 * @param svc   Service implementation to remove.
 * @return      @c BTStatus::OK on success, or an error if removal/rebuild failed.
 * @note        Bluedroid deletes the service in place; NimBLE may rebuild the full GATT table.
 *
 * Implemented per-backend (NimBLEServer.cpp / BluedroidServer.cpp). Declared here
 * so callers get it alongside the shared server Impl base without a selector header.
 */
BTStatus bleServerRemoveService(BLEServer::Impl *impl, std::shared_ptr<BLEService::Impl> svc);
#endif

#endif /* BLE_ENABLED */
