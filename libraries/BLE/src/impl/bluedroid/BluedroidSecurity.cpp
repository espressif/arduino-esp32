/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
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

/**
 * @file BluedroidSecurity.cpp
 * @brief BLE stack security parameters, GAP security events, and bond management (Bluedroid / BLE_SMP_SUPPORTED).
 *
 * Spec references:
 *  - BT Core Spec v5.x, Vol 3, Part H (SMP — Security Manager Protocol).
 *  - §2.3.2   — I/O capability definitions: DisplayOnly=0, DisplayYesNo=1,
 *               KeyboardOnly=2, NoInputNoOutput=3 (None), KeyboardDisplay=4.
 *  - §2.3.3   — Association model selection table based on combined IO capabilities.
 *  - §2.3.5.2 — Passkey Entry: passkey is 6-digit decimal (0–999 999).
 *  - §2.3.5.6 — Numeric Comparison: both devices display and the user must confirm.
 *  - §3.5.1   — Pairing Request/Response Authentication Requirements flags:
 *               Bonding_Flags [1:0], MITM [2], SC [3], Keypress [4], CT2 [5].
 *  - §3.6.1   — Key Distribution/Generation field bits:
 *               EncKey=0x01, IdKey=0x02, SignKey=0x04, LinkKey=0x08.
 */

#include "impl/common/BLEGuards.h"
#if BLE_BLUEDROID

#if BLE_SMP_SUPPORTED

#include "BLE.h"

#include "BluedroidSecurity.h"
#include "impl/common/BLEImplHelpers.h"
#include "impl/common/BLESync.h"
#include "impl/common/BLEMutex.h"
#include "impl/common/BLEConnInfoData.h"
#include "BluedroidConnInfo.h"
#include "esp32-hal-log.h"

#include <esp_gap_ble_api.h>
#include <esp_random.h>
#include <string.h>

// BLESecurityImplCommon::s_instance is defined once in BLESecurity.cpp (shared base).

// API contract is documented on the declarations in the public BLE*.h headers; the definitions below carry implementation notes only.

/**
 * @brief Configures the Bluedroid stack SMP parameters to match the current @c Impl fields.
 * @note IO capability values map to SMP Pairing Request/Response IO_Capability field
 *       (BT Core Spec v5.x, Vol 3, Part H, §2.3.2, Table 2.7):
 *       DisplayOnly (spec 0x00 → ESP_IO_CAP_OUT), DisplayYesNo (0x01 → ESP_IO_CAP_IO),
 *       KeyboardOnly (0x02 → ESP_IO_CAP_IN), NoInputNoOutput (0x03 → ESP_IO_CAP_NONE),
 *       KeyboardDisplay (0x04 → ESP_IO_CAP_KBDISP).
 *       Authentication Requirements flags (Vol 3, Part H, §3.5.1):
 *       Bonding_Flags=ESP_LE_AUTH_BOND, MITM=ESP_LE_AUTH_REQ_MITM,
 *       Secure Connections=ESP_LE_AUTH_REQ_SC_ONLY.
 *       Key distribution bits (Vol 3, Part H, §3.6.1):
 *       EncKey=ESP_BLE_ENC_KEY_MASK, IdKey=ESP_BLE_ID_KEY_MASK,
 *       SignKey=ESP_BLE_CSR_KEY_MASK, LinkKey=ESP_BLE_LINK_KEY_MASK.
 */
