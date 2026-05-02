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
#if BLE_ENABLED

#include <functional>
#include <vector>
#include "WString.h"
#include "BTStatus.h"
#include "BTAddress.h"
#include "BLEAdvTypes.h"
#include "BLEServer.h"
#include "BLEClient.h"
#include "BLECharacteristic.h"
#include "BLEDescriptor.h"
#include "BLEService.h"
#include "BLEAdvertising.h"
#include "BLEScan.h"
#include "BLESecurity.h"
#include "BLEAdvertisedDevice.h"
#include "BLEScanResults.h"
#include "BLERemoteService.h"
#include "BLERemoteCharacteristic.h"
#include "BLERemoteDescriptor.h"
#include "BLEAdvertisementData.h"
#include "BLEBeacon.h"
#include "BLEEddystone.h"
#include "BLEHIDDevice.h"
#include "BLEStream.h"
#include "BLEL2CAP.h"

/**
 * @brief Global BLE singleton -- the entry point for all BLE operations.
 *
 * Follows the Arduino convention used by WiFi, Serial, Wire, etc.
 * The class is BLEClass; users interact via the global `BLE` object.
 *
 * Thread safety: the BLE singleton methods (begin, end, whitelist
 * operations, etc.) are NOT internally synchronized. Call them from
 * a single task — typically the Arduino loop task. BLE callbacks execute
 * on the stack's own task (NimBLE host task or Bluedroid BTC task) and
 * must not call BLEClass methods that mutate state.
 *
 * Usage:
 * @code
 * #include <BLE.h>
 *
 * void setup() {
 *     BLE.begin("MyDevice");
 *     BLEServer server = BLE.createServer();
 *     // ...
 * }
 * @endcode
 */
class BLEClass {
public:
  // --- Lifecycle ---

  /**
   * @brief Initialize the BLE stack and set the device name.
   *
   * Must be called before any other BLE operation. Starts the Bluetooth
   * controller, host stack, and GATT/GAP services.
   *
   * @param deviceName  Advertised device name. Empty string keeps the default.
   * @return BTStatus indicating success or the specific initialization error.
   */
  BTStatus begin(const String &deviceName = "");

  /**
   * @brief Shut down the BLE stack.
   *
   * Stops advertising, disconnects all peers, and deinitializes the host.
   *
   * @param releaseMemory  If true, also releases the Bluetooth controller
   *                       memory. After this, begin() cannot reinitialize BLE
   *                       without a full reboot.
   */
  void end(bool releaseMemory = false);

  /**
   * @brief Check whether the BLE stack has been initialized.
   *
   * @return true if begin() completed successfully and end() has not been called.
   */
  bool isInitialized() const;

  /**
   * @brief Boolean conversion — equivalent to isInitialized().
   *
   * @return true if BLE is initialized and ready.
   */
  explicit operator bool() const;

  // --- Identity ---

  /**
   * @brief Get the local Bluetooth address currently in use.
   *
   * @return The device's BTAddress (public or random, depending on own-address type).
   */
  BTAddress getAddress() const;

  /**
   * @brief Get the device name that was set via begin().
   *
   * @return The current device name as a String.
   */
  String getDeviceName() const;

  /**
   * @brief Set the own-address type used for advertising and connections.
   *
   * @param type  Address type (Public, Random, RPA with public fallback, etc.).
   * @return BTStatus indicating success or failure.
   */
  BTStatus setOwnAddressType(BTAddress::Type type);

  /**
   * @brief Get the currently configured own-address type.
   *
   * @return The BTAddress::Type in use.
   */
  BTAddress::Type getOwnAddressType() const;

  /**
   * @brief Override the local Bluetooth address.
   *
   * @note Must be called before begin(), or the stack may ignore the change.
   *
   * @param addr  The address to use.
   * @return BTStatus indicating success or failure.
   */
  BTStatus setOwnAddress(const BTAddress &addr);

  // --- Factory / Singletons ---

  /**
   * @brief Create (or retrieve) the GATT server singleton.
   *
   * The first call allocates internal state; subsequent calls return a
   * handle to the same underlying server.
   *
   * @return A BLEServer handle. Check with `operator bool()` for validity.
   */
  BLEServer createServer();

  /**
   * @brief Get the BLE scan singleton.
   *
   * @return A BLEScan handle used to configure and start scanning.
   */
  BLEScan getScan();

  /**
   * @brief Get the advertising singleton.
   *
   * @return A BLEAdvertising handle used to configure and start advertising.
   */
  BLEAdvertising getAdvertising();

  /**
   * @brief Get the security/pairing singleton.
   *
   * @return A BLESecurity handle for configuring pairing and bonding.
   */
  BLESecurity getSecurity();

