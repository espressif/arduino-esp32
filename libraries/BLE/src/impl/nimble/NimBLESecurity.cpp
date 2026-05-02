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

/**
 * @file NimBLESecurity.cpp
 * @brief BLE stack security configuration, bond store, and passkey handling (NimBLE / BLE_SMP_SUPPORTED).
 *
 * Spec references:
 *  - BT Core Spec v5.x, Vol 3, Part H (SMP — Security Manager Protocol) —
 *    the primary reference for all pairing and key distribution procedures.
 *  - §2.3     — Pairing methods: Just Works, Passkey Entry, Numeric Comparison, OOB.
 *  - §2.3.2   — I/O capability definitions (Table 2.7): DisplayOnly, DisplayYesNo,
 *               KeyboardOnly, NoInputNoOutput, KeyboardDisplay.
 *  - §2.3.3   — Association model selection based on combined I/O capabilities.
 *  - §2.3.5.2 — Passkey Entry protocol; passkey is a 6-digit decimal value (0–999999).
 *  - §3.5.1   — Pairing Request/Response; Authentication Requirements flags
 *               (Bonding, MITM, Secure Connections, Keypress, CT2).
 *  - §3.6.1   — Key Distribution/Generation fields (EncKey, IdKey, SignKey, LinkKey).
 *  - §2.4.1   — LE Secure Connections (LESC); required for Numeric Comparison with
 *               BLE 4.2+ peers.
 */

#include "impl/BLEGuards.h"
#if BLE_NIMBLE

#include "BLE.h"

#include "NimBLESecurity.h"
#include "impl/BLEImplHelpers.h"
#include "esp32-hal-log.h"

#if BLE_SMP_SUPPORTED

#include <esp_random.h>
#include <host/ble_gap.h>
#include <host/ble_store.h>

/**
 * @brief Pushes the current @c Impl SMP settings into the NimBLE host @c ble_hs_cfg.
 * @note IO capability values map to the SMP I/O Capability field
 *       (BT Core Spec v5.x, Vol 3, Part H, §3.5.1, Table 3.3) as follows:
 *       DisplayOnly=0, DisplayYesNo=1, KeyboardOnly=2, NoInputNoOutput=3, KeyboardDisplay=4.
 *       Key distribution bits: EncKey=0x01, IdKey=0x02, SignKey=0x04, LinkKey=0x08
 *       (Vol 3, Part H, §3.6.1, Table 3.8).
 */
void BLESecurity::Impl::applyToHost() const {
  ble_hs_cfg.sm_io_cap = static_cast<uint8_t>(ioCap);
  ble_hs_cfg.sm_bonding = bonding ? 1 : 0;
  ble_hs_cfg.sm_mitm = mitm ? 1 : 0;
  ble_hs_cfg.sm_sc = sc ? 1 : 0;
  ble_hs_cfg.sm_our_key_dist = initKeyDist;
  ble_hs_cfg.sm_their_key_dist = respKeyDist;
}

// --------------------------------------------------------------------------
// Stack-specific BLESecurity methods
// --------------------------------------------------------------------------

/**
 * @brief Configures the SMP I/O capability used for pairing and passkey/confirm flows.
 * @param cap I/O capability value applied to the NimBLE host.
 * @note The I/O capability determines which association model (Just Works, Passkey,
 *       Numeric Comparison, OOB) is used during SMP pairing.
 *       BT Core Spec v5.x, Vol 3, Part H, §2.3.2 (I/O Capabilities) and
 *       §2.3.3 (Mapping of I/O Capabilities to Key Generation Method).
 */
void BLESecurity::setIOCapability(IOCapability cap) {
  BLE_CHECK_IMPL();
  impl.ioCap = cap;
  impl.applyToHost();
}

/**
 * @brief Enables or disables bonding, MITM, and secure connections, then updates the host SMP config.
 * @param bonding When true, bonding is allowed (Bonding_Flags bits 0:1 in the
 *                AuthReq field = 0b01; BT Core Spec v5.x, Vol 3, Part H, §3.5.1, Table 3.5).
 * @param mitm When true, MITM protection is required
 *             (MITM bit 2 of the AuthReq field; Vol 3, Part H, §3.5.1).
 * @param secureConnection When true, LE Secure Connections are enabled
 *                         (SC bit 3 of the AuthReq field; Vol 3, Part H, §3.5.1).
 *                         Secure Connections use ECDH key exchange and are required
 *                         for Numeric Comparison (Vol 3, Part H, §2.4.1).
 */
void BLESecurity::setAuthenticationMode(bool bonding, bool mitm, bool secureConnection) {
  BLE_CHECK_IMPL();
  impl.bonding = bonding;
  impl.mitm = mitm;
  impl.sc = secureConnection;
  impl.applyToHost();
}