void BLESecurity::Impl::applySecurityParams() {
  // mitm/ioCap/passKeyIsStatic/passKey/keySize come from the BLESecurityImplCommon base;
  // bonding/secureConnection/*Keys are Bluedroid's.

  esp_ble_auth_req_t authReq = ESP_LE_AUTH_NO_BOND;
  if (bonding) {
    authReq = static_cast<esp_ble_auth_req_t>(authReq | ESP_LE_AUTH_BOND);
  }
  if (mitm) {
    authReq = static_cast<esp_ble_auth_req_t>(authReq | ESP_LE_AUTH_REQ_MITM);
  }
  if (secureConnection) {
    authReq = static_cast<esp_ble_auth_req_t>(authReq | ESP_LE_AUTH_REQ_SC_ONLY);
  }

  esp_ble_io_cap_t espIoCap;
  switch (ioCap) {
    case BLESecurity::DisplayOnly:     espIoCap = ESP_IO_CAP_OUT; break;
    case BLESecurity::DisplayYesNo:    espIoCap = ESP_IO_CAP_IO; break;
    case BLESecurity::KeyboardOnly:    espIoCap = ESP_IO_CAP_IN; break;
    case BLESecurity::KeyboardDisplay: espIoCap = ESP_IO_CAP_KBDISP; break;
    default:                           espIoCap = ESP_IO_CAP_NONE; break;
  }

  uint8_t initKeyDist = 0;
  if (static_cast<uint8_t>(initiatorKeys) & static_cast<uint8_t>(KeyDist::EncKey)) {
    initKeyDist |= ESP_BLE_ENC_KEY_MASK;
  }
  if (static_cast<uint8_t>(initiatorKeys) & static_cast<uint8_t>(KeyDist::IdKey)) {
    initKeyDist |= ESP_BLE_ID_KEY_MASK;
  }
  if (static_cast<uint8_t>(initiatorKeys) & static_cast<uint8_t>(KeyDist::SignKey)) {
    initKeyDist |= ESP_BLE_CSR_KEY_MASK;
  }
  if (static_cast<uint8_t>(initiatorKeys) & static_cast<uint8_t>(KeyDist::LinkKey)) {
    initKeyDist |= ESP_BLE_LINK_KEY_MASK;
  }

  uint8_t rspKeyDist = 0;
  if (static_cast<uint8_t>(responderKeys) & static_cast<uint8_t>(KeyDist::EncKey)) {
    rspKeyDist |= ESP_BLE_ENC_KEY_MASK;
  }
  if (static_cast<uint8_t>(responderKeys) & static_cast<uint8_t>(KeyDist::IdKey)) {
    rspKeyDist |= ESP_BLE_ID_KEY_MASK;
  }
  if (static_cast<uint8_t>(responderKeys) & static_cast<uint8_t>(KeyDist::SignKey)) {
    rspKeyDist |= ESP_BLE_CSR_KEY_MASK;
  }
  if (static_cast<uint8_t>(responderKeys) & static_cast<uint8_t>(KeyDist::LinkKey)) {
    rspKeyDist |= ESP_BLE_LINK_KEY_MASK;
  }

  esp_err_t err;
  if (passKeyIsStatic) {
    err = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passKey, sizeof(uint32_t));
    if (err != ESP_OK) {
      log_e("esp_ble_gap_set_security_param(STATIC_PASSKEY): %s", esp_err_to_name(err));
    }
  }

  err = esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &authReq, sizeof(authReq));
  if (err != ESP_OK) {
    log_e("esp_ble_gap_set_security_param(AUTHEN_REQ_MODE): %s", esp_err_to_name(err));
  }
  err = esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &espIoCap, sizeof(espIoCap));
  if (err != ESP_OK) {
    log_e("esp_ble_gap_set_security_param(IOCAP_MODE): %s", esp_err_to_name(err));
  }
  err = esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &keySize, sizeof(keySize));
  if (err != ESP_OK) {
    log_e("esp_ble_gap_set_security_param(MAX_KEY_SIZE): %s", esp_err_to_name(err));
  }
  err = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &initKeyDist, sizeof(initKeyDist));
  if (err != ESP_OK) {
    log_e("esp_ble_gap_set_security_param(SET_INIT_KEY): %s", esp_err_to_name(err));
  }
  err = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rspKeyDist, sizeof(rspKeyDist));
  if (err != ESP_OK) {
    log_e("esp_ble_gap_set_security_param(SET_RSP_KEY): %s", esp_err_to_name(err));
  }
}

/**
 * @brief Dispatches Bluedroid GAP security events to the active @c Impl instance (passkey, confirm, security request, auth complete).
 * @param event GAP event identifier.
 * @param param Event parameters from the Bluedroid callback; must match @a event.
 */