  /**
   * @brief Create a new GATT client instance.
   *
   * Each call allocates a new client. The caller is responsible for
   * connecting and eventually disconnecting.
   *
   * @return A BLEClient handle. Check with `operator bool()` for validity.
   */
  BLEClient createClient();

  // --- L2CAP CoC ---

  /**
   * @brief Create an L2CAP Connection-Oriented Channel server.
   *
   * Listens for incoming L2CAP CoC connections on the given PSM.
   *
   * @param psm  Protocol/Service Multiplexer value to listen on.
   * @param mtu  Maximum Transmission Unit for the channel (default 512).
   * @return A BLEL2CAPServer handle.
   */
  BLEL2CAPServer createL2CAPServer(uint16_t psm, uint16_t mtu = 512);

  /**
   * @brief Open an outgoing L2CAP CoC channel to a connected peer.
   *
   * @param connHandle  BLE connection handle of the already-connected peer.
   * @param psm         Protocol/Service Multiplexer on the remote device.
   * @param mtu         Maximum Transmission Unit for the channel (default 512).
   * @return A BLEL2CAPChannel handle for the new channel.
   */
  BLEL2CAPChannel connectL2CAP(uint16_t connHandle, uint16_t psm, uint16_t mtu = 512);

  // --- Power ---

  /**
   * @brief Set the BLE transmit power.
   *
   * @param txPowerDbm  Transmit power in dBm. The stack clamps to the
   *                    nearest supported value.
   */
  void setPower(int8_t txPowerDbm);

  /**
   * @brief Get the current BLE transmit power.
   *
   * @return Transmit power in dBm.
   */
  int8_t getPower() const;

  // --- MTU ---

  /**
   * @brief Set the preferred ATT MTU for future connections.
   *
   * @note Does not affect already-established connections.
   *
   * @param mtu  Preferred MTU size (23–517).
   * @return BTStatus indicating success or failure.
   */
  BTStatus setMTU(uint16_t mtu);

  /**
   * @brief Get the configured preferred ATT MTU.
   *
   * @return The preferred MTU value.
   */
  uint16_t getMTU() const;

  // --- IRK retrieval (for Home Assistant, ESPresense, etc.) ---

  /**
   * @brief Copy the local Identity Resolving Key into the provided buffer.
   *
   * @param irk  Output buffer of at least 16 bytes.
   * @return true if the IRK was successfully retrieved.
   */
  bool getLocalIRK(uint8_t irk[16]) const;

  /**
   * @brief Get the local IRK as a hex string.
   *
   * @return Hex-encoded IRK, or an empty string on failure.
   */
  String getLocalIRKString() const;

  /**
   * @brief Get the local IRK as a Base64-encoded string.
   *
   * @return Base64-encoded IRK, or an empty string on failure.
   */
  String getLocalIRKBase64() const;

  /**
   * @brief Copy a bonded peer's IRK into the provided buffer.
   *
   * @param peer  Address of the bonded peer.
   * @param irk   Output buffer of at least 16 bytes.
   * @return true if the IRK was found and copied.
   */
  bool getPeerIRK(const BTAddress &peer, uint8_t irk[16]) const;

  /**
   * @brief Get a bonded peer's IRK as a hex string.
   *
   * @param peer  Address of the bonded peer.
   * @return Hex-encoded IRK, or an empty string if not found.
   */
  String getPeerIRKString(const BTAddress &peer) const;

  /**
   * @brief Get a bonded peer's IRK as a Base64-encoded string.
   *
   * @param peer  Address of the bonded peer.
   * @return Base64-encoded IRK, or an empty string if not found.
   */
  String getPeerIRKBase64(const BTAddress &peer) const;

  /**
   * @brief Get a bonded peer's IRK as a byte-reversed hex string.
   *
   * Useful for systems that expect the IRK in reverse byte order
   * (e.g., some Home Assistant integrations).
   *
   * @param peer  Address of the bonded peer.
   * @return Reversed hex-encoded IRK, or an empty string if not found.
   */
  String getPeerIRKReverse(const BTAddress &peer) const;

  // --- Default PHY preference (BLE 5.0) ---

  /**
   * @brief Set the default PHY preferences for new connections.
   *
   * @param txPhy  Preferred transmit PHY (1M, 2M, or Coded).
   * @param rxPhy  Preferred receive PHY (1M, 2M, or Coded).
   * @return BTStatus indicating success or failure.
   */
  BTStatus setDefaultPhy(BLEPhy txPhy, BLEPhy rxPhy);

