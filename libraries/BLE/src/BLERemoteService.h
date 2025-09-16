/*
 * BLERemoteService.h
 *
 *  Created on: Jul 8, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef COMPONENTS_CPP_UTILS_BLEREMOTESERVICE_H_
#define COMPONENTS_CPP_UTILS_BLEREMOTESERVICE_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include <map>

#include "BLEClient.h"
#include "BLERemoteCharacteristic.h"
#include "BLEUUID.h"
#include "RTOS.h"
#include "BLEUtils.h"

/***************************************************************************
 *                           NimBLE includes                               *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_gatt.h>
#endif

/***************************************************************************
 *                         Forward declarations                            *
 ***************************************************************************/

class BLEClient;
class BLERemoteCharacteristic;

/**
 * @brief A model of a remote %BLE service.
 */
class BLERemoteService {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  virtual ~BLERemoteService();
  BLERemoteCharacteristic *getCharacteristic(const char *uuid);  // Get the specified characteristic reference.
  BLERemoteCharacteristic *getCharacteristic(BLEUUID uuid);      // Get the specified characteristic reference.
  BLERemoteCharacteristic *getCharacteristic(uint16_t uuid);     // Get the specified characteristic reference.
  std::map<std::string, BLERemoteCharacteristic *> *getCharacteristics();
  std::map<uint16_t, BLERemoteCharacteristic *> *getCharacteristicsByHandle();  // Get the characteristics map.
  void getCharacteristics(std::map<uint16_t, BLERemoteCharacteristic *> **pCharacteristicMap);
  void retrieveCharacteristics();

  BLEClient *getClient(void);                               // Get a reference to the client associated with this service.
  uint16_t getHandle();                                     // Get the handle of this service.
  BLEUUID getUUID(void);                                    // Get the UUID of this service.
  String getValue(BLEUUID characteristicUuid);              // Get the value of a characteristic.
  void setValue(BLEUUID characteristicUuid, String value);  // Set the value of a characteristic.
  String toString(void);

private:
  friend class BLEClient;
  friend class BLERemoteCharacteristic;

  /***************************************************************************
   *                       Common private properties                         *
   ***************************************************************************/

  // We maintain a map of characteristics owned by this service keyed by a string representation of the UUID.
  std::map<std::string, BLERemoteCharacteristic *> m_characteristicMap;

  // We maintain a map of characteristics owned by this service keyed by a handle.
  std::map<uint16_t, BLERemoteCharacteristic *> m_characteristicMapByHandle;

  bool m_haveCharacteristics;  // Have we previously obtained the characteristics.
  BLEClient *m_pClient;
  FreeRTOS::Semaphore m_semaphoreGetCharEvt = FreeRTOS::Semaphore("GetCharEvt");
  BLEUUID m_uuid;          // The UUID of this service.
  uint16_t m_startHandle;  // The starting handle of this service.
  uint16_t m_endHandle;    // The ending handle of this service.

  /***************************************************************************
   *                       Bluedroid private properties                      *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  esp_gatt_id_t m_srvcId;
#endif

  /***************************************************************************
   *                       Common private declarations                       *
   ***************************************************************************/

  uint16_t getStartHandle();  // Get the start handle for this service.
  uint16_t getEndHandle();    // Get the end handle for this service.
  void removeCharacteristics();

  /***************************************************************************
   *                       Bluedroid private declarations                    *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  // Private constructor ... never meant to be created by a user application.
  BLERemoteService(esp_gatt_id_t srvcId, BLEClient *pClient, uint16_t startHandle, uint16_t endHandle);
  esp_gatt_id_t *getSrvcId();
  void gattClientEventHandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *evtParam);
#endif

  /***************************************************************************
   *                       NimBLE private declarations                       *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  BLERemoteService(BLEClient *pClient, const struct ble_gatt_svc *service);
  static int characteristicDiscCB(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg);
#endif
};  // BLERemoteService

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* COMPONENTS_CPP_UTILS_BLEREMOTESERVICE_H_ */
