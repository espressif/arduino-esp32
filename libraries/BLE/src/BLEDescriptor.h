/*
 * BLEDescriptor.h
 *
 *  Created on: Jun 22, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef COMPONENTS_CPP_UTILS_BLEDESCRIPTOR_H_
#define COMPONENTS_CPP_UTILS_BLEDESCRIPTOR_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include <string>
#include "BLEUUID.h"
#include "BLECharacteristic.h"
#include "RTOS.h"

/***************************************************************************
 *                           Bluedroid includes                            *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gatts_api.h>
#endif

/***************************************************************************
 *                           NimBLE includes                               *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_att.h>
#include "BLEConnInfo.h"

// Bluedroid compatibility
// NimBLE does not support signed reads and writes

#define ESP_GATT_PERM_READ                BLE_ATT_F_READ
#define ESP_GATT_PERM_WRITE               BLE_ATT_F_WRITE
#define ESP_GATT_PERM_READ_ENCRYPTED      BLE_ATT_F_READ_ENC
#define ESP_GATT_PERM_WRITE_ENCRYPTED     BLE_ATT_F_WRITE_ENC
#define ESP_GATT_PERM_READ_AUTHORIZATION  BLE_ATT_F_READ_AUTHOR
#define ESP_GATT_PERM_WRITE_AUTHORIZATION BLE_ATT_F_WRITE_AUTHOR
#define ESP_GATT_PERM_READ_ENC_MITM       BLE_ATT_F_READ_AUTHEN
#define ESP_GATT_PERM_WRITE_ENC_MITM      BLE_ATT_F_WRITE_AUTHEN

#endif

/***************************************************************************
 *                            NimBLE types                                 *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
typedef struct {
  uint16_t attr_max_len; /*!<  attribute max value length */
  uint16_t attr_len;     /*!<  attribute current value length */
  uint8_t *attr_value;   /*!<  the pointer to attribute value */
} esp_attr_value_t;
#endif

/***************************************************************************
 *                           Forward declarations                          *
 ***************************************************************************/

class BLEService;
class BLECharacteristic;
class BLEDescriptorCallbacks;

/**
 * @brief A model of a %BLE descriptor.
 */
class BLEDescriptor {
public:
  /***************************************************************************
   *                           Common public declarations                    *
   ***************************************************************************/

  BLEDescriptor(const char *uuid, uint16_t max_len = 100);
  BLEDescriptor(BLEUUID uuid, uint16_t max_len = 100);
  virtual ~BLEDescriptor();

  uint16_t getHandle() const;                    // Get the handle of the descriptor.
  size_t getLength() const;                      // Get the length of the value of the descriptor.
  BLEUUID getUUID() const;                       // Get the UUID of the descriptor.
  uint8_t *getValue() const;                     // Get a pointer to the value of the descriptor.
  BLECharacteristic *getCharacteristic() const;  // Get the characteristic that this descriptor belongs to.

  void setAccessPermissions(uint16_t perm);               // Set the permissions of the descriptor.
  void setCallbacks(BLEDescriptorCallbacks *pCallbacks);  // Set callbacks to be invoked for the descriptor.
  void setValue(const uint8_t *data, size_t size);        // Set the value of the descriptor as a pointer to data.
  void setValue(const String &value);                     // Set the value of the descriptor as a data buffer.

  String toString() const;  // Convert the descriptor to a string representation.

  /***************************************************************************
   *                           Bluedroid public declarations                 *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  void handleGATTServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
#endif

  /***************************************************************************
   *                           NimBLE public declarations                    *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  static int handleGATTServerEvent(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
#endif

private:
  friend class BLEDescriptorMap;
  friend class BLECharacteristic;
  friend class BLEService;
  friend class BLE2901;
  friend class BLE2902;
  friend class BLE2904;

  /***************************************************************************
   *                           Common private properties                     *
   ***************************************************************************/

  BLEUUID m_bleUUID;
  uint16_t m_handle;
  esp_attr_value_t m_value;
  BLEDescriptorCallbacks *m_pCallback;
  BLECharacteristic *m_pCharacteristic;
  FreeRTOS::Semaphore m_semaphoreSetValue = FreeRTOS::Semaphore("SetValue");

  /***************************************************************************
   *                           Bluedroid private properties                  *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  FreeRTOS::Semaphore m_semaphoreCreateEvt = FreeRTOS::Semaphore("CreateEvt");
  uint16_t m_permissions = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE;
#endif

  /***************************************************************************
   *                           NimBLE private properties                     *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  uint8_t m_permissions = HA_FLAG_PERM_RW;
  uint8_t m_removed = 0;
#endif

  /***************************************************************************
   *                           Common private declarations                   *
   ***************************************************************************/

  void executeCreate(BLECharacteristic *pCharacteristic);
  void setHandle(uint16_t handle);
};  // BLEDescriptor

/**
 * @brief Callbacks that can be associated with a %BLE descriptors to inform of events.
 *
 * When a server application creates a %BLE descriptor, we may wish to be informed when there is either
 * a read or write request to the descriptors value.  An application can register a
 * sub-classed instance of this class and will be notified when such an event happens.
 */
class BLEDescriptorCallbacks {
public:
  /***************************************************************************
   *                           Common public declarations                    *
   ***************************************************************************/

  virtual ~BLEDescriptorCallbacks();
  virtual void onRead(BLEDescriptor *pDescriptor);
  virtual void onWrite(BLEDescriptor *pDescriptor);
};

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* COMPONENTS_CPP_UTILS_BLEDESCRIPTOR_H_ */
