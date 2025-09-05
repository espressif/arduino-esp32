/*
 * BLEUtils.h
 *
 *  Created on: Mar 25, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef COMPONENTS_CPP_UTILS_BLEUTILS_H_
#define COMPONENTS_CPP_UTILS_BLEUTILS_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/*****************************************************************************
 *                             Common includes                               *
 *****************************************************************************/

#include <string>
#include "BLEAddress.h"
#include "WString.h"
#include <freertos/task.h>

/*****************************************************************************
 *                            Bluedroid includes                             *
 *****************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gattc_api.h>
#include <esp_gatts_api.h>
#include <esp_gap_ble_api.h>
#endif

/*****************************************************************************
 *                              NimBLE includes                              *
 *****************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <stdlib.h>
#include <climits>
#include <host/ble_gap.h>
#endif

/*****************************************************************************
 *                             Common types                                  *
 *****************************************************************************/

typedef struct {
  void *peer_device;  // peer device BLEClient or BLEServer
  bool connected;     // connection status
  uint16_t mtu;       // negotiated MTU per peer device
} conn_status_t;

/*****************************************************************************
 *                             NimBLE types                                  *
 *****************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)

/**
 * @brief A structure to hold data for a task that is waiting for a response.
 * @details This structure is used in conjunction with BLEUtils::taskWait() and BLEUtils::taskRelease().
 * All items are optional, the m_pHandle will be set in taskWait().
 */
struct BLETaskData {
  BLETaskData(void *pInstance = nullptr, int flags = 0, void *buf = nullptr);
  ~BLETaskData();
  void *m_pInstance{nullptr};
  mutable int m_flags{0};
  void *m_pBuf{nullptr};

private:
  mutable void *m_pHandle{nullptr};  // semaphore or task handle
  friend class BLEUtils;
};

#endif

/*****************************************************************************
 *                         Forward declarations                              *
 *****************************************************************************/

class BLEClient;

/**
 * @brief A set of general %BLE utilities.
 */
class BLEUtils {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  static char *buildHexData(uint8_t *target, const uint8_t *source, uint8_t length);
  static String buildPrintData(uint8_t *source, size_t length);
  static const char *advDataTypeToString(uint8_t advType);
  static String characteristicPropertiesToString(uint8_t prop);

  /***************************************************************************
   *                      Bluedroid public declarations                      *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  static const char *addressTypeToString(esp_ble_addr_type_t type);
  static String adFlagsToString(uint8_t adFlags);
  static const char *devTypeToString(esp_bt_dev_type_t type);
  static esp_gatt_id_t buildGattId(esp_bt_uuid_t uuid, uint8_t inst_id = 0);
  static esp_gatt_srvc_id_t buildGattSrvcId(esp_gatt_id_t gattId, bool is_primary = true);
  static void dumpGapEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
  static void dumpGattClientEvent(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *evtParam);
  static void dumpGattServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *evtParam);
  static const char *eventTypeToString(esp_ble_evt_type_t eventType);
  static BLEClient *findByAddress(BLEAddress address);
  static BLEClient *findByConnId(uint16_t conn_id);
  static const char *gapEventToString(uint32_t eventType);
  static String gattCharacteristicUUIDToString(uint32_t characteristicUUID);
  static String gattClientEventTypeToString(esp_gattc_cb_event_t eventType);
  static String gattCloseReasonToString(esp_gatt_conn_reason_t reason);
  static String gattcServiceElementToString(esp_gattc_service_elem_t *pGATTCServiceElement);
  static String gattDescriptorUUIDToString(uint32_t descriptorUUID);
  static String gattServerEventTypeToString(esp_gatts_cb_event_t eventType);
  static String gattServiceIdToString(esp_gatt_srvc_id_t srvcId);
  static String gattServiceToString(uint32_t serviceId);
  static String gattStatusToString(esp_gatt_status_t status);
  static String getMember(uint32_t memberId);
  static void registerByAddress(BLEAddress address, BLEClient *pDevice);
  static void registerByConnId(uint16_t conn_id, BLEClient *pDevice);
  static const char *searchEventTypeToString(esp_gap_search_evt_t searchEvt);
#endif

  /***************************************************************************
   *                        NimBLE public declarations                       *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  static void dumpGapEvent(ble_gap_event *event, void *arg);
  static const char *gapEventToString(uint8_t eventType);
  static const char *returnCodeToString(int rc);
  static int checkConnParams(ble_gap_conn_params *params);
  static bool taskWait(const BLETaskData &taskData, uint32_t timeout);
  static void taskRelease(const BLETaskData &taskData, int rc = 0);
#endif
};

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* COMPONENTS_CPP_UTILS_BLEUTILS_H_ */
