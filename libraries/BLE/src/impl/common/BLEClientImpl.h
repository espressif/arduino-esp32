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
 * @brief Stack-agnostic shared (common) implementation state for @c BLEClient::Impl. Must stay free of
 *        any NimBLE/Bluedroid or native BT header.
 *
 * The client Impls are more backend-divergent than the server side (native
 * connection-id naming, per-stack sync primitives, discovery caches), so only
 * the genuinely common state lives here; each backend mixin adds the rest.
 */

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include "BLEClient.h"
#include "impl/common/BLEMutex.h"
#include <atomic>
#include <memory>

/**
 * @brief Shared, stack-agnostic state for a GATT client.
 */
struct BLEClientImplCommon : std::enable_shared_from_this<BLEClient::Impl> {
  BTAddress peerAddress;
  // Set on the BLE event task (connect/disconnect callbacks) and read on the
  // user task (isConnected), so it is atomic for well-defined concurrent access.
  std::atomic<bool> connected{false};
  SemaphoreHandle_t mtx = xSemaphoreCreateRecursiveMutex();

  BLEClient::ConnectHandler onConnectCb = nullptr;
  BLEClient::DisconnectHandler onDisconnectCb = nullptr;
  BLEClient::ConnectFailHandler onConnectFailCb = nullptr;
  BLEClient::MtuChangedHandler onMtuChangedCb = nullptr;
  BLEClient::ConnParamsReqHandler onConnParamsReqCb = nullptr;
  BLEClient::IdentityHandler onIdentityCb = nullptr;

  ~BLEClientImplCommon() {
    if (mtx) {
      vSemaphoreDelete(mtx);
    }
  }

  // Handle factory for backend code that holds a raw Impl* (e.g. a GAP callback's
  // void* arg) and is not a friend of BLEClient: rebuilds the owning shared_ptr via
  // shared_from_this() and mints the handle through the private ctor, which only this
  // befriended ImplCommon can call. Keeps the friend surface to one type per class.
  // See DESIGN.md "Handle construction: makeHandle vs. the private constructor".
  static BLEClient makeHandle(BLEClient::Impl *impl);

  // Stack-agnostic callback dispatch: each copies the handler under
  // mtx, then invokes it outside the lock so user code can safely re-enter the
  // BLE API without deadlocking. Shared by both backends. The NimBLE-only
  // connection-parameter *request* hook (which returns accept/reject) stays in
  // NimBLEClient.cpp because Bluedroid has no equivalent GAP event.

  /**
   * @brief Notify the connect callback (if set) for a new connection.
   * @param connInfo Snapshot of the newly established connection.
   */
  void dispatchConnect(const BLEConnInfo &connInfo);

  /**
   * @brief Notify the disconnect callback (if set).
   * @param connInfo Connection details at the time of disconnect.
   * @param reason Bluetooth disconnect reason code.
   */
  void dispatchDisconnect(const BLEConnInfo &connInfo, uint8_t reason);

  /**
   * @brief Notify the connect-fail callback (if set).
   * @param reason Host/GAP failure code for the failed connect attempt.
   */
  void dispatchConnectFail(int reason);

  /**
   * @brief Notify the MTU-changed callback (if set).
   * @param connInfo Connection whose ATT MTU changed.
   * @param mtu Negotiated ATT MTU in octets.
   */
  void dispatchMtuChanged(const BLEConnInfo &connInfo, uint16_t mtu);

  /**
   * @brief Notify the identity callback (if set) for a connection, backend-agnostic.
   * @param connInfo Connection whose peer identity was resolved / became known.
   * @note The handler is copied under @c mtx, then invoked outside the lock to avoid
   *       deadlocks with user code that re-enters the client. Shared by both backends
   *       for onIdentity parity.
   */
  void dispatchIdentityResolved(const BLEConnInfo &connInfo);
};

#endif /* BLE_ENABLED */
