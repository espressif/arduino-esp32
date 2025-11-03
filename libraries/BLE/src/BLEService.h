/*
 * BLEService.h
 *
 *  Created on: Mar 25, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef COMPONENTS_CPP_UTILS_BLESERVICE_H_
#define COMPONENTS_CPP_UTILS_BLESERVICE_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/*****************************************************************************
 *                             Common includes                               *
 *****************************************************************************/

#include "BLECharacteristic.h"
#include "BLEServer.h"
#include "BLEUUID.h"
#include "BLEUtils.h"
#include "RTOS.h"

/*****************************************************************************
 *                          Bluedroid includes                               *
 *****************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gatts_api.h>
#endif

/*****************************************************************************
 *                            NimBLE includes                                *
 *****************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_gatt.h>
#endif

/*****************************************************************************
 *                          Forward declarations                             *
 *****************************************************************************/

class BLEServer;

/**
 * @brief A data mapping used to manage the set of %BLE characteristics known to the server.
 */
class BLECharacteristicMap {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  void setByUUID(BLECharacteristic *pCharacteristic, const char *uuid);
  void setByUUID(BLECharacteristic *pCharacteristic, BLEUUID uuid);
  void setByHandle(uint16_t handle, BLECharacteristic *pCharacteristic);
  BLECharacteristic *getByUUID(const char *uuid) const;
  BLECharacteristic *getByUUID(BLEUUID uuid) const;
  BLECharacteristic *getByHandle(uint16_t handle) const;
  BLECharacteristic *getFirst();
  BLECharacteristic *getNext();
  String toString() const;
  int getRegisteredCharacteristicCount() const;
  void removeCharacteristic(BLECharacteristic *characteristic);

  /***************************************************************************
   *                      Bluedroid public declarations                      *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  void handleGATTServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
#endif

  /***************************************************************************
   *                        NimBLE public declarations                       *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  void handleGATTServerEvent(uint16_t conn_handle, uint16_t attr_handle, ble_gatt_access_ctxt *ctxt, void *arg);
#endif

private:
  /***************************************************************************
   *                        Common private properties                        *
   ***************************************************************************/

  std::map<BLECharacteristic *, String> m_uuidMap;
  std::map<uint16_t, BLECharacteristic *> m_handleMap;
  std::map<BLECharacteristic *, String>::iterator m_iterator;
};

/**
 * @brief The model of a %BLE service.
 */
class BLEService {
public:
  /***************************************************************************
   *                            Common properties                            *
   ***************************************************************************/

  uint8_t m_instId = 0;

  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  void addCharacteristic(BLECharacteristic *pCharacteristic);
  BLECharacteristic *createCharacteristic(const char *uuid, uint32_t properties);
  BLECharacteristic *createCharacteristic(BLEUUID uuid, uint32_t properties);
  void dump();
  void executeCreate(BLEServer *pServer);
  void executeDelete();
  BLECharacteristic *getCharacteristic(const char *uuid);
  BLECharacteristic *getCharacteristic(BLEUUID uuid);
  BLEUUID getUUID();
  BLEServer *getServer();
  bool start();
  void stop();
  String toString();
  uint16_t getHandle();

  /***************************************************************************
   *                       NimBLE public declarations                        *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  void removeCharacteristic(BLECharacteristic *pCharacteristic, bool deleteChr = false);
#endif

private:
  friend class BLEServer;
  friend class BLEServiceMap;
  friend class BLEDescriptor;
  friend class BLECharacteristic;
  friend class BLEDevice;

  /***************************************************************************
   *                         Common private properties                       *
   ***************************************************************************/

  BLECharacteristicMap m_characteristicMap;
  uint16_t m_handle;
  BLECharacteristic *m_lastCreatedCharacteristic = nullptr;
  BLEServer *m_pServer = nullptr;
  BLEUUID m_uuid;
  FreeRTOS::Semaphore m_semaphoreStartEvt = FreeRTOS::Semaphore("StartEvt");
  uint16_t m_numHandles;

  /***************************************************************************
   *                       Bluedroid private properties                      *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  FreeRTOS::Semaphore m_semaphoreCreateEvt = FreeRTOS::Semaphore("CreateEvt");
  FreeRTOS::Semaphore m_semaphoreDeleteEvt = FreeRTOS::Semaphore("DeleteEvt");
  FreeRTOS::Semaphore m_semaphoreStopEvt = FreeRTOS::Semaphore("StopEvt");
#endif

  /***************************************************************************
   *                       NimBLE private properties                         *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  uint8_t m_removed;
  ble_gatt_svc_def *m_pSvcDef;
#endif

  /***************************************************************************
   *                       Common private declarations                       *
   ***************************************************************************/

  BLEService(const char *uuid, uint16_t numHandles);
  BLEService(BLEUUID uuid, uint16_t numHandles);
  ~BLEService();

  /***************************************************************************
   *                       Common private declarations                       *
   ***************************************************************************/

  void setHandle(uint16_t handle);

  /***************************************************************************
   *                     Bluedroid private declarations                      *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  BLECharacteristic *getLastCreatedCharacteristic();
  void handleGATTServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
#endif
};  // BLEService

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* COMPONENTS_CPP_UTILS_BLESERVICE_H_ */
