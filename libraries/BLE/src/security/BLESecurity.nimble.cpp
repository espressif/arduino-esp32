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
 * @file BLESecurity.nimble.cpp
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

#include "core/BLEGuards.h"
#if BLE_NIMBLE

#include "BLE.h"

#include "security/BLESecurity.nimble.h"
#include "core/BLEImplHelpers.h"
#include "core/BLEMutex.h"
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
void BLESecurity::Impl::applySecurityParams() const {
  // ioCap/mitm come from the BLESecurityImplCommon base; bonding/sc/*KeyDist are NimBLE's.
  ble_hs_cfg.sm_io_cap = static_cast<uint8_t>(ioCap);
  ble_hs_cfg.sm_bonding = bonding ? 1 : 0;
  ble_hs_cfg.sm_mitm = mitm ? 1 : 0;
  ble_hs_cfg.sm_sc = sc ? 1 : 0;
  ble_hs_cfg.sm_our_key_dist = initKeyDist;
  ble_hs_cfg.sm_their_key_dist = respKeyDist;
}

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_WARN
/**
 * @brief Names an SMP pairing failure reason for logging.
 * @param reason sm_err reported by BLE_GAP_EVENT_PARING_COMPLETE.
 * @return Static description, or "unknown reason" for codes outside the spec range.
 * @note These are the Pairing Failed reason codes from BT Core Spec v5.x, Vol 3, Part H, §3.5.5.
 *       Unlike most NimBLE status values they are bare SMP codes, not namespaced BLE_HS_* ones.
 */
static const char *smpFailReasonName(int reason) {
  switch (reason) {
    case BLE_SM_ERR_PASSKEY:          return "passkey entry failed";
    case BLE_SM_ERR_OOB:              return "OOB data not available";
    case BLE_SM_ERR_AUTHREQ:          return "authentication requirements cannot be met";
    case BLE_SM_ERR_CONFIRM_MISMATCH: return "confirm value mismatch";
    case BLE_SM_ERR_PAIR_NOT_SUPP:    return "pairing not supported by peer";
    case BLE_SM_ERR_ENC_KEY_SZ:       return "encryption key size too short";
    case BLE_SM_ERR_CMD_NOT_SUPP:     return "SMP command not supported";
    case BLE_SM_ERR_UNSPECIFIED:      return "unspecified reason";
    case BLE_SM_ERR_REPEATED:         return "repeated attempts, pairing disallowed";
    case BLE_SM_ERR_INVAL:            return "invalid parameters";
    case BLE_SM_ERR_DHKEY:            return "DHKey check failed";
    case BLE_SM_ERR_NUMCMP:           return "numeric comparison mismatch";
    case BLE_SM_ERR_ALREADY:          return "BR/EDR pairing in progress";
    case BLE_SM_ERR_CROSS_TRANS:      return "cross-transport key derivation not allowed";
    case BLE_SM_ERR_KEY_REJ:          return "key rejected by peer";
    default:                          return "unknown reason";
  }
}
#endif

void BLESecurity::Impl::reportPairingComplete(uint16_t connHandle, int smReason) {
  if (smReason == BLE_SM_ERR_SUCCESS) {
    log_d("Security: pairing complete on conn %u", connHandle);
    return;
  }
  log_w("Security: pairing on conn %u failed: %s (SMP reason=0x%02x)", connHandle, smpFailReasonName(smReason), smReason);
}

void BLESecurity::Impl::deleteStaleBond(const ble_addr_t &peerIdAddr) {
  int rc = ble_store_util_delete_peer(&peerIdAddr);
  if (rc != 0) {
    log_e("Security: failed to delete the stale bond, rc=%d", rc);
    return;
  }
  log_w("Security: the peer no longer holds the bonding key, deleted the stale bond so the next attempt can pair again");
}

// --------------------------------------------------------------------------
// Stack-specific BLESecurity methods
// --------------------------------------------------------------------------

// API contract is documented on the declarations in the public BLE*.h headers; the definitions below carry implementation notes only.

