/*
 * BLEDevice.h
 *
 *  Created on: Mar 16, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef MAIN_BLEDevice_H_
#define MAIN_BLEDevice_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include <map>

#if defined(SOC_BLE_SUPPORTED)
#include <esp_bt.h>
#else
// For ESP32-P4 and other chips without native BLE support
// Define minimal types needed for interface compatibility
typedef int esp_power_level_t;
typedef int esp_ble_power_type_t;
#define ESP_BLE_PWR_TYPE_DEFAULT 0
#define ESP_PWR_LVL_N12          0
#endif

#include "WString.h"
#include "BLEServer.h"
#include "BLEClient.h"
#include "BLEUtils.h"
#include "BLEScan.h"
#include "BLEAdvertising.h"
#include "BLESecurity.h"
#include "BLEAddress.h"
#include "BLEUtils.h"
#include "BLEUUID.h"
#include "BLEAdvertisedDevice.h"

/***************************************************************************
 *                           Common definitions                            *
 ***************************************************************************/

enum class BLEStack {
  BLUEDROID,
  NIMBLE,
  UNKNOWN
};

/***************************************************************************
 *                           Bluedroid includes                            *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_gatts_api.h>
#endif

/***************************************************************************
 *                      NimBLE includes and definitions                    *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_gap.h>
#define ESP_GATT_IF_NONE BLE_HS_CONN_HANDLE_NONE

// Hosted HCI transport implementation is provided in BLEHostedHCI.cpp
// and is automatically linked when building for ESP32-P4

// NimBLE configuration compatibility macros
#if defined(CONFIG_SCAN_DUPLICATE_BY_DEVICE_ADDR) && !defined(CONFIG_BTDM_SCAN_DUPL_TYPE_DEVICE)
#define CONFIG_BTDM_SCAN_DUPL_TYPE_DEVICE CONFIG_SCAN_DUPLICATE_BY_DEVICE_ADDR
#endif

#if defined(CONFIG_SCAN_DUPLICATE_BY_ADV_DATA) && !defined(CONFIG_BTDM_SCAN_DUPL_TYPE_DATA)
#define CONFIG_BTDM_SCAN_DUPL_TYPE_DATA CONFIG_SCAN_DUPLICATE_BY_ADV_DATA
#endif

#if defined(CONFIG_SCAN_DUPLICATE_BY_ADV_DATA_AND_DEVICE_ADDR) && !defined(CONFIG_BTDM_SCAN_DUPL_TYPE_DATA_DEVICE)
#define CONFIG_BTDM_SCAN_DUPL_TYPE_DATA_DEVICE CONFIG_SCAN_DUPLICATE_BY_ADV_DATA_AND_DEVICE_ADDR
#endif

#if defined(CONFIG_SCAN_DUPLICATE_TYPE) && !defined(CONFIG_BTDM_SCAN_DUPL_TYPE)
#define CONFIG_BTDM_SCAN_DUPL_TYPE CONFIG_SCAN_DUPLICATE_TYPE
#endif

#if defined(CONFIG_BT_CTRL_SCAN_DUPL_TYPE) && !defined(CONFIG_BTDM_SCAN_DUPL_TYPE)
#define CONFIG_BTDM_SCAN_DUPL_TYPE CONFIG_BT_CTRL_SCAN_DUPL_TYPE
#endif

#if defined(CONFIG_BT_LE_SCAN_DUPL_TYPE) && !defined(CONFIG_BTDM_SCAN_DUPL_TYPE)
#define CONFIG_BTDM_SCAN_DUPL_TYPE CONFIG_BT_LE_SCAN_DUPL_TYPE
#endif

#if defined(CONFIG_DUPLICATE_SCAN_CACHE_SIZE) && !defined(CONFIG_BTDM_SCAN_DUPL_CACHE_SIZE)
#define CONFIG_BTDM_SCAN_DUPL_CACHE_SIZE CONFIG_DUPLICATE_SCAN_CACHE_SIZE
#endif

#if defined(CONFIG_BT_CTRL_SCAN_DUPL_CACHE_SIZE) && !defined(CONFIG_BTDM_SCAN_DUPL_CACHE_SIZE)
#define CONFIG_BTDM_SCAN_DUPL_CACHE_SIZE CONFIG_BT_CTRL_SCAN_DUPL_CACHE_SIZE
#endif

#if defined(CONFIG_BT_LE_LL_DUP_SCAN_LIST_COUNT) && !defined(CONFIG_BTDM_SCAN_DUPL_CACHE_SIZE)
#define CONFIG_BTDM_SCAN_DUPL_CACHE_SIZE CONFIG_BT_LE_LL_DUP_SCAN_LIST_COUNT
#endif

#if defined(CONFIG_NIMBLE_MAX_CONNECTIONS) && !defined(CONFIG_BT_NIMBLE_MAX_CONNECTIONS)
#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS CONFIG_NIMBLE_MAX_CONNECTIONS
#endif

#endif

/***************************************************************************
 *                           Forward declarations                          *
 ***************************************************************************/

class BLEAddress;
class BLEDeviceCallbacks;
class BLESecurityCallbacks;
class BLEServer;
class BLEScan;
class BLEAdvertising;
class BLEClient;
class BLESecurity;

/***************************************************************************
 *                        Bluedroid type definitions                       *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
typedef void (*gap_event_handler)(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
typedef void (*gattc_event_handler)(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
typedef void (*gatts_event_handler)(esp_gatts_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gatts_cb_param_t *param);
#endif

/***************************************************************************
 *                    NimBLE type definitions and externals                *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
extern "C" void ble_store_config_init(void);
typedef int (*gap_event_handler)(struct ble_gap_event *event, void *param);
#endif

class BLEDevice {
public:
  /***************************************************************************
   *                        Common public properties                         *
   ***************************************************************************/

  static uint16_t m_appId;
  static uint16_t m_localMTU;
  static gap_event_handler m_customGapHandler;

  /***************************************************************************
   *                        Bluedroid public properties                      *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  static gattc_event_handler m_customGattcHandler;
  static gatts_event_handler m_customGattsHandler;
#endif

  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  static BLEClient *createClient();
  static BLEServer *createServer();
  static BLEAddress getAddress();
  static BLEServer *getServer();
  static BLEScan *getScan();
  static String getValue(BLEAddress bdAddress, BLEUUID serviceUUID, BLEUUID characteristicUUID);
  static void init(String deviceName);
  static void setPower(esp_power_level_t powerLevel, esp_ble_power_type_t powerType = ESP_BLE_PWR_TYPE_DEFAULT);
  static int getPower(esp_ble_power_type_t powerType = ESP_BLE_PWR_TYPE_DEFAULT);
  static void setValue(BLEAddress bdAddress, BLEUUID serviceUUID, BLEUUID characteristicUUID, String value);
  static String toString();
  static void whiteListAdd(BLEAddress address);
  static void whiteListRemove(BLEAddress address);
  static void setSecurityCallbacks(BLESecurityCallbacks *pCallbacks);
  static esp_err_t setMTU(uint16_t mtu);
  static uint16_t getMTU();
  static bool getInitialized();
  static bool getPeerIRK(BLEAddress peerAddress, uint8_t *irk);
  static String getPeerIRKString(BLEAddress peerAddress);
  static String getPeerIRKBase64(BLEAddress peerAddress);
  static String getPeerIRKReverse(BLEAddress peerAddress);
  static BLEAdvertising *getAdvertising();
  static void startAdvertising();
  static void stopAdvertising();
  static std::map<uint16_t, conn_status_t> getPeerDevices(bool client);
  static void addPeerDevice(void *peer, bool is_client, uint16_t conn_id);
  static void updatePeerDevice(void *peer, bool _client, uint16_t conn_id);
  static void removePeerDevice(uint16_t conn_id, bool client);
  static BLEClient *getClientByID(uint16_t conn_id);
  static BLEClient *getClientByAddress(BLEAddress address);
  static BLEClient *getClientByGattIf(uint16_t conn_id);
  static void setCustomGapHandler(gap_event_handler handler);
  static void deinit(bool release_memory = false);
  static BLEStack getBLEStack();
  static String getBLEStackString();
  static bool isHostedBLE();

  /***************************************************************************
   *                       Bluedroid public declarations                    *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  static void setCustomGattcHandler(gattc_event_handler handler);
  static void setCustomGattsHandler(gatts_event_handler handler);
#endif

  /***************************************************************************
   *                       NimBLE public declarations                       *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  static void onReset(int reason);
  static void onSync(void);
  static void host_task(void *param);
  static bool setOwnAddrType(uint8_t type);
  static bool setOwnAddr(BLEAddress &addr);
  static bool setOwnAddr(uint8_t *addr);
  static void setDeviceCallbacks(BLEDeviceCallbacks *cb);
  static bool onWhiteList(BLEAddress &address);
#endif

private:
  friend class BLEClient;
  friend class BLEScan;
  friend class BLEServer;
  friend class BLECharacteristic;
  friend class BLEAdvertising;

  /***************************************************************************
   *                        Common private properties                        *
   ***************************************************************************/

  static BLEServer *m_pServer;
  static BLEScan *m_pScan;
  static BLEClient *m_pClient;
  static BLESecurityCallbacks *m_securityCallbacks;
  static BLEAdvertising *m_bleAdvertising;
  static std::map<uint16_t, conn_status_t> m_connectedClientsMap;
  static portMUX_TYPE mux;

  /***************************************************************************
   *                        NimBLE private properties                        *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  static uint8_t m_ownAddrType;
  static bool m_synced;
  static std::vector<BLEAddress> m_whiteList;
  static BLEDeviceCallbacks defaultDeviceCallbacks;
  static BLEDeviceCallbacks *m_pDeviceCallbacks;
  static ble_gap_event_listener m_listener;
#endif

  /***************************************************************************
   *                        Bluedroid private declarations                   *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  static esp_gatt_if_t getGattcIF();
  static void gattClientEventHandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
  static void gattServerEventHandler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
  static void gapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
#endif
};  // class BLE

/***************************************************************************
 *                       NimBLE specific classes                           *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
class BLEDeviceCallbacks {
public:
  virtual ~BLEDeviceCallbacks(){};
  virtual int onStoreStatus(struct ble_store_status_event *event, void *arg);
};
#endif

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* MAIN_BLEDevice_H_ */
