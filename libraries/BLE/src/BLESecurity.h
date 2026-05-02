/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
 * Copyright 2017 chegewara
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
#include "BLEConnInfo.h"
#include <memory>
#include <functional>

/**
 * @brief BLE Security configuration and bond management.
 *
 * Lightweight shared handle. Access via BLE.getSecurity().
 * Configures IO capability, authentication mode, passkey behavior,
 * and provides event handlers for the pairing flow.
 */
class BLESecurity {
public:
  BLESecurity();
  ~BLESecurity() = default;
  BLESecurity(const BLESecurity &) = default;
  BLESecurity &operator=(const BLESecurity &) = default;
  BLESecurity(BLESecurity &&) = default;
  BLESecurity &operator=(BLESecurity &&) = default;

  /**
   * @brief Check whether this handle references a valid security instance.
   * @return true if the handle is backed by an initialized implementation, false otherwise.
   */
  explicit operator bool() const;

  // --- IO Capability ---

  /**
   * @brief SMP I/O capability values (pairing user interface model).
   */
  enum IOCapability : uint8_t {
    DisplayOnly = 0,      ///< Device can display a passkey but has no input.
    DisplayYesNo = 1,     ///< Device can display a passkey and accept yes/no confirmation.
    KeyboardOnly = 2,     ///< Device can accept keyboard input but has no display.
    NoInputNoOutput = 3,  ///< Device has no I/O capability (Just Works pairing).
    KeyboardDisplay = 4,  ///< Device has both keyboard input and a display.
  };

  /**
   * @brief Set the I/O capability used for pairing negotiation.
   * @param cap I/O capability value.
   */
  void setIOCapability(IOCapability cap);

  /**
   * @brief Set the authentication requirements for pairing.
   * @param bonding If true, enable bonding (persist keys across connections).
   * @param mitm If true, require MITM protection (passkey or numeric comparison).
   * @param secureConnection If true, require LE Secure Connections (LESC).
   */
  void setAuthenticationMode(bool bonding, bool mitm, bool secureConnection);

  // --- Passkey ---

  /**
   * @brief Configure static or random passkey mode.
   * @param isStatic If true, use a fixed passkey; if false, generate a random one each pairing.
   * @param passkey The fixed passkey value (0–999999). Ignored when isStatic is false.
   */
  void setPassKey(bool isStatic, uint32_t passkey = 0);

  /**
   * @brief Set a fixed static passkey for all future pairings.
   * @param passkey The passkey value (0–999999).
   */
  void setStaticPassKey(uint32_t passkey);

  /**
   * @brief Switch to random passkey generation mode.
   */
  void setRandomPassKey();

  /**
   * @brief Get the currently configured passkey.
   * @return The passkey value.
   */
  uint32_t getPassKey() const;

  /**
   * @brief Generate a cryptographically random 6-digit passkey.
   * @return A random value in the range 0–999999.
   */
  static uint32_t generateRandomPassKey();

  /**
   * @brief Enable or disable automatic passkey regeneration on each new connection.
   * @param enable If true, a new random passkey is generated before every pairing.
   * @note Only effective when random passkey mode is active.
   */
  void regenPassKeyOnConnect(bool enable);

  // --- Security Callbacks ---

  /**
   * @brief Callback requesting the application to supply a passkey for keyboard-entry pairing.
   * @param conn Connection info for the peer requesting pairing.
   * @return The passkey (0–999999) to use for this pairing.
   */
  using PassKeyRequestHandler = std::function<uint32_t(const BLEConnInfo &conn)>;

  /**
   * @brief Callback requesting the application to display a passkey to the user.
   * @param conn Connection info for the peer requesting pairing.
   * @param passKey The passkey value to display.
   */
  using PassKeyDisplayHandler = std::function<void(const BLEConnInfo &conn, uint32_t passKey)>;

  /**
   * @brief Callback requesting the user to confirm a numeric comparison value.
   * @param conn Connection info for the peer requesting pairing.
   * @param passKey The numeric comparison value shown on both devices.
   * @return true if the user confirms the values match, false to reject.
   */
  using ConfirmPassKeyHandler = std::function<bool(const BLEConnInfo &conn, uint32_t passKey)>;

  /**
   * @brief Callback invoked when a peer initiates a security request.
   * @param conn Connection info for the requesting peer.
   * @return true to accept and begin pairing, false to reject.
   */
  using SecurityRequestHandler = std::function<bool(const BLEConnInfo &conn)>;

  /**
   * @brief Callback for runtime ATT-level authorization of reads/writes on protected attributes.
   * @param conn Connection info for the requesting peer.
   * @param attrHandle The attribute handle being accessed.
   * @param isRead true for a read operation, false for a write.
   * @return true to allow the access, false to reject with an ATT error.
   */
  using AuthorizationHandler = std::function<bool(const BLEConnInfo &conn, uint16_t attrHandle, bool isRead)>;

  /**
   * @brief Callback invoked when authentication (pairing) completes.
   * @param conn Connection info for the peer.
   * @param success true if pairing succeeded, false on failure.
   */
  using AuthCompleteHandler = std::function<void(const BLEConnInfo &conn, bool success)>;

  /**
   * @brief Register a handler invoked when the stack needs a passkey from the application.
   * @param handler The handler to invoke, or nullptr to clear.
   */
  void onPassKeyRequest(PassKeyRequestHandler handler);

