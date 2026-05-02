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

#include <Stream.h>
#include "BTStatus.h"
#include "BTAddress.h"
#include "BLEConnInfo.h"
#include "BLEServer.h"
#include "BLEClient.h"
#include "BLECharacteristic.h"
#include "BLERemoteCharacteristic.h"
#include "BLEUUID.h"
#include <memory>
#include <functional>

/**
 * @brief Arduino Stream interface over BLE using the Nordic UART Service (NUS).
 *
 * BLEStream wraps BLE GATT operations behind the familiar Arduino Stream API.
 * It uses the Nordic UART Service (NUS) UUID conventions for maximum
 * interoperability with BLE serial apps (nRF Connect, Serial Bluetooth
 * Terminal, etc.).
 *
 * **Server mode** — the ESP32 advertises a NUS service and accepts connections:
 * @code
 * BLEStream bleStream;
 * bleStream.begin("MyDevice");          // starts advertising
 * if (bleStream.connected()) {
 *   bleStream.println("Hello World!");
 *   if (bleStream.available()) {
 *     String s = bleStream.readStringUntil('\n');
 *   }
 * }
 * @endcode
 *
 * **Client mode** — the ESP32 connects to a remote NUS server:
 * @code
 * BLEStream bleStream;
 * bleStream.beginClient(remoteAddr);    // connects to the remote device
 * bleStream.println("Hello Server!");
 * @endcode
 *
 * Stack-agnostic: works with both NimBLE and Bluedroid backends.
 */
class BLEStream : public Stream {
public:
  /**
   * @brief Construct a BLEStream with the given receive buffer size.
   * @param rxBufferSize Size (in bytes) of the internal receive ring buffer.
   */
  BLEStream(size_t rxBufferSize = 256);
  ~BLEStream();

  // Non-copyable, non-movable (owns server/client resources)
  BLEStream(const BLEStream &) = delete;
  BLEStream &operator=(const BLEStream &) = delete;

  /**
   * @brief Start as a NUS server with advertising.
   * @param deviceName BLE device name (used for BLE.begin and advertising)
   * @return BTStatus::OK on success
   */
  BTStatus begin(const String &deviceName = "BLE_Stream");

  /**
   * @brief Start as a NUS client and connect to a remote NUS server.
   * @param address BLE address of the remote NUS server
   * @param timeoutMs Connection timeout in milliseconds
   * @return BTStatus::OK on success
   */
  BTStatus beginClient(const BTAddress &address, uint32_t timeoutMs = 5000);

  /**
   * @brief Shut down the stream (stop advertising, disconnect, clean up).
   */
  void end();

  /**
   * @brief Check if a remote device is connected.
   */
  bool connected() const;

  /**
   * @brief Check whether the stream has been started (begin or beginClient).
   * @return true if the stream is in server or client mode.
   */
  explicit operator bool() const;

  // --- Arduino Stream interface ---

  /**
   * @brief Return the number of bytes available to read.
   * @return Number of bytes in the receive buffer.
   */
  int available() override;

  /**
   * @brief Read a single byte from the receive buffer.
   * @return The next byte (0–255), or -1 if no data is available.
   */
  int read() override;

  /**
   * @brief Peek at the next byte without removing it.
   * @return The next byte (0–255), or -1 if no data is available.
   */
  int peek() override;

  /**
   * @brief Write a single byte to the remote device.
   * @param c Byte to send.
   * @return 1 on success, 0 on failure or if not connected.
   */
  size_t write(uint8_t c) override;

  /**
   * @brief Write a buffer of bytes to the remote device.
   * @param buffer Pointer to the data to send.
   * @param size Number of bytes to send.
   * @return Number of bytes actually sent.
   */
  size_t write(const uint8_t *buffer, size_t size) override;

  /**
   * @brief Flush pending writes (currently a no-op for BLE).
   */
  void flush() override;

  // --- Callbacks ---

  /**
   * @brief Notified when a peer connects (Nordic UART role established).
   * @param connInfo Connection metadata for the newly connected peer.
   */
  using ConnectHandler = std::function<void(const BLEConnInfo &connInfo)>;
  /**
   * @brief Notified when the peer disconnects.
   * @param connInfo Connection metadata for the disconnected peer.
   * @param reason Disconnect reason code from the stack.
   */
  using DisconnectHandler = std::function<void(const BLEConnInfo &connInfo, uint8_t reason)>;

  /**
   * @brief Register a callback for connection events.
   * @param handler Function to call when a peer connects.
   */
  void onConnect(ConnectHandler handler);

  /**
   * @brief Register a callback for disconnection events.
   * @param handler Function to call when the peer disconnects.
   */
  void onDisconnect(DisconnectHandler handler);

  /**
   * @brief Remove all registered callbacks.
   */
  void resetCallbacks();

  // --- NUS UUIDs (Nordic UART Service) ---

  /**
   * @brief Get the Nordic UART Service UUID.
   * @return The 128-bit NUS service UUID.
   */
  static BLEUUID nusServiceUUID();

  /**
   * @brief Get the NUS RX Characteristic UUID (peer writes here).
   * @return The 128-bit NUS RX characteristic UUID.
   */
  static BLEUUID nusRxCharUUID();

  /**
   * @brief Get the NUS TX Characteristic UUID (peer reads/subscribes here).
   * @return The 128-bit NUS TX characteristic UUID.
   */
  static BLEUUID nusTxCharUUID();

private:
  struct Impl;
  Impl *_impl;
};

#endif /* BLE_ENABLED */
