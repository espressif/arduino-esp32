/*
 * BLECharacteristic.h
 *
 *  Created on: Jun 22, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef COMPONENTS_CPP_UTILS_BLECHARACTERISTIC_H_
#define COMPONENTS_CPP_UTILS_BLECHARACTERISTIC_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include <string>
#include <map>
#include "BLEUUID.h"
#include "BLEDescriptor.h"
#include "BLEValue.h"
#include "RTOS.h"
#include "BLEUtils.h"

/***************************************************************************
 *                          Bluedroid includes                             *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gatts_api.h>
#include <esp_gap_ble_api.h>
#endif

/***************************************************************************
 *                     NimBLE includes and definitions                     *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <vector>
#include <host/ble_gatt.h>
#include <host/ble_att.h>
#include "BLEConnInfo.h"
#define ESP_GATT_MAX_ATTR_LEN            BLE_ATT_ATTR_MAX_LEN
#define ESP_GATT_CHAR_PROP_BIT_READ      BLE_GATT_CHR_PROP_READ
#define ESP_GATT_CHAR_PROP_BIT_WRITE     BLE_GATT_CHR_PROP_WRITE
#define ESP_GATT_CHAR_PROP_BIT_WRITE_NR  BLE_GATT_CHR_PROP_WRITE_NO_RSP
#define ESP_GATT_CHAR_PROP_BIT_BROADCAST BLE_GATT_CHR_PROP_BROADCAST
#define ESP_GATT_CHAR_PROP_BIT_NOTIFY    BLE_GATT_CHR_PROP_NOTIFY
#define ESP_GATT_CHAR_PROP_BIT_INDICATE  BLE_GATT_CHR_PROP_INDICATE
#endif

/***************************************************************************
 *                              NimBLE types                               *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
typedef uint16_t esp_gatt_char_prop_t;
typedef uint16_t esp_gatt_perm_t;
#endif

/***************************************************************************
 *                           Forward declarations                          *
 ***************************************************************************/

class BLEService;
class BLEDescriptor;
class BLECharacteristicCallbacks;

/**
 * @brief A management structure for %BLE descriptors.
 */
class BLEDescriptorMap {
public:
  /***************************************************************************
   *                        Common public declarations                       *
   ***************************************************************************/

  void setByUUID(const char *uuid, BLEDescriptor *pDescriptor);
  void setByUUID(BLEUUID uuid, BLEDescriptor *pDescriptor);
  void setByHandle(uint16_t handle, BLEDescriptor *pDescriptor);
  BLEDescriptor *getByUUID(const char *uuid) const;
  BLEDescriptor *getByUUID(BLEUUID uuid) const;
  BLEDescriptor *getByHandle(uint16_t handle) const;
  String toString() const;
  BLEDescriptor *getFirst();
  BLEDescriptor *getNext();
  int getRegisteredDescriptorCount() const;
  void removeDescriptor(BLEDescriptor *pDescriptor);

  /***************************************************************************
   *                        Bluedroid public declarations                    *
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

  std::map<BLEDescriptor *, String> m_uuidMap;
  std::map<uint16_t, BLEDescriptor *> m_handleMap;
  std::map<BLEDescriptor *, String>::iterator m_iterator;
};

/**
 * @brief The model of a %BLE Characteristic.
 *
 * A BLE Characteristic is an identified value container that manages a value. It is exposed by a BLE server and
 * can be read and written to by a %BLE client.
 */
class BLECharacteristic {
public:
  /***************************************************************************
   *                        Common properties                                *
   ***************************************************************************/

  static const uint32_t indicationTimeout = 1000;