void BLESecurity::Impl::handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  BLESecurity::Impl *sec = BLESecurity::Impl::instance();
  if (!sec) {
    return;
  }

  switch (event) {
    case ESP_GAP_BLE_PASSKEY_REQ_EVT:
    {
      // Passkey Entry — input role: this device must supply the passkey
      // displayed on the peer.  Shared resolver: PassKeyRequestHandler result, else
      // the configured passkey.
      // BT Core Spec v5.x, Vol 3, Part H, §2.3.5.2 (Passkey Entry protocol).
      BLEConnInfo conn = BLEConnInfoImpl::make(param->ble_security.ble_req.bd_addr);
      esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, sec->resolvePasskeyForInput(conn));
      break;
    }

    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
    {
      // Passkey Entry — display role: display the passkey the Bluedroid stack
      // generated so the user can enter it on the peer. Unlike NimBLE, the value
      // comes from the stack event, not from us; only the callback dispatch is shared.
      // BT Core Spec v5.x, Vol 3, Part H, §2.3.5.2 (Passkey Entry protocol).
      BLEConnInfo conn = BLEConnInfoImpl::make(param->ble_security.key_notif.bd_addr);
      sec->notifyPasskeyDisplay(conn, param->ble_security.key_notif.passkey);
      break;
    }

    case ESP_GAP_BLE_NC_REQ_EVT:
    {
      // Numeric Comparison: both devices display a 6-digit value; the user
      // must confirm that the values match on both devices before authentication
      // proceeds.  Without a callback the pairing is auto-accepted (equivalent
      // to Just Works, no MitM protection).
      // BT Core Spec v5.x, Vol 3, Part H, §2.3.5.6 (Numeric Comparison protocol).
      BLEConnInfo conn = BLEConnInfoImpl::make(param->ble_security.key_notif.bd_addr);
      BLESecurity::ConfirmPassKeyHandler confirmCb;
      {
        BLELockGuard lock(sec->mtx);
        confirmCb = sec->confirmPassKeyCb;
      }
      bool accept = true;
      if (confirmCb) {
        accept = confirmCb(conn, param->ble_security.key_notif.passkey);
      }
      esp_ble_confirm_reply(param->ble_security.key_notif.bd_addr, accept);
      break;
    }

    case ESP_GAP_BLE_SEC_REQ_EVT:
    {
      // Security Request from the peer (peripheral requesting encryption).
      // The central may accept or reject; accepting triggers pairing/encryption.
      // BT Core Spec v5.x, Vol 3, Part H, §3.7.1 (Security Request PDU).
      BLEConnInfo conn = BLEConnInfoImpl::make(param->ble_security.ble_req.bd_addr);
      BLESecurity::SecurityRequestHandler secReqCb;
      {
        BLELockGuard lock(sec->mtx);
        secReqCb = sec->securityRequestCb;
      }
      bool accept = true;
      if (secReqCb) {
        accept = secReqCb(conn);
      }
      esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, accept);
      break;
    }

    case ESP_GAP_BLE_AUTH_CMPL_EVT:
    {
      // Pairing/authentication complete.  auth_cmpl.success indicates whether
      // the SMP procedure succeeded or failed (e.g. passkey mismatch, timeout).
      // BT Core Spec v5.x, Vol 3, Part H, §3.5 (Pairing Confirm/Random/DHKey).
      BLEConnInfo conn = BLEConnInfoImpl::make(
        param->ble_security.auth_cmpl.bd_addr, static_cast<BTAddress::Type>(param->ble_security.auth_cmpl.addr_type)
      );
      bool success = param->ble_security.auth_cmpl.success;
      if (success) {
        BLEConnInfoImpl::updateSecurityFromAuthComplete(conn, param->ble_security.auth_cmpl.auth_mode);
      }

      BLESecurity::AuthCompleteHandler authCb;
      {
        BLELockGuard lock(sec->mtx);
        authCb = sec->authCompleteCb;
      }
      if (authCb) {
        authCb(conn, success);
      }
      sec->authSync.give(success ? BTStatus::OK : BTStatus::AuthFailed);
      break;
    }

    case ESP_GAP_BLE_KEY_EVT:
    {
      log_d("BLE key type: %d", param->ble_security.ble_key.key_type);
      break;
    }

    case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT:
    {
      // Completes the blocking deleteBond / deleteAllBonds wait. The store
      // update is finished by the time this event fires, so getBondedDevices()
      // reflects the post-delete state after wait() returns.
      BTStatus st = (param->remove_bond_dev_cmpl.status == ESP_BT_STATUS_SUCCESS) ? BTStatus::OK : BTStatus::Fail;
      if (st != BTStatus::OK) {
        log_w(
          "Security: remove bond complete status=%d addr=" ESP_BD_ADDR_STR, param->remove_bond_dev_cmpl.status,
          ESP_BD_ADDR_HEX(param->remove_bond_dev_cmpl.bd_addr)
        );
      }
      sec->bondSync.give(st);
      break;
    }

    default: break;
  }
}

