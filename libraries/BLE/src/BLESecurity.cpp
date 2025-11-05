/*
 * BLESecurity.cpp
 *
 *  Created on: Dec 17, 2017
 *      Author: chegewara
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on chegewara's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                         Common includes                                 *
 ***************************************************************************/

#include "Arduino.h"
#include "BLESecurity.h"
#include "BLEUtils.h"
#include "BLEDevice.h"
#include "GeneralUtils.h"
#include "esp32-hal-log.h"

/***************************************************************************
 *                         NimBLE includes                                 *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_hs.h>
#endif

/***************************************************************************
 *                         Common properties                               *
 ***************************************************************************/

// If true, the security will be enforced on connection even if no security is needed
// TODO: Make this configurable without breaking Bluedroid/NimBLE compatibility
bool BLESecurity::m_forceSecurity = true;

bool BLESecurity::m_securityEnabled = false;
bool BLESecurity::m_securityStarted = false;
bool BLESecurity::m_passkeySet = false;
bool BLESecurity::m_staticPasskey = true;
bool BLESecurity::m_regenOnConnect = false;
bool BLESecurity::m_authenticationComplete = false;
uint8_t BLESecurity::m_iocap = ESP_IO_CAP_NONE;
uint8_t BLESecurity::m_authReq = 0;
uint8_t BLESecurity::m_initKey = 0;
uint8_t BLESecurity::m_respKey = 0;
uint32_t BLESecurity::m_passkey = BLE_SM_DEFAULT_PASSKEY;

/***************************************************************************
 *                         Bluedroid properties                            *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
uint8_t BLESecurity::m_keySize = 16;
esp_ble_sec_act_t BLESecurity::m_securityLevel;
FreeRTOS::Semaphore *BLESecurity::m_authCompleteSemaphore = nullptr;
#endif

/***************************************************************************
 *                          Common functions                               *
 ***************************************************************************/

// This function initializes the BLESecurity class.
BLESecurity::BLESecurity() {
  log_d("BLESecurity: Initializing");
  setKeySize();
  setInitEncryptionKey();
  setRespEncryptionKey();
  setCapability(ESP_IO_CAP_NONE);
}

// This function sets the authentication mode for the BLE security.
void BLESecurity::setAuthenticationMode(uint8_t auth_req) {
  log_d("setAuthenticationMode: auth_req=%d", auth_req);
  m_authReq = auth_req;
#if defined(CONFIG_BLUEDROID_ENABLED)
  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &m_authReq, sizeof(uint8_t));
#elif defined(CONFIG_NIMBLE_ENABLED)
  BLESecurity::setAuthenticationMode(
    (auth_req & BLE_SM_PAIR_AUTHREQ_BOND) != 0, (auth_req & BLE_SM_PAIR_AUTHREQ_MITM) != 0, (auth_req & BLE_SM_PAIR_AUTHREQ_SC) != 0
  );
#endif
}

// This function sets the Input/Output capability for the BLE security.
void BLESecurity::setCapability(uint8_t iocap) {
  log_d("setCapability: iocap=%d", iocap);
  m_iocap = iocap;
#if defined(CONFIG_BLUEDROID_ENABLED)
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
#elif defined(CONFIG_NIMBLE_ENABLED)
  ble_hs_cfg.sm_io_cap = iocap;
#endif
}

// This sets the initiator key distribution flags.
// ESP_BLE_ENC_KEY_MASK indicates that the device should distribute the Encryption Key to the peer device.
// ESP_BLE_ID_KEY_MASK indicates that the device should distribute the Identity Key to the peer device.
// Both are set by default.
void BLESecurity::setInitEncryptionKey(uint8_t init_key) {
  log_d("setInitEncryptionKey: init_key=%d", init_key);
  m_initKey = init_key;
#if defined(CONFIG_BLUEDROID_ENABLED)
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &m_initKey, sizeof(uint8_t));
#elif defined(CONFIG_NIMBLE_ENABLED)
  ble_hs_cfg.sm_our_key_dist = init_key;
#endif
}

