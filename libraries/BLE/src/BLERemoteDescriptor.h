/*
 * BLERemoteDescriptor.h
 *
 *  Created on: Jul 8, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef COMPONENTS_CPP_UTILS_BLEREMOTEDESCRIPTOR_H_
#define COMPONENTS_CPP_UTILS_BLEREMOTEDESCRIPTOR_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include <string>
#include "BLERemoteCharacteristic.h"
#include "BLEUUID.h"
#include "RTOS.h"
#include "BLEUtils.h"

/***************************************************************************
 *                           Bluedroid includes                            *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gattc_api.h>
#endif

/***************************************************************************
 *                           Forward declarations                          *
 ***************************************************************************/

class BLERemoteCharacteristic;

/**
 * @brief A model of remote %BLE descriptor.
 */
class BLERemoteDescriptor {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  uint16_t getHandle();
  BLERemoteCharacteristic *getRemoteCharacteristic();
  BLEUUID getUUID();
  String readValue(void);
  uint8_t readUInt8(void);
  uint16_t readUInt16(void);
  uint32_t readUInt32(void);
  String toString(void);
  bool writeValue(uint8_t *data, size_t length, bool response = false);
  bool writeValue(String newValue, bool response = false);
  bool writeValue(uint8_t newValue, bool response = false);
  void setAuth(uint8_t auth);

  /***************************************************************************
   *                       Bluedroid public declarations                     *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  void gattClientEventHandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *evtParam);
#endif

private:
  friend class BLERemoteCharacteristic;

  /***************************************************************************
   *                        Common private properties                        *
   ***************************************************************************/
  uint16_t m_handle;  // Server handle of this descriptor.
  BLEUUID m_uuid;     // UUID of this descriptor.
  String m_value;     // Last received value of the descriptor.
  uint8_t m_auth;
  BLERemoteCharacteristic *m_pRemoteCharacteristic;  // Reference to the Remote characteristic of which this descriptor is associated.
  FreeRTOS::Semaphore m_semaphoreReadDescrEvt = FreeRTOS::Semaphore("ReadDescrEvt");
  FreeRTOS::Semaphore m_semaphoreWriteDescrEvt = FreeRTOS::Semaphore("WriteDescrEvt");

  /***************************************************************************
   *                       Common private declarations                       *
   ***************************************************************************/

  BLERemoteDescriptor(uint16_t handle, BLEUUID uuid, BLERemoteCharacteristic *pRemoteCharacteristic);

  /***************************************************************************
   *                       NimBLE private declarations                       *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  BLERemoteDescriptor(BLERemoteCharacteristic *pRemoteCharacteristic, const struct ble_gatt_dsc *dsc);
  static int onWriteCB(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg);
  static int onReadCB(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg);
#endif
};

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* COMPONENTS_CPP_UTILS_BLEREMOTEDESCRIPTOR_H_ */