// --------------------------------------------------------------------------
// Stack-specific BLESecurity methods
// --------------------------------------------------------------------------

void BLESecurity::setIOCapability(IOCapability cap) {
  BLE_CHECK_IMPL();
  // Config fields are read on the host task during pairing (guarded by mtx there),
  // so the writers take the same lock. applySecurityParams runs under the recursive
  // mtx; it only pushes to the native stack (no user callback).
  BLELockGuard lock(impl.mtx);
  impl.ioCap = cap;
  impl.applySecurityParams();
}

void BLESecurity::setAuthenticationMode(bool bond, bool mitm, bool sc) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.bonding = bond;
  impl.mitm = mitm;
  impl.secureConnection = sc;
  impl.applySecurityParams();
}

void BLESecurity::setPassKey(bool isStatic, uint32_t pk) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.passKeyIsStatic = isStatic;
  impl.passKey = pk;
  impl.applySecurityParams();
}

void BLESecurity::setStaticPassKey(uint32_t pk) {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.passKey = pk;
  impl.passKeyIsStatic = true;
  impl.applySecurityParams();
}

void BLESecurity::setRandomPassKey() {
  BLE_CHECK_IMPL();
  BLELockGuard lock(impl.mtx);
  impl.passKey = generateRandomPassKey();
  impl.passKeyIsStatic = true;
  impl.applySecurityParams();
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
  impl.initiatorKeys = keys;
  impl.applySecurityParams();
}

void BLESecurity::setResponderKeys(KeyDist keys) {
  BLE_CHECK_IMPL();
  impl.responderKeys = keys;
  impl.applySecurityParams();
}