// This sets the responder key distribution flags.
// ESP_BLE_ENC_KEY_MASK indicates that the device should distribute the Encryption Key to the peer device.
// ESP_BLE_ID_KEY_MASK indicates that the device should distribute the Identity Key to the peer device.
// Both are set by default.
void BLESecurity::setRespEncryptionKey(uint8_t resp_key) {
  log_d("setRespEncryptionKey: resp_key=%d", resp_key);
  m_respKey = resp_key;
#if defined(CONFIG_BLUEDROID_ENABLED)
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &m_respKey, sizeof(uint8_t));
#elif defined(CONFIG_NIMBLE_ENABLED)
  ble_hs_cfg.sm_their_key_dist = resp_key;
#endif
}

// This function sets the key size for the BLE security.
void BLESecurity::setKeySize(uint8_t key_size) {
#if defined(CONFIG_BLUEDROID_ENABLED)
  log_d("setKeySize: key_size=%d", key_size);
  m_keySize = key_size;
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &m_keySize, sizeof(uint8_t));
#endif
}

// This function generates a random passkey between 000000 and 999999.
uint32_t BLESecurity::generateRandomPassKey() {
  return random(0, 999999);
}

// This function sets a passkey for the BLE security.
// The first argument defines if the passkey is static or random.
// The second argument is the passkey (ignored when using a random passkey).
// The function returns the passkey that was set.
uint32_t BLESecurity::setPassKey(bool staticPasskey, uint32_t passkey) {
  log_d("setPassKey: staticPasskey=%d, passkey=%d", staticPasskey, passkey);
  m_staticPasskey = staticPasskey;

  if (m_staticPasskey) {
    m_passkey = passkey;
    if (m_passkey == BLE_SM_DEFAULT_PASSKEY) {
      log_w("*WARNING* Using default passkey: %06d", BLE_SM_DEFAULT_PASSKEY);
      log_w("*WARNING* Please use a random passkey or set a different static passkey");
    }
  } else {
    m_passkey = generateRandomPassKey();
  }

  m_passkeySet = true;

#if defined(CONFIG_BLUEDROID_ENABLED)
  // Workaround for making Bluedroid and NimBLE manage the random passkey similarly.
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &m_passkey, sizeof(uint32_t));
#endif

  return m_passkey;
}

// This function gets the passkey being used for the BLE security.
// If a static passkey is set, it will return the static passkey.
// If using a random passkey, it will generate a new random passkey if m_regenOnConnect is true.
// Otherwise, it will return the current passkey being used.
uint32_t BLESecurity::getPassKey() {
  if (m_passkeySet && !m_staticPasskey && m_regenOnConnect) {
    m_passkey = generateRandomPassKey();
#if defined(CONFIG_BLUEDROID_ENABLED)
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &m_passkey, sizeof(uint32_t));
#endif
  }
  return m_passkey;
}

// This function sets if the passkey should be regenerated on each connection.
void BLESecurity::regenPassKeyOnConnect(bool enable) {
  m_regenOnConnect = enable;
}

// This function resets the security state on disconnect.
void BLESecurity::resetSecurity() {
  log_d("resetSecurity: Resetting security state");
  m_securityStarted = false;
  m_authenticationComplete = false;
}

// This function sets the authentication mode with bonding, MITM, and secure connection options.
void BLESecurity::setAuthenticationMode(bool bonding, bool mitm, bool sc) {
  log_d("setAuthenticationMode: bonding=%d, mitm=%d, sc=%d", bonding, mitm, sc);
  m_authReq = bonding ? ESP_LE_AUTH_BOND : 0;
  m_authReq |= mitm ? ESP_LE_AUTH_REQ_MITM : 0;
  m_authReq |= sc ? ESP_LE_AUTH_REQ_SC_ONLY : 0;
  m_securityEnabled = (m_authReq != 0);
#if defined(CONFIG_BLUEDROID_ENABLED)
  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &m_authReq, sizeof(uint8_t));
  if (sc) {
    if (mitm) {
      setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
    } else {
      setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_NO_MITM);
    }
  }
#elif defined(CONFIG_NIMBLE_ENABLED)
  ble_hs_cfg.sm_bonding = bonding;
  ble_hs_cfg.sm_mitm = mitm;
  ble_hs_cfg.sm_sc = sc;
#endif
}

