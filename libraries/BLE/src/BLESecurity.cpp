/*
 * BLESecurity.cpp
 *
 *  Created on: Dec 17, 2017
 *      Author: chegewara
 */

#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "BLESecurity.h"
#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)

BLESecurity::BLESecurity() {}

BLESecurity::~BLESecurity() {}
/*
 * @brief Set requested authentication mode
 */
void BLESecurity::setAuthenticationMode(esp_ble_auth_req_t auth_req) {
  m_authReq = auth_req;
  esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &m_authReq, sizeof(uint8_t));  // <--- setup requested authentication mode
}

/**
 * @brief 	Set our device IO capability to let end user perform authorization
 * 			either by displaying or entering generated 6-digits pin code
 */
void BLESecurity::setCapability(esp_ble_io_cap_t iocap) {
  m_iocap = iocap;
  esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
}  // setCapability

/**
 * @brief Init encryption key by server
 * @param key_size is value between 7 and 16
 */
void BLESecurity::setInitEncryptionKey(uint8_t init_key) {
  m_initKey = init_key;
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &m_initKey, sizeof(uint8_t));
}  // setInitEncryptionKey

/**
 * @brief Init encryption key by client
 * @param key_size is value between 7 and 16
 */
void BLESecurity::setRespEncryptionKey(uint8_t resp_key) {
  m_respKey = resp_key;
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &m_respKey, sizeof(uint8_t));
}  // setRespEncryptionKey

/**
 *
 *
 */
void BLESecurity::setKeySize(uint8_t key_size) {
  m_keySize = key_size;
  esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &m_keySize, sizeof(uint8_t));
}  //setKeySize

/**
 * Setup for static PIN connection, call it first and then call setAuthenticationMode eventually to change it
 */
void BLESecurity::setStaticPIN(uint32_t pin) {
  uint32_t passkey = pin;
  esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));
  setCapability(ESP_IO_CAP_OUT);
  setKeySize();
  setAuthenticationMode(ESP_LE_AUTH_REQ_SC_ONLY);
  setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
}

/**
 * @brief Debug function to display what keys are exchanged by peers
 */
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
}  // esp_key_type_to_str

#endif /* CONFIG_BLUEDROID_ENABLED */
#endif /* SOC_BLE_SUPPORTED */
