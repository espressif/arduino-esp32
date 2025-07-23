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
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                         Common includes                                 *
 ***************************************************************************/

#include "BLESecurity.h"
#include "BLEUtils.h"
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

uint32_t BLESecurity::m_passkey = BLE_SM_DEFAULT_PASSKEY;

/***************************************************************************
 *                          Common functions                               *
 ***************************************************************************/

BLESecurity::BLESecurity() {}

BLESecurity::~BLESecurity() {}

void BLESecurity::setAuthenticationMode(uint8_t auth_req) {
  m_authReq = auth_req;
#if defined(CONFIG_BLUEDROID_ENABLED)
  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &m_authReq, sizeof(uint8_t));
#elif defined(CONFIG_NIMBLE_ENABLED)
  BLESecurity::setAuthenticationMode(
    (auth_req & BLE_SM_PAIR_AUTHREQ_BOND) != 0, (auth_req & BLE_SM_PAIR_AUTHREQ_MITM) != 0, (auth_req & BLE_SM_PAIR_AUTHREQ_SC) != 0
  );
#endif
}

void BLESecurity::setCapability(uint8_t iocap) {
  m_iocap = iocap;
#if defined(CONFIG_BLUEDROID_ENABLED)
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
#elif defined(CONFIG_NIMBLE_ENABLED)
  ble_hs_cfg.sm_io_cap = iocap;
#endif
}

void BLESecurity::setInitEncryptionKey(uint8_t init_key) {
  m_initKey = init_key;
#if defined(CONFIG_BLUEDROID_ENABLED)
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &m_initKey, sizeof(uint8_t));
#elif defined(CONFIG_NIMBLE_ENABLED)
  ble_hs_cfg.sm_our_key_dist = init_key;
#endif
}

void BLESecurity::setRespEncryptionKey(uint8_t resp_key) {
  m_respKey = resp_key;
#if defined(CONFIG_BLUEDROID_ENABLED)
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &m_respKey, sizeof(uint8_t));
#elif defined(CONFIG_NIMBLE_ENABLED)
  ble_hs_cfg.sm_their_key_dist = resp_key;
#endif
}

void BLESecurity::setKeySize(uint8_t key_size) {
#if defined(CONFIG_BLUEDROID_ENABLED)
  m_keySize = key_size;
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &m_keySize, sizeof(uint8_t));
#endif
}

void BLESecurity::setStaticPIN(uint32_t pin) {
  m_passkey = pin;
#if defined(CONFIG_BLUEDROID_ENABLED)
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &m_passkey, sizeof(uint32_t));
  setCapability(ESP_IO_CAP_OUT);
  setKeySize();
  setAuthenticationMode(ESP_LE_AUTH_REQ_SC_ONLY);
  setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
#elif defined(CONFIG_NIMBLE_ENABLED)
  setCapability(BLE_HS_IO_DISPLAY_ONLY);
  setKeySize();
  setAuthenticationMode(false, false, true);  // No bonding, no MITM, secure connection only
  setInitEncryptionKey(BLE_HS_KEY_DIST_ENC_KEY | BLE_HS_KEY_DIST_ID_KEY);
#endif
}

/***************************************************************************
 *                          Bluedroid functions                            *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
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
#endif

/***************************************************************************
 *                          NimBLE functions                               *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
void BLESecurity::setAuthenticationMode(bool bonding, bool mitm, bool sc) {
  m_authReq = bonding ? BLE_SM_PAIR_AUTHREQ_BOND : 0;
  m_authReq |= mitm ? BLE_SM_PAIR_AUTHREQ_MITM : 0;
  m_authReq |= sc ? BLE_SM_PAIR_AUTHREQ_SC : 0;
  ble_hs_cfg.sm_bonding = bonding;
  ble_hs_cfg.sm_mitm = mitm;
  ble_hs_cfg.sm_sc = sc;
}

bool BLESecurity::startSecurity(uint16_t connHandle, int *rcPtr) {
  int rc = ble_gap_security_initiate(connHandle);
  if (rc != 0) {
    log_e("ble_gap_security_initiate: rc=%d %s", rc, BLEUtils::returnCodeToString(rc));
  }
  if (rcPtr) {
    *rcPtr = rc;
  }
  return rc == 0 || rc == BLE_HS_EALREADY;
}
#endif

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED */
