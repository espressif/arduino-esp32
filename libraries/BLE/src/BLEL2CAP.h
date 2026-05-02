/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2026 Ryan Powell <ryan@nable-embedded.io> and
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
#if BLE_ENABLED

#include "BTStatus.h"
#include "BTAddress.h"
#include "BLEConnInfo.h"
#include <memory>
#include <functional>

class BLEClass;

/**
 * @brief L2CAP Connection-Oriented Channel (CoC) handle.
 *
 * Represents a single bidirectional L2CAP CoC channel. Channels are
 * created by BLEL2CAPServer (server side) or BLEClass::connectL2CAP()
 * (client side).
 *
 * L2CAP CoC channels provide a higher-throughput alternative to GATT
 * for bulk data transfer, bypassing the GATT attribute protocol overhead.
 */
class BLEL2CAPChannel {
public:
  BLEL2CAPChannel();
  ~BLEL2CAPChannel() = default;
  BLEL2CAPChannel(const BLEL2CAPChannel &) = default;
  BLEL2CAPChannel &operator=(const BLEL2CAPChannel &) = default;
  BLEL2CAPChannel(BLEL2CAPChannel &&) = default;
  BLEL2CAPChannel &operator=(BLEL2CAPChannel &&) = default;

  /**
   * @brief Test whether this handle refers to a valid channel.
   * @return true if the channel is initialized and usable, false otherwise.
   */
  explicit operator bool() const;

  /**
   * @brief Callback invoked when data arrives on the channel.
   * @param channel The channel that received data.
   * @param data    Pointer to the received bytes.
   * @param len     Number of bytes received.
   */
  using DataHandler = std::function<void(BLEL2CAPChannel channel, const uint8_t *data, size_t len)>;

  /**
   * @brief Callback invoked when the channel is disconnected.
   * @param channel The channel that was disconnected.
   */
  using DisconnectHandler = std::function<void(BLEL2CAPChannel channel)>;

  /**
   * @brief Send data over the L2CAP channel.
   * @param data Pointer to the bytes to send.
   * @param len  Number of bytes to send.
   * @return BTStatus::OK on success.
   * @note Data larger than getMTU() will be segmented by the stack.
   */
  BTStatus write(const uint8_t *data, size_t len);

  /**
   * @brief Disconnect this L2CAP channel.
   * @return BTStatus::OK on success.
   */
  BTStatus disconnect();

  /**
   * @brief Register a handler for incoming data on this channel.
   * @param handler Callback to invoke when data is received.
   * @return BTStatus::OK on success.
   */
  BTStatus onData(DataHandler handler);

  /**
   * @brief Register a handler for disconnection of this channel.
   * @param handler Callback to invoke on disconnect.
   * @return BTStatus::OK on success.
   */
  BTStatus onDisconnect(DisconnectHandler handler);

  /**
   * @brief Remove all registered callbacks from this channel.
   * @return BTStatus::OK on success.
   */
  BTStatus resetCallbacks();

  /**
   * @brief Check if the channel is currently connected.
   * @return true if connected, false otherwise.
   */
  bool isConnected() const;

  /**
   * @brief Get the Protocol/Service Multiplexer number for this channel.
   * @return The PSM value.
   */
  uint16_t getPSM() const;

  /**
   * @brief Get the negotiated Maximum Transmission Unit for this channel.
   * @return The MTU in bytes.
   */
  uint16_t getMTU() const;

  /**
   * @brief Get the underlying BLE connection handle.
   * @return The HCI connection handle.
   */
  uint16_t getConnHandle() const;

  struct Impl;

private:
  explicit BLEL2CAPChannel(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;
  friend class BLEL2CAPServer;
  friend class BLEClass;
};

/**
 * @brief L2CAP CoC server — listens for incoming channel connections on a PSM.
 *
 * Create via BLE.createL2CAPServer(psm, mtu). When a remote device opens an
 * L2CAP CoC channel to this PSM, the onAccept callback fires with a
 * BLEL2CAPChannel handle.
 */
class BLEL2CAPServer {
public:
  BLEL2CAPServer();
  ~BLEL2CAPServer() = default;
  BLEL2CAPServer(const BLEL2CAPServer &) = default;
  BLEL2CAPServer &operator=(const BLEL2CAPServer &) = default;
  BLEL2CAPServer(BLEL2CAPServer &&) = default;
  BLEL2CAPServer &operator=(BLEL2CAPServer &&) = default;

  /**
   * @brief Test whether this handle refers to a valid server.
   * @return true if the server is initialized, false otherwise.
   */
  explicit operator bool() const;

  /**
   * @brief Callback invoked when a new L2CAP channel is accepted.
   * @param channel The newly accepted channel handle.
   */
  using AcceptHandler = std::function<void(BLEL2CAPChannel channel)>;

  /**
   * @brief Callback invoked when data arrives on any channel owned by this server.
   * @param channel The channel that received data.
   * @param data    Pointer to the received bytes.
   * @param len     Number of bytes received.
   */
  using DataHandler = std::function<void(BLEL2CAPChannel channel, const uint8_t *data, size_t len)>;

  /**
   * @brief Callback invoked when a channel owned by this server disconnects.
   * @param channel The channel that was disconnected.
   */
  using DisconnectHandler = std::function<void(BLEL2CAPChannel channel)>;

  /**
   * @brief Register a handler for incoming channel connections.
   * @param handler Callback to invoke when a remote device opens a channel to this PSM.
   * @return BTStatus::OK on success.
   */
  BTStatus onAccept(AcceptHandler handler);

  /**
   * @brief Register a default data handler for all channels accepted by this server.
   * @param handler Callback to invoke when data is received.
   * @return BTStatus::OK on success.
   * @note Per-channel handlers registered via BLEL2CAPChannel::onData() take precedence.
   */
  BTStatus onData(DataHandler handler);

  /**
   * @brief Register a default disconnect handler for all channels accepted by this server.
   * @param handler Callback to invoke on channel disconnect.
   * @return BTStatus::OK on success.
   */
  BTStatus onDisconnect(DisconnectHandler handler);

  /**
   * @brief Remove all registered callbacks from this server.
   * @return BTStatus::OK on success.
   */
  BTStatus resetCallbacks();

  /**
   * @brief Get the Protocol/Service Multiplexer number this server listens on.
   * @return The PSM value.
   */
  uint16_t getPSM() const;

  /**
   * @brief Get the MTU configured for this server.
   * @return The MTU in bytes.
   */
  uint16_t getMTU() const;

  struct Impl;

private:
  explicit BLEL2CAPServer(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;
  friend class BLEClass;
};

#endif /* BLE_ENABLED */