/**
 * @brief Sets a static 6-digit passkey or generates a random one, depending on @a isStatic.
 * @param isStatic When true, @a passkey is reduced modulo 1000000 and stored. When false, a random key is generated.
 * @param passkey Raw passkey value; only used when @a isStatic is true.
 * @note The passkey is a 6-digit decimal value in the range 0–999999.
 *       Values are reduced modulo 1,000,000 to enforce this constraint.
 *       BT Core Spec v5.x, Vol 3, Part H, §2.3.5.2 (Passkey Entry protocol).
 */
void BLESecurity::setPassKey(bool isStatic, uint32_t passkey) {
  BLE_CHECK_IMPL();
  impl.staticPassKey = isStatic;
  impl.passKey = isStatic ? (passkey % 1000000) : generateRandomPassKey();
}

/**
 * @brief Sets a fixed 6-digit passkey used when the stack requires a static value.
 * @param passkey Passkey reduced modulo 1000000 when applied.
 */
void BLESecurity::setStaticPassKey(uint32_t passkey) {
  setPassKey(true, passkey);
}

/**
 * @brief Generates and stores a new random 6-digit passkey for pairing.
 */
void BLESecurity::setRandomPassKey() {
  setPassKey(false);
}

/**
 * @brief Returns the current passkey held by the implementation, or 0 if no implementation exists.
 * @return The stored passkey, or 0.
 */
uint32_t BLESecurity::getPassKey() const {
  return _impl ? _impl->passKey : 0;
}

/**
 * @brief Sets the initiator (local) key distribution mask and reapplies the host SMP configuration.
 * @param keys Key distribution flags for the pairing initiator role.
 * @note Key distribution field bits: EncKey=0x01 (LTK+Rand+EDIV), IdKey=0x02 (IRK+BD_ADDR),
 *       SignKey=0x04 (CSRK), LinkKey=0x08 (BR/EDR link key derived from LTK).
 *       BT Core Spec v5.x, Vol 3, Part H, §3.6.1 (Table 3.8).
 */
void BLESecurity::setInitiatorKeys(KeyDist keys) {
  BLE_CHECK_IMPL();
  impl.initKeyDist = static_cast<uint8_t>(keys);
  impl.applyToHost();
}

/**
 * @brief Sets the responder (remote) key distribution mask and reapplies the host SMP configuration.
 * @param keys Key distribution flags for the pairing responder role.
 * @note Same bit definitions as for the initiator (see setInitiatorKeys).
 *       BT Core Spec v5.x, Vol 3, Part H, §3.6.1 (Table 3.8).
 */
void BLESecurity::setResponderKeys(KeyDist keys) {
  BLE_CHECK_IMPL();
  impl.respKeyDist = static_cast<uint8_t>(keys);
  impl.applyToHost();
}

/**
 * @brief Enumerates bonded peer addresses from the NimBLE store (bounded scan).
 * @return List of addresses read from the security database; empty if unimplemented or none found.
 */
std::vector<BTAddress> BLESecurity::getBondedDevices() const {
  std::vector<BTAddress> result;
  if (!_impl) {
    return result;
  }

  struct ble_store_key_sec key = {};
  struct ble_store_value_sec value = {};
  key.idx = 0;

  while (ble_store_read_peer_sec(&key, &value) == 0) {
    result.push_back(BTAddress(value.peer_addr.val, static_cast<BTAddress::Type>(value.peer_addr.type)));
    key.idx++;
    if (key.idx > 100) {
      break;
    }
  }
  return result;
}

/**
 * @brief Removes the bond and pairing data for a single address.
 * @param address Address of the peer to unpair.
 * @return Status indicating success, failure, or invalid state.
 */
