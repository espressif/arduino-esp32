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

#pragma once

#include "impl/BLEGuards.h"
#if BLE_ENABLED

#include <vector>
#include "BTStatus.h"
#include "BTAddress.h"
#include "BLEUUID.h"
#include "BLEProperty.h"
#include "BLEConnInfo.h"
#include "BLEAdvTypes.h"
#include "BLEService.h"
#include <memory>
#include <functional>

class BLEClass;
class BLECharacteristic;
class BLEDescriptor;
class BLEAdvertising;

/**
 * @brief GATT Server handle.
 *
 * Lightweight shared handle wrapping a BLEServer::Impl. Copying creates
 * shared ownership (refcount increment). Moving transfers ownership.
 *
 * Create via BLE.createServer(). Idempotent -- always returns a handle to
 * the same singleton server.
 *
 * Callback threading: all server callbacks (onConnect, onDisconnect,
 * onMtuChanged, etc.) execute on the BLE stack task — the NimBLE host
 * task or the Bluedroid BTC task, depending on the backend. Callbacks
 * are invoked outside the server mutex, so they may safely call other
 * BLE APIs on the same server, but they must not block for extended
 * periods or perform lengthy I/O.
 */
class BLEServer {
public:
  BLEServer();
  ~BLEServer() = default;
  BLEServer(const BLEServer &) = default;
  BLEServer &operator=(const BLEServer &) = default;
  BLEServer(BLEServer &&) = default;
  BLEServer &operator=(BLEServer &&) = default;

  /**
   * @brief Check whether this handle references a valid server implementation.
   * @return true if backed by a live Impl; false if default-constructed or moved-from.
   */
  explicit operator bool() const;

  /**
   * @brief Callback invoked when a central connects.
   * @param server The server handle that accepted the connection.
   * @param conn   Connection information for the new link.
   */
  using ConnectHandler = std::function<void(BLEServer server, const BLEConnInfo &conn)>;

  /**
   * @brief Callback invoked when a central disconnects.
   * @param server The server handle the central was connected to.
   * @param conn   Connection information for the terminated link.
   * @param reason HCI disconnect reason code (e.g. 0x13 = Remote User Terminated).
   */
  using DisconnectHandler = std::function<void(BLEServer server, const BLEConnInfo &conn, uint8_t reason)>;

  /**
   * @brief Callback invoked when the ATT MTU is negotiated or changed.
   * @param server The server handle.
   * @param conn   Connection information.
   * @param mtu    The newly agreed ATT MTU size in bytes.
   */
  using MtuChangedHandler = std::function<void(BLEServer server, const BLEConnInfo &conn, uint16_t mtu)>;

  /**
   * @brief Callback invoked when connection parameters are updated.
   * @param server The server handle.
   * @param conn   Connection information reflecting the updated parameters.
   */
  using ConnParamsHandler = std::function<void(BLEServer server, const BLEConnInfo &conn)>;

  /**
   * @brief Callback invoked when identity resolution completes for a peer.
   * @param server The server handle.
   * @param conn   Connection information with resolved identity address.
   */
  using IdentityHandler = std::function<void(BLEServer server, const BLEConnInfo &conn)>;

  /**
   * @brief Create a new GATT service and register it on this server.
   * @param uuid       UUID of the service (16-bit, 32-bit, or 128-bit).
   * @param numHandles Maximum number of attribute handles reserved for this service
   *                   (includes the service declaration, characteristics, and descriptors).
   * @param instId     Instance ID to distinguish multiple services with the same UUID.
   * @return A BLEService handle, or an invalid handle on failure.
   */
  BLEService createService(const BLEUUID &uuid, uint32_t numHandles = 15, uint8_t instId = 0);

  /**
   * @brief Look up an existing service by UUID.
   * @param uuid UUID of the service to find.
   * @return The matching BLEService handle, or an invalid handle if not found.
   */
  BLEService getService(const BLEUUID &uuid);

  /**
   * @brief Get all services registered on this server.
   * @return A vector of BLEService handles.
   */
  std::vector<BLEService> getServices() const;

  /**
   * @brief Remove a service from this server and from the stack.
   *
   * @note Bluedroid: deletes the GATT service via the controller.
   * @note NimBLE: rebuilds the whole GATT table (no per-service delete); active
   *       connections may be affected — prefer calling when no central is connected.
   *
   * @param service The service to remove.
   * @return BTStatus indicating success or the error encountered.
   */
  BTStatus removeService(const BLEService &service);

  /**
   * @brief Start the GATT server and register all added services with the stack.
   * @return BTStatus indicating success or the error encountered.
   */
  BTStatus start();

  /**
   * @brief Check whether the server has been started.
   * @return true if start() completed successfully; false otherwise.
   */
  bool isStarted() const;

