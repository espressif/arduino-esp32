/*
 * BLESecurity.h
 *
 *  Created on: Dec 17, 2017
 *      Author: chegewara
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on chegewara's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef COMPONENTS_CPP_UTILS_BLESECURITY_H_
#define COMPONENTS_CPP_UTILS_BLESECURITY_H_

#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                            Common includes                              *
 ***************************************************************************/

#include "WString.h"

/***************************************************************************
 *                           Bluedroid includes                            *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gap_ble_api.h>
#endif

/***************************************************************************
 *                             NimBLE includes                             *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_gap.h>
#endif

/***************************************************************************
 *                            Common definitions                           *
 ***************************************************************************/

#define BLE_SM_DEFAULT_PASSKEY 123456

/***************************************************************************
 *                          Forward declarations                           *
 ***************************************************************************/

class BLEServer;
class BLEClient;

/**
 * @brief Security management class
 */
class BLESecurity {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  BLESecurity();
  virtual ~BLESecurity();
  static void setAuthenticationMode(uint8_t auth_req);
  static void setCapability(uint8_t iocap);
  static void setInitEncryptionKey(uint8_t init_key);
  static void setRespEncryptionKey(uint8_t resp_key);
  static void setKeySize(uint8_t key_size = 16);
  static void setStaticPIN(uint32_t pin);

  /***************************************************************************
   *                       Bluedroid public declarations                     *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  static char *esp_key_type_to_str(esp_ble_key_type_t key_type);
#endif

  /***************************************************************************
   *                       NimBLE public declarations                        *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  static void setAuthenticationMode(bool bonding, bool mitm, bool sc);
  static bool startSecurity(uint16_t connHandle, int *rcPtr = nullptr);
#endif

private:
  friend class BLEServer;
  friend class BLEClient;

  /***************************************************************************
   *                       Common private properties                         *
   ***************************************************************************/

  static uint8_t m_iocap;
  static uint8_t m_authReq;
  static uint8_t m_initKey;
  static uint8_t m_respKey;
  static uint32_t m_passkey;

  /***************************************************************************
   *                       Bluedroid private properties                       *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  static uint8_t m_keySize;
#endif

};  // BLESecurity

/**
 * @brief Callbacks to handle GAP events related to authorization
 */
class BLESecurityCallbacks {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  virtual ~BLESecurityCallbacks(){};
  virtual uint32_t onPassKeyRequest() = 0;
  virtual void onPassKeyNotify(uint32_t pass_key) = 0;
  virtual bool onSecurityRequest() = 0;
  virtual bool onConfirmPIN(uint32_t pin) = 0;

  /***************************************************************************
   *                       Bluedroid public declarations                     *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  virtual void onAuthenticationComplete(esp_ble_auth_cmpl_t) = 0;
#endif

  /***************************************************************************
   *                       NimBLE public declarations                        *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  virtual void onAuthenticationComplete(ble_gap_conn_desc *) = 0;
#endif

};  // BLESecurityCallbacks

#endif  /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif  /* SOC_BLE_SUPPORTED */
#endif  // COMPONENTS_CPP_UTILS_BLESECURITY_H_