BTStatus BLESecurity::deleteBond(const BTAddress &address) {
  if (!_impl) {
    log_w("Security: deleteBond called with no security impl");
    return BTStatus::InvalidState;
  }
  esp_bd_addr_t bda;
  address.toEspBdAddr(bda);
  // esp_ble_remove_bond_device posts work to the BTC task and completes via
  // ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT — wait so the public API is
  // synchronous (matching NimBLE's ble_gap_unpair).
  _impl->bondSync.take();
  esp_err_t err = esp_ble_remove_bond_device(bda);
  if (err != ESP_OK) {
    log_e("Security: esp_ble_remove_bond_device for %s: %s", address.toString().c_str(), esp_err_to_name(err));
    _impl->bondSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  BTStatus st = _impl->bondSync.wait(5000);
  if (st == BTStatus::Timeout) {
    log_e("Security: deleteBond timed out waiting for remove-complete for %s", address.toString().c_str());
  }
  return st;
}

BTStatus BLESecurity::deleteAllBonds() {
  if (!_impl) {
    log_w("Security: deleteAllBonds called with no security impl");
    return BTStatus::InvalidState;
  }

  int num = esp_ble_get_bond_device_num();
  if (num <= 0) {
    return BTStatus::OK;
  }

  std::vector<esp_ble_bond_dev_t> devList(num);
  esp_err_t err = esp_ble_get_bond_device_list(&num, devList.data());
  if (err != ESP_OK) {
    log_e("Security: esp_ble_get_bond_device_list: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }

  // Remove one-by-one and wait for each REMOVE_BOND_DEV_COMPLETE_EVT so the
  // store is empty by the time this returns (NimBLE ble_store_clear is sync).
  bool allOk = true;
  for (int i = 0; i < num; i++) {
    _impl->bondSync.take();
    esp_err_t remErr = esp_ble_remove_bond_device(devList[i].bd_addr);
    if (remErr != ESP_OK) {
      log_w("Security: failed to remove bond device %d of %d: %s", i + 1, num, esp_err_to_name(remErr));
      _impl->bondSync.give(BTStatus::Fail);
      allOk = false;
      continue;
    }
    BTStatus st = _impl->bondSync.wait(5000);
    if (st != BTStatus::OK) {
      log_w("Security: remove bond device %d of %d did not complete: %s", i + 1, num, st.toString());
      allOk = false;
    }
  }
  return allOk ? BTStatus::OK : BTStatus::Fail;
}

std::vector<BTAddress> BLESecurity::getBondedDevices() const {
  std::vector<BTAddress> result;
  if (!_impl) {
    return result;
  }

  int num = esp_ble_get_bond_device_num();
  if (num <= 0) {
    return result;
  }

  std::vector<esp_ble_bond_dev_t> devList(num);
  esp_err_t err = esp_ble_get_bond_device_list(&num, devList.data());
  if (err != ESP_OK) {
    return result;
  }

  for (int i = 0; i < num; i++) {
    result.push_back(BTAddress(devList[i].bd_addr, BTAddress::Type::Public));
  }
  return result;
}

// connHandle is unused on the Bluedroid path; security parameters are applied globally to the stack rather than per connection.
BTStatus BLESecurity::startSecurity(uint16_t /*connHandle*/) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  impl.applySecurityParams();
  return BTStatus::OK;
}

void BLESecurity::resetSecurity() {
  BLE_CHECK_IMPL();
  impl.ioCap = NoInputNoOutput;
  impl.bonding = true;
  impl.mitm = false;
  impl.secureConnection = true;
  impl.forceAuth = false;
  impl.applySecurityParams();
}

// --------------------------------------------------------------------------
// Free function for GAP security event routing from BluedroidCore.cpp
// --------------------------------------------------------------------------

/**
 * @brief Forwards a Bluedroid GAP security event to @c BLESecurity::Impl::handleGAP.
 * @param event GAP event code.
 * @param param Event payload from the Bluedroid stack.
 */
void bluedroidSecurityHandleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  BLESecurity::Impl::handleGAP(event, param);
}

#else /* !BLE_SMP_SUPPORTED -- stubs */

#include "BLE.h"
#include "esp32-hal-log.h"

// Stubs for BLE_SMP_SUPPORTED == 0: log where useful; return NotSupported, empty, or 0.

void BLESecurity::setIOCapability(IOCapability cap) {
  (void)cap;
  log_w("SMP not supported");
}

void BLESecurity::setAuthenticationMode(bool bond, bool mitm, bool sc) {
  (void)bond;
  (void)mitm;
  (void)sc;
  log_w("SMP not supported");
}

void BLESecurity::setPassKey(bool isStatic, uint32_t pk) {
  (void)isStatic;
  (void)pk;
  log_w("SMP not supported");
}

void BLESecurity::setStaticPassKey(uint32_t pk) {
  (void)pk;
  log_w("SMP not supported");
}

void BLESecurity::setRandomPassKey() {
  log_w("SMP not supported");
}

uint32_t BLESecurity::getPassKey() const {
  return 0;
}

void BLESecurity::setInitiatorKeys(KeyDist keys) {
  (void)keys;
  log_w("SMP not supported");
}

void BLESecurity::setResponderKeys(KeyDist keys) {
  (void)keys;
  log_w("SMP not supported");
}

BTStatus BLESecurity::deleteBond(const BTAddress &address) {
  (void)address;
  log_w("SMP not supported");
  return BTStatus::NotSupported;
}

BTStatus BLESecurity::deleteAllBonds() {
  log_w("SMP not supported");
  return BTStatus::NotSupported;
}

std::vector<BTAddress> BLESecurity::getBondedDevices() const {
  return {};
}

BTStatus BLESecurity::startSecurity(uint16_t connHandle) {
  (void)connHandle;
  log_w("SMP not supported");
  return BTStatus::NotSupported;
}

void BLESecurity::resetSecurity() {
  log_w("SMP not supported");
}

#endif /* BLE_SMP_SUPPORTED */

#endif /* BLE_BLUEDROID */