  /**
   * @brief Register a handler invoked when the stack needs the application to display a passkey.
   * @param handler The handler to invoke, or nullptr to clear.
   */
  void onPassKeyDisplay(PassKeyDisplayHandler handler);

  /**
   * @brief Register a handler for numeric comparison confirmation.
   * @param handler The handler to invoke, or nullptr to clear.
   */
  void onConfirmPassKey(ConfirmPassKeyHandler handler);

  /**
   * @brief Register a handler for incoming security requests from a peer.
   * @param handler The handler to invoke, or nullptr to clear.
   */
  void onSecurityRequest(SecurityRequestHandler handler);

  /**
   * @brief Register a handler for runtime ATT authorization decisions.
   * @param handler The handler to invoke, or nullptr to clear.
   */
  void onAuthorization(AuthorizationHandler handler);

  /**
   * @brief Register a handler called when authentication completes.
   * @param handler The handler to invoke, or nullptr to clear.
   */
  void onAuthenticationComplete(AuthCompleteHandler handler);

  /**
   * @brief Remove all registered security callbacks.
   */
  void resetCallbacks();

  // --- Key Distribution ---

  /**
   * @brief Bitmask of SMP key types to distribute during pairing.
   */
  enum class KeyDist : uint8_t {
    EncKey = 0x01,   ///< Long Term Key (LTK) / encryption key.
    IdKey = 0x02,    ///< Identity Resolving Key (IRK).
    SignKey = 0x04,  ///< Connection Signature Resolving Key (CSRK).
    LinkKey = 0x08,  ///< BR/EDR Link Key derived from LE pairing.
  };
  friend inline constexpr KeyDist operator|(KeyDist a, KeyDist b) {
    return static_cast<KeyDist>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
  }

  /**
   * @brief Set which key types the initiator distributes during pairing.
   * @param keys Bitmask of KeyDist values combined with operator|.
   */
  void setInitiatorKeys(KeyDist keys);

  /**
   * @brief Set which key types the responder distributes during pairing.
   * @param keys Bitmask of KeyDist values combined with operator|.
   */
  void setResponderKeys(KeyDist keys);

  /**
   * @brief Set the maximum encryption key size.
   * @param size Key size in bytes (7–16).
   */
  void setKeySize(uint8_t size);

  // --- Authentication ---

  /**
   * @brief Require authenticated pairing for all GATT access on encrypted characteristics.
   * @param force If true, unauthenticated encrypted links are rejected.
   * @note When this path is used together with an active GATT client, the
   *       relative ordering of "authentication complete" and "first secure
   *       read/write" is stack-dependent; when in doubt, wait for
   *       @ref onAuthenticationComplete or @ref waitForAuthenticationComplete
   *       before relying on protected attributes.
   */
  void setForceAuthentication(bool force);

  /**
   * @brief Get whether forced authentication is enabled.
   * @return true if forced authentication is active, false otherwise.
   */
  bool getForceAuthentication() const;

  // --- Bond Management ---

  /**
   * @brief Retrieve the list of bonded device addresses.
   * @return Vector of addresses for all currently bonded peers.
   */
  std::vector<BTAddress> getBondedDevices() const;

  /**
   * @brief Delete the bond for a specific device.
   * @param address Address of the bonded device to remove.
   * @return BTStatus indicating success or error.
   */
  BTStatus deleteBond(const BTAddress &address);

  /**
   * @brief Delete all stored bonds.
   * @return BTStatus indicating success or error.
   */
  BTStatus deleteAllBonds();

  /**
   * @brief Callback invoked when the bond store is full and a new bond must be saved.
   * @param oldestBond Address of the oldest bonded device that would be evicted.
   */
  using BondStoreOverflowHandler = std::function<void(const BTAddress &oldestBond)>;

  /**
   * @brief Register a handler for bond store overflow events.
   * @param handler The handler to invoke, or nullptr to clear.
   */
  void onBondStoreOverflow(BondStoreOverflowHandler handler);

  // --- Security Procedures ---

  /**
   * @brief Initiate the security/pairing procedure on an existing connection.
   * @param connHandle Connection handle to secure.
   * @return BTStatus indicating success or error.
   */
  BTStatus startSecurity(uint16_t connHandle);

  /**
   * @brief Block until authentication completes or the timeout elapses.
   * @param timeoutMs Maximum time to wait in milliseconds.
   * @return true if authentication completed (check the AuthCompleteHandler for success/failure),
   *         false if the timeout expired.
   */
  bool waitForAuthenticationComplete(uint32_t timeoutMs = 10000);

  /**
   * @brief Reset internal security state.
   */
  void resetSecurity();

  struct Impl;

private:
  explicit BLESecurity(std::shared_ptr<Impl> impl) : _impl(std::move(impl)) {}
  std::shared_ptr<Impl> _impl;

  void notifyAuthComplete(const BLEConnInfo &conn, bool success);
  bool notifyAuthorization(const BLEConnInfo &conn, uint16_t attrHandle, bool isRead);
  uint32_t resolvePasskeyForDisplay(const BLEConnInfo &conn);
  uint32_t resolvePasskeyForInput(const BLEConnInfo &conn);
  bool resolveNumericComparison(const BLEConnInfo &conn, uint32_t numcmp);
  bool notifyBondOverflow(const BTAddress &oldest);

  friend class BLEClass;
  friend class BLESecurityBackend;
};

#endif /* BLE_ENABLED */
