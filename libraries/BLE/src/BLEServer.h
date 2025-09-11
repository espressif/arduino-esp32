/*
 * BLEServer.h
 *
 *  Created on: Apr 16, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef COMPONENTS_CPP_UTILS_BLESERVER_H_
#define COMPONENTS_CPP_UTILS_BLESERVER_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/*****************************************************************************
 *                             Common includes                               *
 *****************************************************************************/

#include <string>
#include <string.h>
#include "BLEDevice.h"
#include "BLEConnInfo.h"
#include "BLEUUID.h"
#include "BLEAdvertising.h"
#include "BLECharacteristic.h"
#include "BLEService.h"
#include "BLESecurity.h"
#include "RTOS.h"
#include "BLEAddress.h"
#include "BLEUtils.h"
#include "BLEUtils.h"

/*****************************************************************************
 *                            Bluedroid includes                             *
 *****************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gatts_api.h>
#endif

/*****************************************************************************
 *                       NimBLE includes and definitions                     *
 *****************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_gatt.h>
#define ESP_GATT_IF_NONE         BLE_HS_CONN_HANDLE_NONE
#define NIMBLE_ATT_REMOVE_HIDE   1
#define NIMBLE_ATT_REMOVE_DELETE 2
#endif

/*****************************************************************************
 *                           Forward declarations                            *
 *****************************************************************************/

class BLEServerCallbacks;
class BLEService;
class BLECharacteristic;
class BLEDevice;
class BLESecurity;
class BLEAdvertising;

/**
 * @brief A data structure that manages the %BLE services owned by a BLE server.
 */
class BLEServiceMap {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  BLEService *getByHandle(uint16_t handle);
  BLEService *getByUUID(const char *uuid);
  BLEService *getByUUID(BLEUUID uuid, uint8_t inst_id = 0);
  void setByHandle(uint16_t handle, BLEService *service);
  void setByUUID(const char *uuid, BLEService *service);
  void setByUUID(BLEUUID uuid, BLEService *service);
  String toString();
  BLEService *getFirst();
  BLEService *getNext();
  void removeService(BLEService *service);
  int getRegisteredServiceCount();

  /***************************************************************************
   *                       Bluedroid public declarations                     *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  void handleGATTServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
#endif

private:
  /***************************************************************************
   *                       Common private properties                         *
   ***************************************************************************/

  std::map<uint16_t, BLEService *> m_handleMap;
  std::map<BLEService *, String> m_uuidMap;
  std::map<BLEService *, String>::iterator m_iterator;
};

/**
 * @brief The model of a %BLE server.
 */
class BLEServer {
public:
  /***************************************************************************
   *                        Common public properties                         *
   ***************************************************************************/

  uint16_t m_appId;

  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  uint32_t getConnectedCount();
  BLEService *createService(const char *uuid);
  BLEService *createService(BLEUUID uuid, uint32_t numHandles = 15, uint8_t inst_id = 0);
  BLEAdvertising *getAdvertising();
  void setCallbacks(BLEServerCallbacks *pCallbacks);
  void startAdvertising();
  void removeService(BLEService *service);
  BLEService *getServiceByUUID(const char *uuid);
  BLEService *getServiceByUUID(BLEUUID uuid);
  void start();
#if !defined(CONFIG_BT_NIMBLE_EXT_ADV) || defined(CONFIG_BLUEDROID_ENABLED)
  void advertiseOnDisconnect(bool enable);
#endif

  // Connection management functions
  std::map<uint16_t, conn_status_t> getPeerDevices(bool client);
  void addPeerDevice(void *peer, bool is_client, uint16_t conn_id);
  bool removePeerDevice(uint16_t conn_id, bool client);
  BLEServer *getServerByConnId(uint16_t conn_id);
  void updatePeerMTU(uint16_t connId, uint16_t mtu);
  uint16_t getPeerMTU(uint16_t conn_id);
  uint16_t getConnId();

