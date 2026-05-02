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
#include "WString.h"
#include "BTStatus.h"
#include "BTAddress.h"
#include "BLEUUID.h"
#include "BLEAdvTypes.h"
#include "BLEConnInfo.h"
#include <memory>
#include <functional>

class BLEAdvertisedDevice;
class BLERemoteService;
class BLERemoteCharacteristic;
class BLERemoteDescriptor;

/**
 * @brief GATT client -- multi-instance, connect to remote peripherals.
 *
 * Create via BLE.createClient(). Each instance manages one connection.
 * Multiple BLEClient instances can coexist for multi-connection use cases.
 *
 * Callback threading: client callbacks (onConnect, onDisconnect, notify
 * callbacks, etc.) execute on the BLE stack task. They are invoked
 * outside the client mutex and must not block for extended periods.
 *
 * GATT operation limitation: only one read or write can be in-flight per
 * client at a time (Bluedroid delivers completions without a per-operation
 * token). Concurrent reads/writes from multiple tasks will collide.
 */
class BLEClient {
public:
  BLEClient();
  ~BLEClient() = default;
  BLEClient(const BLEClient &) = default;
  BLEClient &operator=(const BLEClient &) = default;
  BLEClient(BLEClient &&) = default;
  BLEClient &operator=(BLEClient &&) = default;

  /**
   * @brief Boolean conversion — true if this handle references a valid client.
   *
   * @return true if the underlying implementation exists.
   */
  explicit operator bool() const;

  /**
   * @brief Callback invoked when a connection is established.
   *
   * @param client  The BLEClient that connected.
   * @param conn    Connection information for the new link.
   */
  using ConnectHandler = std::function<void(BLEClient client, const BLEConnInfo &conn)>;

  /**
   * @brief Callback invoked when a connection is terminated.
   *
   * @param client  The BLEClient that was disconnected.
   * @param conn    Connection information for the terminated link.
   * @param reason  BLE HCI disconnect reason code.
   */
  using DisconnectHandler = std::function<void(BLEClient client, const BLEConnInfo &conn, uint8_t reason)>;

  /**
   * @brief Callback invoked when a connection attempt fails.
   *
   * @param client  The BLEClient whose connection attempt failed.
   * @param reason  Backend-specific error code.
   */
  using ConnectFailHandler = std::function<void(BLEClient client, int reason)>;

  /**
   * @brief Callback invoked when the ATT MTU changes after negotiation.
   *
   * @param client  The BLEClient whose MTU changed.
   * @param conn    Connection information for the link.
   * @param mtu     The new effective MTU size.
   */
  using MtuChangedHandler = std::function<void(BLEClient client, const BLEConnInfo &conn, uint16_t mtu)>;

  /**
   * @brief Callback invoked when the remote peer requests a connection parameter update.
   *
   * @param client  The BLEClient receiving the request.
   * @param params  The requested connection parameters.
   * @return true to accept the parameters; false to reject.
   */
  using ConnParamsReqHandler = std::function<bool(BLEClient client, const BLEConnParams &params)>;

  /**
   * @brief Callback invoked when the peer's identity is resolved (after bonding/pairing).
   *
   * @param client  The BLEClient whose peer identity was resolved.
   * @param conn    Updated connection information including the resolved address.
   */
  using IdentityHandler = std::function<void(BLEClient client, const BLEConnInfo &conn)>;

  /**
   * @brief Connect to a peripheral by address (blocking).
   *
   * Blocks the calling task until the connection completes or the timeout
   * expires. Uses the default 1M PHY.
   *
   * @param address    Address of the remote peripheral.
   * @param timeoutMs  Maximum time to wait for the connection (ms).
   * @return BTStatus indicating success or the connection error.
   */
  BTStatus connect(const BTAddress &address, uint32_t timeoutMs = 5000);

  /**
   * @brief Connect to a previously scanned device (blocking).
   *
   * @param device     An advertised device obtained from a BLEScan.
   * @param timeoutMs  Maximum time to wait for the connection (ms).
   * @return BTStatus indicating success or the connection error.
   */
  BTStatus connect(const BLEAdvertisedDevice &device, uint32_t timeoutMs = 5000);

