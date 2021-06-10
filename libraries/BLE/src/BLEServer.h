/*
 * BLEServer.h
 *
 *  Created on: Apr 16, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_BLESERVER_H_
#define COMPONENTS_CPP_UTILS_BLESERVER_H_
#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gatts_api.h>

#include <string>
#include <string.h>
// #include "BLEDevice.h"

#include "BLEUUID.h"
#include "BLEAdvertising.h"
#include "BLECharacteristic.h"
#include "BLEService.h"
#include "BLESecurity.h"
#include "RTOS.h"
#include "BLEAddress.h"

class BLEServerCallbacks;
/* TODO possibly refactor this struct */ 
typedef struct {
	void *peer_device;		// peer device BLEClient or BLEServer - maybe its better to have 2 structures or union here
	bool connected;			// do we need it?
	uint16_t mtu;			// every peer device negotiate own mtu
} conn_status_t;


/**
 * @brief A data structure that manages the %BLE servers owned by a BLE server.
 */
class BLEServiceMap {
public:
	BLEService* getByHandle(uint16_t handle);
	BLEService* getByUUID(const char* uuid);	
	BLEService* getByUUID(BLEUUID uuid, uint8_t inst_id = 0);
	void        handleGATTServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param);
	void        setByHandle(uint16_t handle, BLEService* service);
	void        setByUUID(const char* uuid, BLEService* service);
	void        setByUUID(BLEUUID uuid, BLEService* service);
	std::string toString();
	BLEService* getFirst();
	BLEService* getNext();
	void 		removeService(BLEService *service);
	int 		getRegisteredServiceCount();

private:
	std::map<uint16_t, BLEService*>    m_handleMap;
	std::map<BLEService*, std::string> m_uuidMap;
	std::map<BLEService*, std::string>::iterator m_iterator;
};


/**
 * @brief The model of a %BLE server.
 */
class BLEServer {
public:
	uint32_t        getConnectedCount();
	BLEService*     createService(const char* uuid);	
	BLEService*     createService(BLEUUID uuid, uint32_t numHandles=15, uint8_t inst_id=0);
	BLEAdvertising* getAdvertising();
	void            setCallbacks(BLEServerCallbacks* pCallbacks);
	void            startAdvertising();
	void 			removeService(BLEService* service);
	BLEService* 	getServiceByUUID(const char* uuid);
	BLEService* 	getServiceByUUID(BLEUUID uuid);
	bool 			connect(BLEAddress address);
	void 			disconnect(uint16_t connId);
	uint16_t		m_appId;
	void			updateConnParams(esp_bd_addr_t remote_bda, uint16_t minInterval, uint16_t maxInterval, uint16_t latency, uint16_t timeout);

	/* multi connection support */
	std::map<uint16_t, conn_status_t> getPeerDevices(bool client);
	void addPeerDevice(void* peer, bool is_client, uint16_t conn_id);
	bool removePeerDevice(uint16_t conn_id, bool client);
	BLEServer* getServerByConnId(uint16_t conn_id);
	void updatePeerMTU(uint16_t connId, uint16_t mtu);
	uint16_t getPeerMTU(uint16_t conn_id);
	uint16_t        getConnId();


private:
	BLEServer();
	friend class BLEService;
	friend class BLECharacteristic;
	friend class BLEDevice;
	esp_ble_adv_data_t  m_adv_data;
	// BLEAdvertising      m_bleAdvertising;
	uint16_t			m_connId;
	uint32_t            m_connectedCount;
	uint16_t            m_gatts_if;
  	std::map<uint16_t, conn_status_t> m_connectedServersMap;

	FreeRTOS::Semaphore m_semaphoreRegisterAppEvt 	= FreeRTOS::Semaphore("RegisterAppEvt");
	FreeRTOS::Semaphore m_semaphoreCreateEvt 		= FreeRTOS::Semaphore("CreateEvt");
	FreeRTOS::Semaphore m_semaphoreOpenEvt   		= FreeRTOS::Semaphore("OpenEvt");
	BLEServiceMap       m_serviceMap;
	BLEServerCallbacks* m_pServerCallbacks = nullptr;

	void            createApp(uint16_t appId);
	uint16_t        getGattsIf();
	void            handleGATTServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
	void            registerApp(uint16_t);
}; // BLEServer


/**
 * @brief Callbacks associated with the operation of a %BLE server.
 */
class BLEServerCallbacks {
public:
	virtual ~BLEServerCallbacks() {};
	/**
	 * @brief Handle a new client connection.
	 *
	 * When a new client connects, we are invoked.
	 *
	 * @param [in] pServer A reference to the %BLE server that received the client connection.
	 */
	virtual void onConnect(BLEServer* pServer);
	virtual void onConnect(BLEServer* pServer, esp_ble_gatts_cb_param_t *param);
	/**
	 * @brief Handle an existing client disconnection.
	 *
	 * When an existing client disconnects, we are invoked.
	 *
	 * @param [in] pServer A reference to the %BLE server that received the existing client disconnection.
	 */
	virtual void onDisconnect(BLEServer* pServer);

	/**
	 * @brief Handle a new client connection.
	 *
	 * When the MTU changes this method is invoked.
	 *
	 * @param [in] pServer A reference to the %BLE server that received the client connection.
	 * @param [in] param A reference to esp_ble_gatts_cb_param_t.
	 */
	virtual void onMtuChanged(BLEServer* pServer, esp_ble_gatts_cb_param_t* param);
}; // BLEServerCallbacks


#endif /* CONFIG_BLUEDROID_ENABLED */
#endif /* COMPONENTS_CPP_UTILS_BLESERVER_H_ */
