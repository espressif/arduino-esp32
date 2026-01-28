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
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                            Common includes                              *
 ***************************************************************************/

#include "WString.h"
#include "BLEDevice.h"
#include "BLEClient.h"
#include "BLEServer.h"
#include "RTOS.h"

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
 *                          Common definitions                             *
 ***************************************************************************/

#define BLE_SM_DEFAULT_PASSKEY 123456

/***************************************************************************
 *                          NimBLE definitions                             *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
// Compatibility with Bluedroid definitions

#define ESP_IO_CAP_OUT    BLE_HS_IO_DISPLAY_ONLY
#define ESP_IO_CAP_IO     BLE_HS_IO_DISPLAY_YESNO
#define ESP_IO_CAP_IN     BLE_HS_IO_KEYBOARD_ONLY
#define ESP_IO_CAP_NONE   BLE_HS_IO_NO_INPUT_OUTPUT
#define ESP_IO_CAP_KBDISP BLE_HS_IO_KEYBOARD_DISPLAY

#define ESP_LE_AUTH_NO_BOND          0x00
#define ESP_LE_AUTH_BOND             BLE_SM_PAIR_AUTHREQ_BOND
#define ESP_LE_AUTH_REQ_MITM         BLE_SM_PAIR_AUTHREQ_MITM
#define ESP_LE_AUTH_REQ_SC_ONLY      BLE_SM_PAIR_AUTHREQ_SC
#define ESP_LE_AUTH_REQ_BOND_MITM    (BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM)
#define ESP_LE_AUTH_REQ_SC_BOND      (BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_SC)
#define ESP_LE_AUTH_REQ_SC_MITM      (BLE_SM_PAIR_AUTHREQ_MITM | BLE_SM_PAIR_AUTHREQ_SC)
#define ESP_LE_AUTH_REQ_SC_MITM_BOND (BLE_SM_PAIR_AUTHREQ_MITM | BLE_SM_PAIR_AUTHREQ_SC | BLE_SM_PAIR_AUTHREQ_BOND)

#define ESP_BLE_ENC_KEY_MASK BLE_HS_KEY_DIST_ENC_KEY
#define ESP_BLE_ID_KEY_MASK  BLE_HS_KEY_DIST_ID_KEY
#endif

/***************************************************************************
 *                          Forward declarations                           *
 ***************************************************************************/

class BLEDevice;
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
  virtual ~BLESecurity() = default;
  static void setAuthenticationMode(uint8_t auth_req);
  static void setCapability(uint8_t iocap);
  static void setInitEncryptionKey(uint8_t init_key = (ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK));
  static void setRespEncryptionKey(uint8_t resp_key = (ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK));
  static void setKeySize(uint8_t key_size = 16);
  static uint32_t setPassKey(bool staticPasskey = false, uint32_t passkey = BLE_SM_DEFAULT_PASSKEY);
  static void setAuthenticationMode(bool bonding, bool mitm, bool sc);
  static uint32_t getPassKey();
  static uint32_t generateRandomPassKey();
  static void regenPassKeyOnConnect(bool enable = false);
  static void resetSecurity();
  static void waitForAuthenticationComplete(uint32_t timeoutMs = 10000);
  static void signalAuthenticationComplete();

  /***************************************************************************
   *                       Bluedroid public declarations                     *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  static char *esp_key_type_to_str(esp_ble_key_type_t key_type);
  static void setEncryptionLevel(esp_ble_sec_act_t level);
  static bool startSecurity(esp_bd_addr_t bd_addr, int *rcPtr = nullptr);
#endif

  /***************************************************************************
   *                       NimBLE public declarations                        *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  static bool startSecurity(uint16_t connHandle, int *rcPtr = nullptr);
#endif

private:
  friend class BLEDevice;
  friend class BLEServer;
  friend class BLEClient;
  friend class BLERemoteCharacteristic;
  friend class BLERemoteDescriptor;

  /***************************************************************************
   *                       Common private properties                         *
   ***************************************************************************/

  static bool m_securityEnabled;
  static bool m_securityStarted;
  static bool m_forceSecurity;
  static bool m_passkeySet;
  static bool m_staticPasskey;
  static bool m_regenOnConnect;
  static bool m_authenticationComplete;
  static uint8_t m_iocap;
  static uint8_t m_authReq;
  static uint8_t m_initKey;
  static uint8_t m_respKey;
  static uint32_t m_passkey;

  /***************************************************************************
   *                       Bluedroid private properties                      *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  static uint8_t m_keySize;
  static esp_ble_sec_act_t m_securityLevel;
  static class FreeRTOS::Semaphore *m_authCompleteSemaphore;
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

  BLESecurityCallbacks() = default;
  virtual ~BLESecurityCallbacks() = default;

  // This callback is called by the device that has Input capability when the peer device has Output capability
  // and the passkey is not set.
  // It should return the passkey that the peer device is showing on its output.
  // This MUST be replaced with a custom implementation when being used.
  virtual uint32_t onPassKeyRequest();

  // This callback is called by the device that has Output capability when the peer device has Input capability
  // It should display the passkey that will need to be entered on the peer device
  virtual void onPassKeyNotify(uint32_t pass_key);

  // This callback is called when the peer device requests a secure connection.
  // Usually the client accepts the server's security request.
  // It should return true if the connection is accepted, false otherwise.
  virtual bool onSecurityRequest();

  // This callback is called by both devices when both have the DisplayYesNo capability.
  // It should return true if both devices display the same passkey.
  // This MUST be replaced with a custom implementation when being used.
  virtual bool onConfirmPIN(uint32_t pin);

  // This callback is called when the peer device requests authorization to read or write a characteristic.
  // It should return true if the authorization is granted, false otherwise.
  // This MUST be replaced with a custom implementation when being used.
  virtual bool onAuthorizationRequest(uint16_t connHandle, uint16_t attrHandle, bool isRead);

  /***************************************************************************
   *                       Bluedroid public declarations                     *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  // This callback is called when the authentication is complete.
  // Status can be checked in the desc parameter.
  virtual void onAuthenticationComplete(esp_ble_auth_cmpl_t desc);
#endif

  /***************************************************************************
   *                       NimBLE public declarations                        *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  // This callback is called when the authentication is complete.
  // Status can be checked in the desc parameter.
  virtual void onAuthenticationComplete(ble_gap_conn_desc *desc);
#endif

};  // BLESecurityCallbacks

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif  // COMPONENTS_CPP_UTILS_BLESECURITY_H_