BTStatus BLESecurity::deleteBond(const BTAddress &address) {
  if (!_impl) {
    log_w("Security: deleteBond called with no security impl");
    return BTStatus::InvalidState;
  }
  ble_addr_t addr;
  addr.type = static_cast<uint8_t>(address.type());
  memcpy(addr.val, address.data(), 6);
  int rc = ble_gap_unpair(&addr);
  if (rc != 0) {
    log_e("Security: deleteBond failed for %s rc=%d", address.toString().c_str(), rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

/**
 * @brief Clears the entire local bonding store.
 * @return @c OK on success, or @c InvalidState if no security implementation is present.
 */
BTStatus BLESecurity::deleteAllBonds() {
  if (!_impl) {
    log_w("Security: deleteAllBonds called with no security impl");
    return BTStatus::InvalidState;
  }
  ble_store_clear();
  return BTStatus::OK;
}

/**
 * @brief Starts the SMP security procedure on an existing connection.
 * @param connHandle NimBLE connection handle for the link.
 * @return Status from @c ble_gap_security_initiate, or @c InvalidState if unimplemented.
 */
BTStatus BLESecurity::startSecurity(uint16_t connHandle) {
  if (!_impl) {
    log_w("Security: startSecurity called with no security impl");
    return BTStatus::InvalidState;
  }
  int rc = ble_gap_security_initiate(connHandle);
  if (rc != 0) {
    log_e("Security: ble_gap_security_initiate handle=%u rc=%d", connHandle, rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

/**
 * @brief Resets SMP options to a conservative default and re-applies them to the host.
 */
void BLESecurity::resetSecurity() {
  BLE_CHECK_IMPL();
  impl.ioCap = NoInputNoOutput;
  impl.bonding = false;
  impl.mitm = false;
  impl.sc = true;
  impl.forceAuth = false;
  impl.applyToHost();
}

// --------------------------------------------------------------------------
// Stack event dispatch methods
// --------------------------------------------------------------------------

/**
 * @brief Supplies the passkey the stack should use or display, optionally regenerating it and invoking the display callback.
 * @param conn Connection information for the pairing peer.
 * @return Passkey value for the display step (0 if implementation is missing).
 */
uint32_t BLESecurity::resolvePasskeyForDisplay(const BLEConnInfo &conn) {
  BLE_CHECK_IMPL(0);
  uint32_t pk = impl.passKey;
  if (impl.regenOnConnect) {
    pk = generateRandomPassKey();
    impl.passKey = pk;
  }
  if (impl.passKeyDisplayCb) {
    impl.passKeyDisplayCb(conn, pk);
  }
  return pk;
}

/**
 * @brief Resolves the passkey for the input/entry role, preferring a user callback if set.
 * @param conn Connection information for the pairing peer.
 * @return User-entered or default passkey (0 if no implementation is present and checks fail).
 */
uint32_t BLESecurity::resolvePasskeyForInput(const BLEConnInfo &conn) {
  BLE_CHECK_IMPL(0);
  if (impl.passKeyRequestCb) {
    return impl.passKeyRequestCb(conn);
  }
  return impl.passKey;
}

// --------------------------------------------------------------------------
// BLEClass::getSecurity() factory
// --------------------------------------------------------------------------

/**
 * @brief Returns a @c BLESecurity facade backed by a process-wide shared NimBLE implementation, or an empty object if BLE is not initialized.
 * @return A @c BLESecurity instance, possibly default-constructed when BLE is not ready.
 */
BLESecurity BLEClass::getSecurity() {
  if (!isInitialized()) {
    return BLESecurity();
  }
  static std::shared_ptr<BLESecurity::Impl> secImpl;
  if (!secImpl) {
    secImpl = std::make_shared<BLESecurity::Impl>();
  }
  return BLESecurity(secImpl);
}

#else /* !BLE_SMP_SUPPORTED -- stubs */

// Stubs for BLE_SMP_SUPPORTED == 0: log where useful; return NotSupported, empty, or 0.

void BLESecurity::setIOCapability(IOCapability) {
  log_w("SMP not supported");
}

void BLESecurity::setAuthenticationMode(bool, bool, bool) {
  log_w("SMP not supported");
}

void BLESecurity::setPassKey(bool, uint32_t) {
  log_w("SMP not supported");
}

void BLESecurity::setStaticPassKey(uint32_t) {
  log_w("SMP not supported");
}

void BLESecurity::setRandomPassKey() {
  log_w("SMP not supported");
}

uint32_t BLESecurity::getPassKey() const {
  return 0;
}

void BLESecurity::setInitiatorKeys(KeyDist) {
  log_w("SMP not supported");
}

void BLESecurity::setResponderKeys(KeyDist) {
  log_w("SMP not supported");
}

std::vector<BTAddress> BLESecurity::getBondedDevices() const {
  log_w("SMP not supported");
  return {};
}

BTStatus BLESecurity::deleteBond(const BTAddress &) {
  log_w("SMP not supported");
  return BTStatus::NotSupported;
}

BTStatus BLESecurity::deleteAllBonds() {
  log_w("SMP not supported");
  return BTStatus::NotSupported;
}

BTStatus BLESecurity::startSecurity(uint16_t) {
  log_w("SMP not supported");
  return BTStatus::NotSupported;
}

void BLESecurity::resetSecurity() {
  log_w("SMP not supported");
}

uint32_t BLESecurity::resolvePasskeyForDisplay(const BLEConnInfo &) {
  return 0;
}

uint32_t BLESecurity::resolvePasskeyForInput(const BLEConnInfo &) {
  return 0;
}

BLESecurity BLEClass::getSecurity() {
  log_w("SMP not supported");
  return BLESecurity();
}

#endif /* BLE_SMP_SUPPORTED */

#endif /* BLE_NIMBLE */