  /***************************************************************************
   *                       Bluedroid public declarations                     *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  bool connect(BLEAddress address);
  void updateConnParams(esp_bd_addr_t remote_bda, uint16_t minInterval, uint16_t maxInterval, uint16_t latency, uint16_t timeout);
  void disconnect(uint16_t connId);
#endif

  /***************************************************************************
   *                       NimBLE public declarations                        *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  uint16_t getHandle();
  void updateConnParams(uint16_t conn_handle, uint16_t minInterval, uint16_t maxInterval, uint16_t latency, uint16_t timeout);
  int disconnect(uint16_t connId, uint8_t reason = BLE_ERR_REM_USER_CONN_TERM);
#endif

private:
  friend class BLEService;
  friend class BLECharacteristic;
  friend class BLEDevice;
  friend class BLESecurity;
  friend class BLEAdvertising;

  /***************************************************************************
   *                       Common private properties                         *
   ***************************************************************************/

  uint16_t m_connId;
  uint32_t m_connectedCount;
  bool m_gattsStarted;
  std::map<uint16_t, conn_status_t> m_connectedServersMap;
#if !defined(CONFIG_BT_NIMBLE_EXT_ADV) || defined(CONFIG_BLUEDROID_ENABLED)
  bool m_advertiseOnDisconnect;
#endif
  FreeRTOS::Semaphore m_semaphoreRegisterAppEvt = FreeRTOS::Semaphore("RegisterAppEvt");
  FreeRTOS::Semaphore m_semaphoreCreateEvt = FreeRTOS::Semaphore("CreateEvt");
  FreeRTOS::Semaphore m_semaphoreOpenEvt = FreeRTOS::Semaphore("OpenEvt");
  BLEServiceMap m_serviceMap;
  BLEServerCallbacks *m_pServerCallbacks = nullptr;

  /***************************************************************************
   *                       Bluedroid private properties                       *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  uint16_t m_gatts_if;
  esp_ble_adv_data_t m_adv_data;
#endif

  /***************************************************************************
   *                       NimBLE private properties                         *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  bool m_svcChanged;
  uint16_t m_indWait[CONFIG_BT_NIMBLE_MAX_CONNECTIONS];
  std::vector<BLECharacteristic *> m_notifyChrVec;
  ble_hs_adv_fields m_adv_data;
#endif

  /***************************************************************************
   *                       Common private declarations                       *
   ***************************************************************************/

  BLEServer();
  void createApp(uint16_t appId);

  /***************************************************************************
   *                       Bluedroid private declarations                    *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  uint16_t getGattsIf();
  void registerApp(uint16_t);
  void handleGATTServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
#endif

  /***************************************************************************
   *                       NimBLE private declarations                       *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  void serviceChanged();
  void resetGATT();
  bool setIndicateWait(uint16_t conn_handle);
  void clearIndicateWait(uint16_t conn_handle);
  static int handleGATTServerEvent(struct ble_gap_event *event, void *arg);
#endif
};  // BLEServer

/**
 * @brief Callbacks associated with the operation of a %BLE server.
 */
class BLEServerCallbacks {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  virtual ~BLEServerCallbacks(){};
  virtual void onConnect(BLEServer *pServer);
  virtual void onDisconnect(BLEServer *pServer);

  /***************************************************************************
   *                       Bluedroid public declarations                     *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  virtual void onConnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param);
  virtual void onDisconnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param);
  virtual void onMtuChanged(BLEServer *pServer, esp_ble_gatts_cb_param_t *param);
#endif

  /***************************************************************************
   *                       NimBLE public declarations                        *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  virtual void onConnect(BLEServer *pServer, ble_gap_conn_desc *desc);
  virtual void onDisconnect(BLEServer *pServer, ble_gap_conn_desc *desc);
  virtual void onMtuChanged(BLEServer *pServer, ble_gap_conn_desc *desc, uint16_t mtu);
#endif
};  // BLEServerCallbacks

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* COMPONENTS_CPP_UTILS_BLESERVER_H_ */
