/*
 * BLERemoteCharacteristic.h
 *
 *  Created on: Jul 8, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef COMPONENTS_CPP_UTILS_BLEREMOTECHARACTERISTIC_H_
#define COMPONENTS_CPP_UTILS_BLEREMOTECHARACTERISTIC_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include <functional>
#include "BLERemoteService.h"
#include "BLERemoteDescriptor.h"
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
 *                     NimBLE includes and definitions                     *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_sm.h>
#include <host/ble_gatt.h>
#include <host/ble_att.h>

// Bluedroid Compatibility
#define ESP_GATT_MAX_ATTR_LEN            BLE_ATT_ATTR_MAX_LEN
#define ESP_GATT_CHAR_PROP_BIT_READ      BLE_GATT_CHR_PROP_READ
#define ESP_GATT_CHAR_PROP_BIT_WRITE     BLE_GATT_CHR_PROP_WRITE
#define ESP_GATT_CHAR_PROP_BIT_WRITE_NR  BLE_GATT_CHR_PROP_WRITE_NO_RSP
#define ESP_GATT_CHAR_PROP_BIT_BROADCAST BLE_GATT_CHR_PROP_BROADCAST
#define ESP_GATT_CHAR_PROP_BIT_NOTIFY    BLE_GATT_CHR_PROP_NOTIFY
#define ESP_GATT_CHAR_PROP_BIT_INDICATE  BLE_GATT_CHR_PROP_INDICATE

#define ESP_GATT_AUTH_REQ_NONE           0
#define ESP_GATT_AUTH_REQ_NO_MITM        1
#define ESP_GATT_AUTH_REQ_MITM           2
#define ESP_GATT_AUTH_REQ_SIGNED_NO_MITM 3
#define ESP_GATT_AUTH_REQ_SIGNED_MITM    4

#endif

/***************************************************************************
 *                             Common types                                *
 ***************************************************************************/

typedef std::function<void(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify)> notify_callback;

/***************************************************************************
 *                             NimBLE types                                *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
typedef struct {
  const BLEUUID *uuid;
  void *task_data;
} desc_filter_t;
#endif

/***************************************************************************
 *                           Forward declarations                          *
 ***************************************************************************/

class BLERemoteService;
class BLERemoteDescriptor;

/**
 * @brief A model of a remote %BLE characteristic.
 */
class BLERemoteCharacteristic {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  ~BLERemoteCharacteristic();
  bool canBroadcast();
  bool canIndicate();
  bool canNotify();
  bool canRead();
  bool canWrite();
  bool canWriteNoResponse();
  BLERemoteDescriptor *getDescriptor(BLEUUID uuid);
  std::map<std::string, BLERemoteDescriptor *> *getDescriptors();
  BLERemoteService *getRemoteService();
  uint16_t getHandle();
  BLEUUID getUUID();
  String readValue();
  uint8_t readUInt8();
  uint16_t readUInt16();
  uint32_t readUInt32();
  float readFloat();
  void registerForNotify(notify_callback _callback, bool notifications = true, bool descriptorRequiresRegistration = true);
  bool writeValue(uint8_t *data, size_t length, bool response = false);
  bool writeValue(String newValue, bool response = false);
  bool writeValue(uint8_t newValue, bool response = false);
  String toString();
  uint8_t *readRawData();
  void setAuth(uint8_t auth);

  /***************************************************************************
   *                       NimBLE public declarations                        *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  bool subscribe(bool notifications = true, notify_callback notifyCallback = nullptr, bool response = true);
  bool unsubscribe(bool response = true);
#endif

private:
  friend class BLEClient;
  friend class BLERemoteService;
  friend class BLERemoteDescriptor;

  /***************************************************************************
   *                       Common private properties                        *
   ***************************************************************************/

  BLEUUID m_uuid;
  uint8_t m_charProp;
  uint8_t m_auth;
  uint16_t m_handle;
  BLERemoteService *m_pRemoteService;
  FreeRTOS::Semaphore m_semaphoreReadCharEvt = FreeRTOS::Semaphore("ReadCharEvt");
  FreeRTOS::Semaphore m_semaphoreRegForNotifyEvt = FreeRTOS::Semaphore("RegForNotifyEvt");
  FreeRTOS::Semaphore m_semaphoreWriteCharEvt = FreeRTOS::Semaphore("WriteCharEvt");
  String m_value;
  uint8_t *m_rawData;
  notify_callback m_notifyCallback;

  // We maintain a map of descriptors owned by this characteristic keyed by a string representation of the UUID.
  std::map<std::string, BLERemoteDescriptor *> m_descriptorMap;

  /***************************************************************************
   *                       NimBLE private properties                        *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  uint16_t m_defHandle;
  uint16_t m_endHandle;
#endif

  /***************************************************************************
   *                       Common private declarations                       *
   ***************************************************************************/

  void removeDescriptors();

  /***************************************************************************
   *                       Bluedroid private declarations                    *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  BLERemoteCharacteristic(uint16_t handle, BLEUUID uuid, uint8_t charProp, BLERemoteService *pRemoteService);
  void retrieveDescriptors();
  void gattClientEventHandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *evtParam);
#endif

  /***************************************************************************
   *                       NimBLE private declarations                       *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  BLERemoteCharacteristic(BLERemoteService *pRemoteservice, const struct ble_gatt_chr *chr);
  bool setNotify(uint16_t val, notify_callback notifyCallback = nullptr, bool response = true);
  bool retrieveDescriptors(const BLEUUID *uuid_filter = nullptr);
  static int onReadCB(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg);
  static int onWriteCB(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg);
  static int descriptorDiscCB(uint16_t conn_handle, const struct ble_gatt_error *error, uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg);
#endif
};  // BLERemoteCharacteristic

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* COMPONENTS_CPP_UTILS_BLEREMOTECHARACTERISTIC_H_ */