  /**
   * @brief Connect to a peripheral by address on a specific PHY (blocking).
   *
   * @param address    Address of the remote peripheral.
   * @param phy        PHY to use for the connection (1M, 2M, or Coded).
   * @param timeoutMs  Maximum time to wait for the connection (ms).
   * @return BTStatus indicating success or the connection error.
   */
  BTStatus connect(const BTAddress &address, BLEPhy phy, uint32_t timeoutMs = 5000);

  /**
   * @brief Connect to a previously scanned device on a specific PHY (blocking).
   *
   * @param device     An advertised device obtained from a BLEScan.
   * @param phy        PHY to use for the connection (1M, 2M, or Coded).
   * @param timeoutMs  Maximum time to wait for the connection (ms).
   * @return BTStatus indicating success or the connection error.
   */
  BTStatus connect(const BLEAdvertisedDevice &device, BLEPhy phy, uint32_t timeoutMs = 5000);

  /**
   * @brief Initiate connection by address without blocking.
   *
   * The result is delivered via the ConnectHandler or ConnectFailHandler
   * callbacks.
   *
   * @param address  Address of the remote peripheral.
   * @param phy      PHY to use for the connection (default 1M).
   * @return BTStatus indicating whether the connect request was accepted.
   */
  BTStatus connectAsync(const BTAddress &address, BLEPhy phy = BLEPhy::PHY_1M);

  /**
   * @brief Initiate connection to a scanned device without blocking.
   *
   * @param device  An advertised device obtained from a BLEScan.
   * @param phy     PHY to use for the connection (default 1M).
   * @return BTStatus indicating whether the connect request was accepted.
   */
  BTStatus connectAsync(const BLEAdvertisedDevice &device, BLEPhy phy = BLEPhy::PHY_1M);

  /**
   * @brief Cancel a pending connection attempt.
   *
   * @return BTStatus indicating success or failure.
   */
  BTStatus cancelConnect();

  /**
   * @brief Disconnect from the remote peripheral.
   *
   * @return BTStatus indicating success or failure.
   */
  BTStatus disconnect();

  /**
   * @brief Check whether this client is currently connected.
   *
   * @return true if connected.
   */
  bool isConnected() const;

  /**
   * @brief Initiate or upgrade security on the current connection (pairing/bonding).
   *
   * @return BTStatus indicating success or failure.
   */
  BTStatus secureConnection();

  /**
   * @brief Get a remote service by UUID from the discovered service cache.
   *
   * @note Call discoverServices() first, or this may return an invalid handle.
   *
   * @param uuid  UUID of the service to look up.
   * @return A BLERemoteService handle. Check with `operator bool()` for validity.
   */
  BLERemoteService getService(const BLEUUID &uuid);

  /**
   * @brief Get all discovered remote services.
   *
   * @return A vector of BLERemoteService handles (empty if none discovered).
   */
  std::vector<BLERemoteService> getServices() const;

  /**
   * @brief Perform GATT service discovery on the connected peripheral.
   *
   * Populates the internal service cache used by getService() and getServices().
   *
   * @return BTStatus indicating success or failure.
   */
  BTStatus discoverServices();

  /**
   * @brief Read a characteristic value by service and characteristic UUID.
   *
   * Convenience wrapper that locates the service and characteristic, then reads.
   *
   * @param serviceUUID  UUID of the containing service.
   * @param charUUID     UUID of the characteristic to read.
   * @return The characteristic value as a String, or an empty String on failure.
   */
  String getValue(const BLEUUID &serviceUUID, const BLEUUID &charUUID);

  /**
   * @brief Write a characteristic value by service and characteristic UUID.
   *
   * @param serviceUUID  UUID of the containing service.
   * @param charUUID     UUID of the characteristic to write.
   * @param value        The value to write.
   * @return BTStatus indicating success or failure.
   */
  BTStatus setValue(const BLEUUID &serviceUUID, const BLEUUID &charUUID, const String &value);