  /***************************************************************************
   *                    Bluedroid public properties                        *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  static const uint32_t PROPERTY_READ = 1 << 0;
  static const uint32_t PROPERTY_READ_ENC = 0;     // Not supported by Bluedroid. Use setAccessPermissions() instead.
  static const uint32_t PROPERTY_READ_AUTHEN = 0;  // Not supported by Bluedroid. Use setAccessPermissions() instead.
  static const uint32_t PROPERTY_READ_AUTHOR = 0;  // Not supported by Bluedroid. Use setAccessPermissions() instead.
  static const uint32_t PROPERTY_WRITE = 1 << 1;
  static const uint32_t PROPERTY_WRITE_NR = 1 << 5;
  static const uint32_t PROPERTY_WRITE_ENC = 0;     // Not supported by Bluedroid. Use setAccessPermissions() instead.
  static const uint32_t PROPERTY_WRITE_AUTHEN = 0;  // Not supported by Bluedroid. Use setAccessPermissions() instead.
  static const uint32_t PROPERTY_WRITE_AUTHOR = 0;  // Not supported by Bluedroid. Use setAccessPermissions() instead.
  static const uint32_t PROPERTY_NOTIFY = 1 << 2;
  static const uint32_t PROPERTY_BROADCAST = 1 << 3;
  static const uint32_t PROPERTY_INDICATE = 1 << 4;
#endif

  /***************************************************************************
   *                     NimBLE public properties                            *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  static const uint32_t PROPERTY_READ = BLE_GATT_CHR_F_READ;
  static const uint32_t PROPERTY_READ_ENC = BLE_GATT_CHR_F_READ_ENC;
  static const uint32_t PROPERTY_READ_AUTHEN = BLE_GATT_CHR_F_READ_AUTHEN;
  static const uint32_t PROPERTY_READ_AUTHOR = BLE_GATT_CHR_F_READ_AUTHOR;
  static const uint32_t PROPERTY_WRITE = BLE_GATT_CHR_F_WRITE;
  static const uint32_t PROPERTY_WRITE_NR = BLE_GATT_CHR_F_WRITE_NO_RSP;
  static const uint32_t PROPERTY_WRITE_ENC = BLE_GATT_CHR_F_WRITE_ENC;
  static const uint32_t PROPERTY_WRITE_AUTHEN = BLE_GATT_CHR_F_WRITE_AUTHEN;
  static const uint32_t PROPERTY_WRITE_AUTHOR = BLE_GATT_CHR_F_WRITE_AUTHOR;
  static const uint32_t PROPERTY_NOTIFY = BLE_GATT_CHR_F_NOTIFY;
  static const uint32_t PROPERTY_BROADCAST = BLE_GATT_CHR_F_BROADCAST;
  static const uint32_t PROPERTY_INDICATE = BLE_GATT_CHR_F_INDICATE;
#endif

  /***************************************************************************
   *                        Common public declarations                       *
   ***************************************************************************/

  BLECharacteristic(const char *uuid, uint32_t properties = 0);
  BLECharacteristic(BLEUUID uuid, uint32_t properties = 0);
  virtual ~BLECharacteristic();

  void addDescriptor(BLEDescriptor *pDescriptor);
  BLEDescriptor *getDescriptorByUUID(const char *descriptorUUID) const;
  BLEDescriptor *getDescriptorByUUID(BLEUUID descriptorUUID) const;
  BLEUUID getUUID() const;
  String getValue() const;
  uint8_t *getData();
  size_t getLength() const;
  void indicate();
  void notify(bool is_notification = true);
  void setCallbacks(BLECharacteristicCallbacks *pCallbacks);
  void setValue(const uint8_t *data, size_t size);
  void setValue(const String &value);
  void setValue(uint16_t data16);
  void setValue(uint32_t data32);
  void setValue(int data32);
  void setValue(float data32);
  void setValue(double data64);
  String toString() const;
  uint16_t getHandle() const;
  void setAccessPermissions(uint16_t perm);
  esp_gatt_char_prop_t getProperties() const;
  void setReadProperty(bool value);
  void setWriteProperty(bool value);
  void setNotifyProperty(bool value);
  void setBroadcastProperty(bool value);
  void setIndicateProperty(bool value);
  void setWriteNoResponseProperty(bool value);

private:
  friend class BLEServer;
  friend class BLEService;
  friend class BLEDescriptor;
  friend class BLECharacteristicMap;

