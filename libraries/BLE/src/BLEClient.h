/*
 * BLEClient.h
 *
 *  Created on: Mar 22, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef MAIN_BLECLIENT_H_
#define MAIN_BLECLIENT_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include <string.h>
#include <map>
#include <string>
#include "BLERemoteService.h"
#include "BLEService.h"
#include "BLEAddress.h"
#include "BLEAdvertisedDevice.h"
#include "BLEUtils.h"

/***************************************************************************
 *                           Bluedroid includes                            *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gattc_api.h>
#ifndef BLE_ERR_REM_USER_CONN_TERM
#define BLE_ERR_REM_USER_CONN_TERM 0x13
#endif
#endif

/***************************************************************************
 *                     NimBLE includes and definitions                     *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <nimble/ble.h>
#include <host/ble_hs.h>
#include <nimble/nimble_port.h>
#define ESP_GATT_IF_NONE BLE_HS_CONN_HANDLE_NONE
#endif

/***************************************************************************
 *                              NimBLE types                               *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
typedef uint16_t esp_gatt_if_t;
#endif

/***************************************************************************
 *                           Forward declarations                          *
 ***************************************************************************/

class BLERemoteService;
class BLEClientCallbacks;
class BLEAdvertisedDevice;
struct BLETaskData;

/**
 * @brief A model of a %BLE client.
 */
class BLEClient {
public:
  /***************************************************************************
   *                            Common public properties                     *
   ***************************************************************************/

  uint16_t m_appId;

  /***************************************************************************
   *                           Common public declarations                    *
   ***************************************************************************/

  BLEClient();
  ~BLEClient();
  bool connect(BLEAdvertisedDevice *device);
  bool connectTimeout(BLEAdvertisedDevice *device, uint32_t timeoutMS = portMAX_DELAY);
  bool connect(BLEAddress address, uint8_t type = 0, uint32_t timeoutMS = portMAX_DELAY);
  bool secureConnection();
  int disconnect(uint8_t reason = BLE_ERR_REM_USER_CONN_TERM);
  BLEAddress getPeerAddress();
  int getRssi();
  std::map<std::string, BLERemoteService *> *getServices();
  BLERemoteService *getService(const char *uuid);
  BLERemoteService *getService(BLEUUID uuid);
  String getValue(BLEUUID serviceUUID, BLEUUID characteristicUUID);
  bool isConnected();
  void setClientCallbacks(BLEClientCallbacks *pClientCallbacks);
  void setValue(BLEUUID serviceUUID, BLEUUID characteristicUUID, String value);
  String toString();
  uint16_t getConnId();
  esp_gatt_if_t getGattcIf();
  uint16_t getMTU();
  bool setMTU(uint16_t mtu);

  /***************************************************************************
   *                           Bluedroid public declarations                 *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  void handleGAPEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
#endif

  /***************************************************************************
   *                           NimBLE public declarations                    *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  static int handleGAPEvent(struct ble_gap_event *event, void *arg);
  static int serviceDiscoveredCB(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg);
#endif

private:
  friend class BLEDevice;
  friend class BLERemoteService;
  friend class BLERemoteCharacteristic;
  friend class BLERemoteDescriptor;

  /***************************************************************************
   *                           Common private properties                     *
   ***************************************************************************/

  BLEAddress m_peerAddress = BLEAddress((uint8_t *)"\0\0\0\0\0\0");
  uint16_t m_conn_id;
  bool m_haveServices = false;
  bool m_isConnected = false;
  BLEClientCallbacks *m_pClientCallbacks;
  FreeRTOS::Semaphore m_semaphoreRegEvt = FreeRTOS::Semaphore("RegEvt");
  FreeRTOS::Semaphore m_semaphoreOpenEvt = FreeRTOS::Semaphore("OpenEvt");
  FreeRTOS::Semaphore m_semaphoreSearchCmplEvt = FreeRTOS::Semaphore("SearchCmplEvt");
  FreeRTOS::Semaphore m_semaphoreRssiCmplEvt = FreeRTOS::Semaphore("RssiCmplEvt");
  std::map<std::string, BLERemoteService *> m_servicesMap;
  std::map<BLERemoteService *, uint16_t> m_servicesMapByInstID;
  uint16_t m_mtu = 23;

  /***************************************************************************
   *                           Bluedroid private properties                  *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  esp_gatt_if_t m_gattc_if;
#endif

  /***************************************************************************
   *                           NimBLE private properties                     *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  int m_lastErr;
  int32_t m_connectTimeout;
  uint8_t m_terminateFailCount;
  ble_gap_conn_params m_pConnParams;
  mutable BLETaskData *m_pTaskData;
#endif

  /***************************************************************************
   *                           Common private declarations                   *
   ***************************************************************************/

  void clearServices();

  /***************************************************************************
   *                          Bluedroid private declarations                 *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  void gattClientEventHandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
#endif

  /***************************************************************************
   *                           NimBLE private declarations                   *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  static void dcTimerCb(ble_npl_event *event);
#endif
};  // class BLEClient

/**
 * @brief Callbacks associated with a %BLE client.
 */
class BLEClientCallbacks {
public:
  /***************************************************************************
   *                           Common public declarations                    *
   ***************************************************************************/

  virtual ~BLEClientCallbacks(){};
  virtual void onConnect(BLEClient *pClient);
  virtual void onDisconnect(BLEClient *pClient);

  /***************************************************************************
   *                           NimBLE public declarations                    *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  virtual bool onConnParamsUpdateRequest(BLEClient *pClient, const ble_gap_upd_params *params);
#endif
};

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* MAIN_BLECLIENT_H_ */