void BLESecurity::setIOCapability(BLEIOCapability cap) {
  BLE_CHECK_IMPL();
  impl.ioCap = cap;
  impl.applySecurityParams();
}

void BLESecurity::setAuthenticationMode(bool bonding, bool mitm, bool secureConnection) {
  BLE_CHECK_IMPL();
  impl.bonding = bonding;
  impl.mitm = mitm;
  impl.sc = secureConnection;
  impl.applySecurityParams();
}

// A static passkey is reduced modulo 1000000 to enforce the 6-digit (0–999999) range.
void BLESecurity::setPassKey(bool isStatic, uint32_t passkey) {
  BLE_CHECK_IMPL();
  // passKey is read on the host task during pairing (resolvePasskeyForInput/
  // resolvePasskeyForDisplay, both under mtx), so the writer takes the same lock.
  BLELockGuard lock(impl.mtx);
  impl.passKeyIsStatic = isStatic;
  impl.passKey = isStatic ? (passkey % 1000000) : generateRandomPassKey();
}

void BLESecurity::setStaticPassKey(uint32_t passkey) {
  setPassKey(true, passkey);
}

void BLESecurity::setRandomPassKey() {
  setPassKey(false);
}

uint32_t BLESecurity::getPassKey() const {
  if (!_impl) {
    return 0;
  }
  BLELockGuard lock(_impl->mtx);
  return _impl->passKey;
}

void BLESecurity::setInitiatorKeys(KeyDist keys) {
  BLE_CHECK_IMPL();
  impl.initKeyDist = static_cast<uint8_t>(keys);
  impl.applySecurityParams();
}

void BLESecurity::setResponderKeys(KeyDist keys) {
  BLE_CHECK_IMPL();
  impl.respKeyDist = static_cast<uint8_t>(keys);
  impl.applySecurityParams();
}

// Bounded scan: stops after 100 stored bonds to avoid unbounded store iteration.
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

BTStatus BLESecurity::deleteAllBonds() {
  if (!_impl) {
    log_w("Security: deleteAllBonds called with no security impl");
    return BTStatus::InvalidState;
  }
  ble_store_clear();
  return BTStatus::OK;
}

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

void BLESecurity::resetSecurity() {
  BLE_CHECK_IMPL();
  impl.ioCap = BLEIOCapability::NoInputNoOutput;
  impl.bonding = true;
  impl.mitm = false;
  impl.sc = true;
  impl.forceAuth = false;
  impl.applySecurityParams();
}

// --------------------------------------------------------------------------
// Stack event dispatch methods
// --------------------------------------------------------------------------

uint32_t BLESecurity::Impl::resolvePasskeyForDisplay(const BLEConnInfo &conn) {
  // passKey/regenOnConnect/notifyPasskeyDisplay come from the BLESecurityImplCommon base.
  // Read/regenerate passKey under mtx (matching the setters and
  // resolvePasskeyForInput), then dispatch the display callback with the lock
  // released (notifyPasskeyDisplay snapshots its handler under mtx internally).
  uint32_t pk;
  {
    BLELockGuard lock(mtx);
    pk = passKey;
    if (regenOnConnect) {
      pk = BLESecurity::generateRandomPassKey();
      passKey = pk;
    }
  }
  notifyPasskeyDisplay(conn, pk);
  return pk;
}

#else /* !BLE_SMP_SUPPORTED -- stubs */

// Stubs for BLE_SMP_SUPPORTED == 0: log where useful; return NotSupported, empty, or 0.

void BLESecurity::setIOCapability(BLEIOCapability) {
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

uint32_t BLESecurity::Impl::resolvePasskeyForDisplay(const BLEConnInfo &) {
  return 0;
}

void BLESecurity::Impl::reportPairingComplete(uint16_t, int) {}

void BLESecurity::Impl::deleteStaleBond(const ble_addr_t &) {}

#endif /* BLE_SMP_SUPPORTED */

#endif /* BLE_NIMBLE */