  /**
   * @brief Register a handler called when a central connects.
   * @param handler Callback to invoke on connection, or nullptr to clear.
   */
  void onConnect(ConnectHandler handler);

  /**
   * @brief Register a handler called when a central disconnects.
   * @param handler Callback to invoke on disconnection, or nullptr to clear.
   */
  void onDisconnect(DisconnectHandler handler);

  /**
   * @brief Register a handler called when the ATT MTU changes.
   * @param handler Callback to invoke on MTU change, or nullptr to clear.
   */
  void onMtuChanged(MtuChangedHandler handler);

  /**
   * @brief Register a handler called when connection parameters are updated.
   * @param handler Callback to invoke on parameter update, or nullptr to clear.
   */
  void onConnParamsUpdate(ConnParamsHandler handler);

  /**
   * @brief Register a handler called when peer identity is resolved.
   * @param handler Callback to invoke on identity resolution, or nullptr to clear.
   */
  void onIdentity(IdentityHandler handler);

  /**
   * @brief Clear all registered callback handlers.
   */
  void resetCallbacks();

  /**
   * @brief Enable or disable automatic advertising restart after a disconnection.
   * @param enable true to restart advertising on disconnect; false to remain idle.
   */
  void advertiseOnDisconnect(bool enable);

  /**
   * @brief Get the advertising handle associated with this server.
   * @return A BLEAdvertising handle for configuring and controlling advertisements.
   */
  BLEAdvertising getAdvertising();

  /**
   * @brief Start advertising using the current advertising configuration.
   * @return BTStatus indicating success or the error encountered.
   */
  BTStatus startAdvertising();

  /**
   * @brief Stop any active advertising.
   * @return BTStatus indicating success or the error encountered.
   */
  BTStatus stopAdvertising();

  /**
   * @brief Get the number of currently connected centrals.
   * @return The active connection count.
   */
  size_t getConnectedCount() const;

  /**
   * @brief Get connection information for every active link.
   * @return A vector of BLEConnInfo for each connected peer.
   */
  std::vector<BLEConnInfo> getConnections() const;

  /**
   * @brief Disconnect a connected peer.
   * @param connHandle Connection handle of the peer to disconnect.
   * @param reason     HCI reason code (default 0x13 = Remote User Terminated Connection).
   * @return BTStatus indicating success or the error encountered.
   */
  BTStatus disconnect(uint16_t connHandle, uint8_t reason = 0x13);

  /**
   * @brief Initiate a connection to a remote device.
   * @param address Bluetooth address of the device to connect to.
   * @return BTStatus indicating success or the error encountered.
   */
  BTStatus connect(const BTAddress &address);

  /**
   * @brief Get the negotiated ATT MTU for a specific connection.
   * @param connHandle Connection handle to query.
   * @return The current ATT MTU size in bytes for that connection.
   */
  uint16_t getPeerMTU(uint16_t connHandle) const;

  /**
   * @brief Request a connection parameter update for a specific link.
   * @param connHandle Connection handle of the peer.
   * @param params     Desired connection parameters.
   * @return BTStatus indicating success or the error encountered.
   */
  BTStatus updateConnParams(uint16_t connHandle, const BLEConnParams &params);

  /**
   * @brief Set the preferred PHY for a connection (BLE 5.0+).
   * @param connHandle Connection handle of the peer.
   * @param txPhy      Preferred transmit PHY.
   * @param rxPhy      Preferred receive PHY.
   * @return BTStatus indicating success or the error encountered.
   */
  BTStatus setPhy(uint16_t connHandle, BLEPhy txPhy, BLEPhy rxPhy);

  /**
   * @brief Read the current PHY in use for a connection (BLE 5.0+).
   * @param connHandle Connection handle of the peer.
   * @param txPhy      [out] Current transmit PHY.
   * @param rxPhy      [out] Current receive PHY.
   * @return BTStatus indicating success or the error encountered.
   */
  BTStatus getPhy(uint16_t connHandle, BLEPhy &txPhy, BLEPhy &rxPhy) const;

  /**
   * @brief Set the Data Length Extension (DLE) parameters for a connection (BLE 4.2+).
   * @param connHandle Connection handle of the peer.
   * @param txOctets   Preferred maximum number of payload octets per TX PDU.
   * @param txTime     Preferred maximum TX time in microseconds.
   * @return BTStatus indicating success or the error encountered.
   */
  BTStatus setDataLen(uint16_t connHandle, uint16_t txOctets, uint16_t txTime);

  /**
   * @brief Forward an internal GAP event to the server (used by advertising).
   * @param event Opaque pointer to the backend-specific GAP event structure.
   * @return 0 on success, or a non-zero error code.
   */
  int handleGapEvent(void *event);

  struct Impl;

private:
  explicit BLEServer(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;
  friend class BLEClass;
  friend class BLEService;
};

#endif /* BLE_ENABLED */