// This callback is called by the device that has Input capability when the peer device has Output capability
// It can also be called in NimBLE when there is no passkey set.
// It should return the passkey that the peer device is showing on its output.
// This might not be called if the client has a static passkey set.
uint32_t BLESecurityCallbacks::onPassKeyRequest() {
  Serial.println("BLESecurityCallbacks: *ATTENTION* Using insecure onPassKeyRequest.");
  Serial.println("BLESecurityCallbacks: *ATTENTION* Please implement onPassKeyRequest with a suitable passkey in your BLESecurityCallbacks class");
  Serial.printf("BLESecurityCallbacks: Default passkey: %06d\n", BLE_SM_DEFAULT_PASSKEY);
  return BLE_SM_DEFAULT_PASSKEY;
}

// This callback is called by the device that has Output capability when the peer device has Input capability
// It should display the passkey that will need to be entered on the peer device
void BLESecurityCallbacks::onPassKeyNotify(uint32_t passkey) {
  Serial.printf("BLESecurityCallbacks: Using default onPassKeyNotify. Passkey: %06lu\n", passkey);
}

// This callback is called when the peer device requests a secure connection.
// Usually the client accepts the server's security request.
// It should return true if the connection is accepted, false otherwise.
bool BLESecurityCallbacks::onSecurityRequest() {
  Serial.println("BLESecurityCallbacks: Using default onSecurityRequest. It will accept any security request.");
  return true;
}

// This callback is called by both devices when both have the DisplayYesNo capability.
// It should return true if both devices display the same passkey.
bool BLESecurityCallbacks::onConfirmPIN(uint32_t pin) {
  Serial.println("BLESecurityCallbacks: *ATTENTION* Using insecure onConfirmPIN. It will accept any passkey.");
  Serial.println("BLESecurityCallbacks: *ATTENTION* Please implement onConfirmPIN with a suitable confirmation logic in your BLESecurityCallbacks class");
  return true;
}

// This callback is called when the characteristic requires authorization.
// connHandle is the connection handle of the peer device.
// attrHandle is the handle of the characteristic.
// If isRead is true, the peer device is requesting to read the characteristic,
// otherwise it is requesting to write.
// It should return true if the authorization is granted, false otherwise.
bool BLESecurityCallbacks::onAuthorizationRequest(uint16_t connHandle, uint16_t attrHandle, bool isRead) {
  Serial.println("BLESecurityCallbacks: *ATTENTION* Using insecure onAuthorizationRequest. It will accept any authorization request.");
  Serial.println(
    "BLESecurityCallbacks: *ATTENTION* Please implement onAuthorizationRequest with a suitable authorization logic in your BLESecurityCallbacks class"
  );
  return true;
}

// This function waits for authentication to complete when bonding is enabled
// It prevents GATT operations from proceeding before pairing completes
void BLESecurity::waitForAuthenticationComplete(uint32_t timeoutMs) {
#if defined(CONFIG_BLUEDROID_ENABLED)
  // Only wait if bonding is enabled
  if ((m_authReq & ESP_LE_AUTH_BOND) == 0) {
    return;
  }

  // If already authenticated, no need to wait
  if (m_authenticationComplete) {
    return;
  }

  // Semaphore should have been created in startSecurity()
  if (m_authCompleteSemaphore == nullptr) {
    log_e("Authentication semaphore not initialized");
    return;
  }

  // Wait for authentication with timeout
  bool success = m_authCompleteSemaphore->timedWait("waitForAuthenticationComplete", timeoutMs / portTICK_PERIOD_MS);

  if (!success) {
    log_w("Timeout waiting for authentication to complete");
  }
#endif
}

// This function signals that authentication has completed
// Called from ESP_GAP_BLE_AUTH_CMPL_EVT handler
void BLESecurity::signalAuthenticationComplete() {
#if defined(CONFIG_BLUEDROID_ENABLED)
  m_authenticationComplete = true;

  // Signal waiting threads if semaphore exists
  if (m_authCompleteSemaphore != nullptr) {
    m_authCompleteSemaphore->give();
  }
#endif
}