  /**
   * @brief Get the current default PHY preferences.
   *
   * @param txPhy  Output: current default transmit PHY.
   * @param rxPhy  Output: current default receive PHY.
   * @return BTStatus indicating success or failure.
   */
  BTStatus getDefaultPhy(BLEPhy &txPhy, BLEPhy &rxPhy) const;

  // --- Whitelist ---

  /**
   * @brief Add a device address to the BLE whitelist.
   *
   * @param address  The peer address to add.
   * @return BTStatus indicating success or failure.
   */
  BTStatus whiteListAdd(const BTAddress &address);

  /**
   * @brief Remove a device address from the BLE whitelist.
   *
   * @param address  The peer address to remove.
   * @return BTStatus indicating success or failure.
   */
  BTStatus whiteListRemove(const BTAddress &address);

  /**
   * @brief Check whether an address is on the whitelist.
   *
   * @param address  The peer address to look up.
   * @return true if the address is currently on the whitelist.
   */
  bool isOnWhiteList(const BTAddress &address) const;

  // --- Advertising shortcuts ---

  /**
   * @brief Start advertising using the current advertising configuration.
   *
   * @return BTStatus indicating success or failure.
   */
  BTStatus startAdvertising();

  /**
   * @brief Stop advertising.
   *
   * @return BTStatus indicating success or failure.
   */
  BTStatus stopAdvertising();

  // --- Stack info ---

  enum class Stack {
    NimBLE,
    Bluedroid
  };

  /**
   * @brief Get the Bluetooth stack in use at compile time.
   *
   * @return Stack::NimBLE or Stack::Bluedroid.
   */
  Stack getStack() const;

  /**
   * @brief Get the Bluetooth stack name as a C-string.
   *
   * @return "NimBLE" or "Bluedroid".
   */
  const char *getStackName() const;

  /**
   * @brief Check whether BLE is running over esp-hosted (e.g., on ESP32-P4).
   *
   * @return true if the BLE stack is hosted on a remote co-processor.
   */
  bool isHostedBLE() const;

  // --- Hosted BLE (ESP32-P4 via esp-hosted) ---

  /**
   * @brief Configure the SDIO/SPI pins for the esp-hosted co-processor.
   *
   * @note Only relevant on chips that use esp-hosted for BLE (e.g., ESP32-P4).
   *       Must be called before begin().
   *
   * @param clk  Clock pin.
   * @param cmd  Command pin.
   * @param d0   Data line 0.
   * @param d1   Data line 1.
   * @param d2   Data line 2.
   * @param d3   Data line 3.
   * @param rst  Reset pin.
   * @return BTStatus indicating success or failure.
   */
  BTStatus setPins(int8_t clk, int8_t cmd, int8_t d0, int8_t d1, int8_t d2, int8_t d3, int8_t rst);

  // --- Custom event handlers (advanced/extension point) ---

  /**
   * @brief Signature for raw stack event handlers.
   *
   * @param event  Pointer to the backend-specific event structure.
   * @param arg    Backend-specific context argument.
   * @return 0 on success; non-zero to signal an error to the stack.
   */
  using RawEventHandler = std::function<int(void *event, void *arg)>;

  /**
   * @brief Register a custom handler for raw GAP events.
   *
   * @param handler  Callback invoked for every GAP event from the stack.
   * @return BTStatus indicating success or failure.
   */
  BTStatus setCustomGapHandler(RawEventHandler handler);

  /**
   * @brief Register a custom handler for raw GATT-client events.
   *
   * @param handler  Callback invoked for every GATT-client event from the stack.
   * @return BTStatus indicating success or failure.
   */
  BTStatus setCustomGattcHandler(RawEventHandler handler);

  /**
   * @brief Register a custom handler for raw GATT-server events.
   *
   * @param handler  Callback invoked for every GATT-server event from the stack.
   * @return BTStatus indicating success or failure.
   */
  BTStatus setCustomGattsHandler(RawEventHandler handler);

  BLEClass();
  ~BLEClass();
  BLEClass(const BLEClass &) = delete;
  BLEClass &operator=(const BLEClass &) = delete;
  BLEClass(BLEClass &&) = delete;
  BLEClass &operator=(BLEClass &&) = delete;

private:
  struct Impl;
  Impl *_impl;

  bool _initialized = false;
  bool _memoryReleased = false;
  BTAddress::Type _ownAddressType = BTAddress::Type::Public;
  String _deviceName;
  std::vector<BTAddress> _whiteList;
};

#if !defined(NO_GLOBAL_INSTANCES) && !defined(NO_GLOBAL_BLE)
extern BLEClass BLE;
#endif

#endif /* BLE_ENABLED */