  /***************************************************************************
   *                        Common private properties                        *
   ***************************************************************************/

  BLEUUID m_bleUUID;
  BLEDescriptorMap m_descriptorMap;
  uint16_t m_handle;
  esp_gatt_char_prop_t m_properties;
  BLECharacteristicCallbacks *m_pCallbacks;
  BLEService *m_pService;
  BLEValue m_value;
  bool m_writeEvt = false;
  FreeRTOS::Semaphore m_semaphoreConfEvt = FreeRTOS::Semaphore("ConfEvt");
  FreeRTOS::Semaphore m_semaphoreSetValue = FreeRTOS::Semaphore("SetValue");

  /***************************************************************************
   *                        Bluedroid private properties                      *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  FreeRTOS::Semaphore m_semaphoreCreateEvt = FreeRTOS::Semaphore("CreateEvt");
  esp_gatt_perm_t m_permissions = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE;
#endif

  /***************************************************************************
   *                        NimBLE private properties                        *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  portMUX_TYPE m_readMux;
  uint8_t m_removed;
  std::vector<std::pair<uint16_t, uint16_t>> m_subscribedVec;
#endif

  /***************************************************************************
   *                        Common private declarations                      *
   ***************************************************************************/

  void executeCreate(BLEService *pService);
  BLEService *getService() const;
  void setHandle(uint16_t handle);

  /***************************************************************************
   *                        Bluedroid private declarations                    *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  void handleGATTServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
#endif

  /***************************************************************************
   *                        NimBLE private declarations                      *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  void setSubscribe(struct ble_gap_event *event);
  static int handleGATTServerEvent(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
#endif
};  // BLECharacteristic

/**
 * @brief Callbacks that can be associated with a %BLE characteristic to inform of events.
 *
 * When a server application creates a %BLE characteristic, we may wish to be informed when there is either
 * a read or write request to the characteristic's value. An application can register a
 * sub-classed instance of this class and will be notified when such an event happens.
 */
class BLECharacteristicCallbacks {
public:
  /***************************************************************************
   *                        Common public types                              *
   ***************************************************************************/

  typedef enum {
    SUCCESS_INDICATE,
    SUCCESS_NOTIFY,
    ERROR_INDICATE_DISABLED,
    ERROR_NOTIFY_DISABLED,
    ERROR_GATT,
    ERROR_NO_CLIENT,
    ERROR_NO_SUBSCRIBER,
    ERROR_INDICATE_TIMEOUT,
    ERROR_INDICATE_FAILURE
  } Status;

  /***************************************************************************
   *                        Common public declarations                       *
   ***************************************************************************/

  virtual ~BLECharacteristicCallbacks();
  virtual void onRead(BLECharacteristic *pCharacteristic);
  virtual void onWrite(BLECharacteristic *pCharacteristic);
  virtual void onNotify(BLECharacteristic *pCharacteristic);
  virtual void onStatus(BLECharacteristic *pCharacteristic, Status s, uint32_t code);

  /***************************************************************************
   *                        Bluedroid public declarations                    *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  virtual void onRead(BLECharacteristic *pCharacteristic, esp_ble_gatts_cb_param_t *param);
  virtual void onWrite(BLECharacteristic *pCharacteristic, esp_ble_gatts_cb_param_t *param);
#endif

  /***************************************************************************
   *                        NimBLE public declarations                       *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  virtual void onRead(BLECharacteristic *pCharacteristic, ble_gap_conn_desc *desc);
  virtual void onWrite(BLECharacteristic *pCharacteristic, ble_gap_conn_desc *desc);
  virtual void onSubscribe(BLECharacteristic *pCharacteristic, ble_gap_conn_desc *desc, uint16_t subValue);
#endif
};

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* COMPONENTS_CPP_UTILS_BLECHARACTERISTIC_H_ */