  /**
   * @brief Set the callback invoked when a connection is established.
   *
   * @param handler  ConnectHandler callback, or nullptr to clear.
   */
  void onConnect(ConnectHandler handler);

  /**
   * @brief Set the callback invoked when a connection is terminated.
   *
   * @param handler  DisconnectHandler callback, or nullptr to clear.
   */
  void onDisconnect(DisconnectHandler handler);

  /**
   * @brief Set the callback invoked when a connection attempt fails.
   *
   * @param handler  ConnectFailHandler callback, or nullptr to clear.
   */
  void onConnectFail(ConnectFailHandler handler);

  /**
   * @brief Set the callback invoked when the ATT MTU changes.
   *
   * @param handler  MtuChangedHandler callback, or nullptr to clear.
   */
  void onMtuChanged(MtuChangedHandler handler);

  /**
   * @brief Set the callback for remote connection-parameter update requests.
   *
   * @param handler  ConnParamsReqHandler callback, or nullptr to clear.
   */
  void onConnParamsUpdateRequest(ConnParamsReqHandler handler);

  /**
   * @brief Set the callback invoked when the peer's identity is resolved.
   *
   * @param handler  IdentityHandler callback, or nullptr to clear.
   */
  void onIdentity(IdentityHandler handler);

  /**
   * @brief Remove all registered callbacks.
   */
  void resetCallbacks();

  /**
   * @brief Set the preferred ATT MTU to request during MTU exchange.
   *
   * @param mtu  Preferred MTU size.
   */
  void setMTU(uint16_t mtu);

  /**
   * @brief Get the current effective ATT MTU for this connection.
   *
   * @return The negotiated MTU, or the default (23) if not yet exchanged.
   */
  uint16_t getMTU() const;

  /**
   * @brief Read the RSSI of the current connection.
   *
   * @return RSSI in dBm, or 0 if not connected.
   */
  int8_t getRSSI() const;

  /**
   * @brief Get the address of the connected peer.
   *
   * @return The peer's BTAddress.
   */
  BTAddress getPeerAddress() const;

  /**
   * @brief Get the BLE connection handle for this link.
   *
   * @return The connection handle, or 0 if not connected.
   */
  uint16_t getHandle() const;

  /**
   * @brief Get detailed connection information.
   *
   * @return A BLEConnInfo snapshot of the current connection state.
   */
  BLEConnInfo getConnection() const;

  /**
   * @brief Request a connection parameter update.
   *
   * @param params  The desired connection parameters.
   * @return BTStatus indicating success or failure.
   */
  BTStatus updateConnParams(const BLEConnParams &params);

  /**
   * @brief Set the preferred PHY for this connection.
   *
   * @param txPhy  Preferred transmit PHY (1M, 2M, or Coded).
   * @param rxPhy  Preferred receive PHY (1M, 2M, or Coded).
   * @return BTStatus indicating success or failure.
   */
  BTStatus setPhy(BLEPhy txPhy, BLEPhy rxPhy);

  /**
   * @brief Get the active PHY for this connection.
   *
   * @param txPhy  Output: current transmit PHY.
   * @param rxPhy  Output: current receive PHY.
   * @return BTStatus indicating success or failure.
   */
  BTStatus getPhy(BLEPhy &txPhy, BLEPhy &rxPhy) const;

  /**
   * @brief Set the Data Length Extension parameters for this connection.
   *
   * @param txOctets  Maximum number of payload octets per TX PDU (27–251).
   * @param txTime    Maximum TX time per PDU in microseconds (328–17040).
   * @return BTStatus indicating success or failure.
   */
  BTStatus setDataLen(uint16_t txOctets, uint16_t txTime);

  /**
   * @brief Get a human-readable description of this client.
   *
   * @return A String with the peer address and connection state.
   */
  String toString() const;

  struct Impl;

private:
  explicit BLEClient(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;
  friend class BLEClass;
  friend class BLERemoteService;
  friend class BLERemoteCharacteristic;
  friend class BLERemoteDescriptor;
};

#endif /* BLE_ENABLED */
