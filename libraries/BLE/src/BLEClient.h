/*
 * BLEDevice.h
 *
 *  Created on: Mar 22, 2017
 *      Author: kolban
 */

#ifndef MAIN_BLEDEVICE_H_
#define MAIN_BLEDEVICE_H_

#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)

#include <esp_gattc_api.h>
#include <string.h>
#include <map>
#include <string>
//#include "BLEExceptions.h"
#include "BLERemoteService.h"
#include "BLEService.h"
#include "BLEAddress.h"
#include "BLEAdvertisedDevice.h"

class BLERemoteService;
class BLEClientCallbacks;
class BLEAdvertisedDevice;

/**
 * @brief A model of a %BLE client.
 */
class BLEClient {
public:
  BLEClient();
  ~BLEClient();

  bool connect(BLEAdvertisedDevice* device);
  bool connectTimeout(BLEAdvertisedDevice* device, uint32_t timeoutMS = portMAX_DELAY);
  bool connect(BLEAddress address, esp_ble_addr_type_t type = BLE_ADDR_TYPE_PUBLIC, uint32_t timeoutMS = portMAX_DELAY);  // Connect to the remote BLE Server
  void disconnect();                                                                                                      // Disconnect from the remote BLE Server
  BLEAddress getPeerAddress();                                                                                            // Get the address of the remote BLE Server
  int getRssi();                                                                                                          // Get the RSSI of the remote BLE Server
  std::map<String, BLERemoteService*>* getServices();                                                                     // Get a map of the services offered by the remote BLE Server
  BLERemoteService* getService(const char* uuid);                                                                         // Get a reference to a specified service offered by the remote BLE server.
  BLERemoteService* getService(BLEUUID uuid);                                                                             // Get a reference to a specified service offered by the remote BLE server.
  String getValue(BLEUUID serviceUUID, BLEUUID characteristicUUID);                                                       // Get the value of a given characteristic at a given service.

  void handleGAPEvent(
    esp_gap_ble_cb_event_t event,
    esp_ble_gap_cb_param_t* param);

  bool isConnected();  // Return true if we are connected.

  void setClientCallbacks(BLEClientCallbacks* pClientCallbacks);
  void setValue(BLEUUID serviceUUID, BLEUUID characteristicUUID, String value);  // Set the value of a given characteristic at a given service.

  String toString();  // Return a string representation of this client.
  uint16_t getConnId();
  esp_gatt_if_t getGattcIf();
  uint16_t getMTU();
  bool setMTU(uint16_t mtu);

  uint16_t m_appId;
private:
  friend class BLEDevice;
  friend class BLERemoteService;
  friend class BLERemoteCharacteristic;
  friend class BLERemoteDescriptor;

  void gattClientEventHandler(
    esp_gattc_cb_event_t event,
    esp_gatt_if_t gattc_if,
    esp_ble_gattc_cb_param_t* param);

  BLEAddress m_peerAddress = BLEAddress((uint8_t*)"\0\0\0\0\0\0");  // The BD address of the remote server.
  uint16_t m_conn_id;
  //  int           m_deviceType;
  esp_gatt_if_t m_gattc_if;
  bool m_haveServices = false;  // Have we previously obtain the set of services from the remote server.
  bool m_isConnected = false;   // Are we currently connected.

  BLEClientCallbacks* m_pClientCallbacks;
  FreeRTOS::Semaphore m_semaphoreRegEvt = FreeRTOS::Semaphore("RegEvt");
  FreeRTOS::Semaphore m_semaphoreOpenEvt = FreeRTOS::Semaphore("OpenEvt");
  FreeRTOS::Semaphore m_semaphoreSearchCmplEvt = FreeRTOS::Semaphore("SearchCmplEvt");
  FreeRTOS::Semaphore m_semaphoreRssiCmplEvt = FreeRTOS::Semaphore("RssiCmplEvt");
  std::map<String, BLERemoteService*> m_servicesMap;
  std::map<BLERemoteService*, uint16_t> m_servicesMapByInstID;
  void clearServices();  // Clear any existing services.
  uint16_t m_mtu = 23;
};  // class BLEDevice


/**
 * @brief Callbacks associated with a %BLE client.
 */
class BLEClientCallbacks {
public:
  virtual ~BLEClientCallbacks(){};
  virtual void onConnect(BLEClient* pClient) = 0;
  virtual void onDisconnect(BLEClient* pClient) = 0;
};

#endif /* CONFIG_BLUEDROID_ENABLED */
#endif /* SOC_BLE_SUPPORTED */
#endif /* MAIN_BLEDEVICE_H_ */
