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

#include "impl/BLEGuards.h"
#if BLE_BLUEDROID

#if BLE_SMP_SUPPORTED

#include "BLE.h"

#include "BluedroidSecurity.h"
#include "impl/BLEImplHelpers.h"
#include "impl/BLESync.h"
#include "impl/BLEMutex.h"
#include "impl/BLEConnInfoData.h"
#include "esp32-hal-log.h"

#include <esp_gap_ble_api.h>
#include <esp_random.h>
#include <string.h>

BLESecurity::Impl *BLESecurity::Impl::s_instance = nullptr;

// --------------------------------------------------------------------------
// BLEConnInfoImpl -- Bluedroid bridge (security-side)
// --------------------------------------------------------------------------

struct BLEConnInfoImpl {
  /**
   * @brief Builds a minimal @c BLEConnInfo for a six-byte public address.
   * @param bda Bluetooth device address in wire order.
   * @return Populated @c BLEConnInfo with default link parameters and security flags cleared.
   */
  static BLEConnInfo make(const uint8_t bda[6]) {
    BLEConnInfo info;
    info._valid = true;
    auto *d = info.data();
    d->handle = 0;
    d->address = BTAddress(bda, BTAddress::Type::Public);
    d->mtu = 23;
    d->central = false;
    d->encrypted = false;
    d->authenticated = false;
    d->bonded = false;
    d->keySize = 0;
    d->interval = 0;
    d->latency = 0;
    d->supervisionTimeout = 0;
    d->txPhy = 1;
    d->rxPhy = 1;
    d->rssi = 0;
    return info;
  }

  /**
   * @brief Overwrites the encryption and bonding-related fields in @a info.
   * @param info Connection info to update in place.
   * @param encrypted Whether the link is encrypted.
   * @param authenticated Whether the peer is considered authenticated.
   * @param bonded Whether bonding is active for this result.
   */
  static void setAuthResult(BLEConnInfo &info, bool encrypted, bool authenticated, bool bonded) {
    auto *d = info.data();
    d->encrypted = encrypted;
    d->authenticated = authenticated;
    d->bonded = bonded;
  }
};

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
    case DisplayOnly:     espIoCap = ESP_IO_CAP_OUT; break;
    case DisplayYesNo:    espIoCap = ESP_IO_CAP_IO; break;
    case KeyboardOnly:    espIoCap = ESP_IO_CAP_IN; break;
    case KeyboardDisplay: espIoCap = ESP_IO_CAP_KBDISP; break;
    default:              espIoCap = ESP_IO_CAP_NONE; break;
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
  if (passKeySet) {
    err = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &staticPassKey, sizeof(uint32_t));
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
  Impl *sec = s_instance;
  if (!sec) {
    return;
  }

  switch (event) {
    case ESP_GAP_BLE_PASSKEY_REQ_EVT:
    {
      // Passkey Entry — input role: this device must supply the passkey
      // displayed on the peer.  The application provides it via the registered
      // PassKeyRequestHandler; a default is used when no handler is installed.
      // BT Core Spec v5.x, Vol 3, Part H, §2.3.5.2 (Passkey Entry protocol).
      BLEConnInfo conn = BLEConnInfoImpl::make(param->ble_security.ble_req.bd_addr);
      PassKeyRequestHandler requestCb;
      uint32_t pk;
      {
        BLELockGuard lock(sec->mtx);
        requestCb = sec->passKeyRequestCb;
        pk = sec->staticPassKey;
      }
      if (requestCb) {
        pk = requestCb(conn);
      }
      esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, pk);
      break;
    }

    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
    {
      // Passkey Entry — display role: this device must display the passkey
      // for the user to enter on the peer.
      // BT Core Spec v5.x, Vol 3, Part H, §2.3.5.2 (Passkey Entry protocol).
      BLEConnInfo conn = BLEConnInfoImpl::make(param->ble_security.key_notif.bd_addr);
      PassKeyDisplayHandler displayCb;
      {
        BLELockGuard lock(sec->mtx);
        displayCb = sec->passKeyDisplayCb;
      }
      if (displayCb) {
        displayCb(conn, param->ble_security.key_notif.passkey);
      }
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
      ConfirmPassKeyHandler confirmCb;
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
      SecurityRequestHandler secReqCb;
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
      BLEConnInfo conn = BLEConnInfoImpl::make(param->ble_security.auth_cmpl.bd_addr);
      bool success = param->ble_security.auth_cmpl.success;
      if (success) {
        bool authed = param->ble_security.auth_cmpl.auth_mode != ESP_LE_AUTH_NO_BOND;
        BLEConnInfoImpl::setAuthResult(conn, true, authed, authed);
      }

      AuthCompleteHandler authCb;
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

    default: break;
  }
}