/***************************************************************************
 *                          Bluedroid functions                            *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
// This function sets the encryption level that will be negotiated with peer device during connection
void BLESecurity::setEncryptionLevel(esp_ble_sec_act_t level) {
  m_securityLevel = level;
}

bool BLESecurity::startSecurity(esp_bd_addr_t bd_addr, int *rcPtr) {
#ifdef CONFIG_BLE_SMP_ENABLE
  log_d("startSecurity: bd_addr=%s", BLEAddress(bd_addr).toString().c_str());
  if (m_securityStarted) {
    log_w("Security already started for bd_addr=%s", BLEAddress(bd_addr).toString().c_str());
    if (rcPtr) {
      *rcPtr = ESP_OK;
    }
    return true;
  }

  if (m_securityEnabled) {
    // Initialize semaphore before starting security to avoid race condition
    if (m_authCompleteSemaphore == nullptr) {
      m_authCompleteSemaphore = new FreeRTOS::Semaphore("AuthComplete");
    }

    // Reset authentication complete flag when starting new security negotiation
    m_authenticationComplete = false;

    // Consume any pending semaphore signals from previous operations
    // This ensures the next wait will block until the new auth completes
    m_authCompleteSemaphore->take("startSecurity-reset");

    int rc = esp_ble_set_encryption(bd_addr, m_securityLevel);
    if (rc != ESP_OK) {
      log_e("esp_ble_set_encryption: rc=%d %s", rc, GeneralUtils::errorToString(rc));
    }
    if (rcPtr) {
      *rcPtr = rc;
    }
    m_securityStarted = (rc == ESP_OK);
  } else {
    log_e("Security is not enabled. Can't start security.");
    if (rcPtr) {
      *rcPtr = ESP_FAIL;
    }
    return false;
  }
  return m_securityStarted;
#else
  log_e("Bluedroid SMP is not enabled. Can't start security.");
  return false;
#endif
}

// This function converts an ESP BLE key type to a string representation.
char *BLESecurity::esp_key_type_to_str(esp_ble_key_type_t key_type) {
  char *key_str = nullptr;
  switch (key_type) {
    case ESP_LE_KEY_NONE:  key_str = (char *)"ESP_LE_KEY_NONE"; break;
    case ESP_LE_KEY_PENC:  key_str = (char *)"ESP_LE_KEY_PENC"; break;
    case ESP_LE_KEY_PID:   key_str = (char *)"ESP_LE_KEY_PID"; break;
    case ESP_LE_KEY_PCSRK: key_str = (char *)"ESP_LE_KEY_PCSRK"; break;
    case ESP_LE_KEY_PLK:   key_str = (char *)"ESP_LE_KEY_PLK"; break;
    case ESP_LE_KEY_LLK:   key_str = (char *)"ESP_LE_KEY_LLK"; break;
    case ESP_LE_KEY_LENC:  key_str = (char *)"ESP_LE_KEY_LENC"; break;
    case ESP_LE_KEY_LID:   key_str = (char *)"ESP_LE_KEY_LID"; break;
    case ESP_LE_KEY_LCSRK: key_str = (char *)"ESP_LE_KEY_LCSRK"; break;
    default:               key_str = (char *)"INVALID BLE KEY TYPE"; break;
  }
  return key_str;
}

// This function is called when authentication is complete.
void BLESecurityCallbacks::onAuthenticationComplete(esp_ble_auth_cmpl_t param) {
  bool success = param.success;
  Serial.printf("Using default onAuthenticationComplete. Authentication %s.\n", success ? "successful" : "failed");
}
#endif

/***************************************************************************
 *                          NimBLE functions                               *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
// This function initiates security for a given connection handle.
bool BLESecurity::startSecurity(uint16_t connHandle, int *rcPtr) {
  log_d("startSecurity: connHandle=%d", connHandle);
  if (m_securityStarted) {
    log_w("Security already started for connHandle=%d", connHandle);
    if (rcPtr) {
      *rcPtr = 0;
    }
    return true;
  }

  if (m_securityEnabled) {
    int rc = ble_gap_security_initiate(connHandle);
    if (rc != 0) {
      log_e("ble_gap_security_initiate: rc=%d %s", rc, BLEUtils::returnCodeToString(rc));
    }
    if (rcPtr) {
      *rcPtr = rc;
    }
    m_securityStarted = (rc == 0 || rc == BLE_HS_EALREADY);
  } else {
    log_e("Security is not enabled. Can't start security.");
    if (rcPtr) {
      *rcPtr = ESP_FAIL;
    }
    return false;
  }
  return m_securityStarted;
}

// This function is called when authentication is complete for NimBLE.
void BLESecurityCallbacks::onAuthenticationComplete(ble_gap_conn_desc *desc) {
  bool success = desc != nullptr;
  Serial.printf("Using default onAuthenticationComplete. Authentication %s.\n", success ? "successful" : "failed");
}
#endif

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