// --------------------------------------------------------------------------
// Stack-specific BLESecurity methods
// --------------------------------------------------------------------------

/**
 * @brief Sets the SMP I/O capability for subsequent pairing and passes it to the Bluedroid stack.
 * @param cap I/O capability to apply.
 */
void BLESecurity::setIOCapability(IOCapability cap) {
  BLE_CHECK_IMPL();
  impl.ioCap = cap;
  impl.applySecurityParams();
}

/**
 * @brief Configures bonding, MITM, and secure-connection requirements, then re-applies stack parameters.
 * @param bond When true, bonding is enabled.
 * @param mitm When true, MITM protection is required when the stack allows it.
 * @param sc When true, only secure connection pairing is requested.
 */
void BLESecurity::setAuthenticationMode(bool bond, bool mitm, bool sc) {
  BLE_CHECK_IMPL();
  impl.bonding = bond;
  impl.mitm = mitm;
  impl.secureConnection = sc;
  impl.applySecurityParams();
}

/**
 * @brief Stores a static or managed passkey flag and value, then refreshes the stack security parameters.
 * @param isStatic When true, the provided value is a static passkey.
 * @param pk Passkey or placeholder value, depending on @a isStatic.
 */
void BLESecurity::setPassKey(bool isStatic, uint32_t pk) {
  BLE_CHECK_IMPL();
  impl.passKeySet = isStatic;
  impl.staticPassKey = pk;
  impl.applySecurityParams();
}

/**
 * @brief Sets a fixed static passkey and flags it for @c esp_ble_gap_set_security_param.
 * @param pk Static 6-digit passkey value to install.
 */
void BLESecurity::setStaticPassKey(uint32_t pk) {
  BLE_CHECK_IMPL();
  impl.staticPassKey = pk;
  impl.passKeySet = true;
  impl.applySecurityParams();
}

/**
 * @brief Generates a random passkey, marks it as static in the @c Impl, and re-applies stack security parameters.
 */
void BLESecurity::setRandomPassKey() {
  BLE_CHECK_IMPL();
  impl.staticPassKey = generateRandomPassKey();
  impl.passKeySet = true;
  impl.applySecurityParams();
}

/**
 * @brief Returns the static passkey stored in the implementation, or 0 if there is no implementation.
 * @return Current static passkey value, or 0.
 */
uint32_t BLESecurity::getPassKey() const {
  return _impl ? _impl->staticPassKey : 0;
}

/**
 * @brief Sets the initiator key-distribution field and re-applies Bluedroid SMP options.
 * @param keys Key distribution mask for the initiator.
 */
void BLESecurity::setInitiatorKeys(KeyDist keys) {
  BLE_CHECK_IMPL();
  impl.initiatorKeys = keys;
  impl.applySecurityParams();
}

/**
 * @brief Sets the responder key-distribution field and re-applies Bluedroid SMP options.
 * @param keys Key distribution mask for the responder.
 */
void BLESecurity::setResponderKeys(KeyDist keys) {
  BLE_CHECK_IMPL();
  impl.responderKeys = keys;
  impl.applySecurityParams();
}

/**
 * @brief Removes pairing data for one bonded device.
 * @param address Address of the device to remove.
 * @return @c OK on success, or an error code on failure.
 */
BTStatus BLESecurity::deleteBond(const BTAddress &address) {
  if (!_impl) {
    log_w("Security: deleteBond called with no security impl");
    return BTStatus::InvalidState;
  }
  esp_bd_addr_t bda;
  memcpy(bda, address.data(), 6);
  esp_err_t err = esp_ble_remove_bond_device(bda);
  if (err != ESP_OK) {
    log_e("Security: esp_ble_remove_bond_device for %s: %s", address.toString().c_str(), esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

/**
 * @brief Erases all bonded devices by listing them and calling @c esp_ble_remove_bond_device for each.
 * @return @c OK if all removals succeed or there was nothing to remove, or a failure status on critical errors.
 */
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

  bool allOk = true;
  for (int i = 0; i < num; i++) {
    if (esp_ble_remove_bond_device(devList[i].bd_addr) != ESP_OK) {
      log_w("Security: failed to remove bond device %d of %d", i + 1, num);
      allOk = false;
    }
  }
  return allOk ? BTStatus::OK : BTStatus::Fail;
}

/**
 * @brief Lists the addresses of all currently bonded devices from the Bluedroid bond list.
 * @return Vector of public addresses, possibly empty.
 */
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

/**
 * @brief Re-applies the configured security parameters to the stack. The connection handle is not used for this Bluedroid path.
 * @param connHandle Reserved for API parity; ignored.
 * @return @c OK after applying parameters, or @c InvalidState if no implementation.
 */
BTStatus BLESecurity::startSecurity(uint16_t /*connHandle*/) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  impl.applySecurityParams();
  return BTStatus::OK;
}

/**
 * @brief Restores a default @c Impl SMP profile and pushes it to the stack.
 */
void BLESecurity::resetSecurity() {
  BLE_CHECK_IMPL();
  impl.ioCap = NoInputNoOutput;
  impl.bonding = true;
  impl.mitm = false;
  impl.secureConnection = false;
  impl.forceAuth = false;
  impl.applySecurityParams();
}

/**
 * @brief Returns the passkey for a display passkey event, optionally regenerating a random one.
 * @param conn Reserved for future use; the current path does not use connection-specific state.
 * @return Passkey to use for display, or 0 on missing implementation.
 */
uint32_t BLESecurity::resolvePasskeyForDisplay(const BLEConnInfo &) {
  BLE_CHECK_IMPL(0);
  uint32_t pk = impl.staticPassKey;
  if (impl.regenOnConnect) {
    pk = generateRandomPassKey();
    impl.staticPassKey = pk;
  }
  return pk;
}

/**
 * @brief Resolves the passkey for a user input request using the request callback or static value.
 * @param conn Connection context (used when a callback is registered).
 * @return Passkey to send to the stack.
 */
uint32_t BLESecurity::resolvePasskeyForInput(const BLEConnInfo &conn) {
  BLE_CHECK_IMPL(0);
  return impl.passKeyRequestCb ? impl.passKeyRequestCb(conn) : impl.staticPassKey;
}

// --------------------------------------------------------------------------
// BLEClass::getSecurity() factory
// --------------------------------------------------------------------------

/**
 * @brief Returns a shared @c BLESecurity for the process, registering a singleton @c Impl with the GAP instance on first use.
 * @return @c BLESecurity with a valid @c _impl when BLE is initialized, otherwise a default object.
 */
BLESecurity BLEClass::getSecurity() {
  if (!isInitialized()) {
    return BLESecurity();
  }
  static std::shared_ptr<BLESecurity::Impl> secImpl;
  if (!secImpl) {
    secImpl = std::make_shared<BLESecurity::Impl>();
    BLESecurity::Impl::s_instance = secImpl.get();
    secImpl->applySecurityParams();
  }
  return BLESecurity(secImpl);
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

uint32_t BLESecurity::resolvePasskeyForDisplay(const BLEConnInfo &conn) {
  (void)conn;
  return 0;
}

uint32_t BLESecurity::resolvePasskeyForInput(const BLEConnInfo &conn) {
  (void)conn;
  return 0;
}

BLESecurity BLEClass::getSecurity() {
  log_w("SMP not supported");
  return BLESecurity();
}

#endif /* BLE_SMP_SUPPORTED */

#endif /* BLE_BLUEDROID */
