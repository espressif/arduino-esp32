/*
 * BLEService.h
 *
 *  Created on: Mar 25, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_BLESERVICE_H_
#define COMPONENTS_CPP_UTILS_BLESERVICE_H_
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include <esp_gatts_api.h>

#include "BLECharacteristic.h"
#include "BLEServer.h"
#include "BLEUUID.h"
#include "RTOS.h"

class BLEServer;

/**
 * @brief A data mapping used to manage the set of %BLE characteristics known to the server.
 */
class BLECharacteristicMap {
public:
	void setByUUID(BLECharacteristic* pCharacteristic, const char* uuid);
	void setByUUID(BLECharacteristic* pCharacteristic, BLEUUID uuid);
	void setByHandle(uint16_t handle, BLECharacteristic* pCharacteristic);
	BLECharacteristic* getByUUID(const char* uuid);	
	BLECharacteristic* getByUUID(BLEUUID uuid);
	BLECharacteristic* getByHandle(uint16_t handle);
	BLECharacteristic* getFirst();
	BLECharacteristic* getNext();
	std::string toString();
	void handleGATTServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param);

private:
	std::map<BLECharacteristic*, std::string> m_uuidMap;
	std::map<uint16_t, BLECharacteristic*> m_handleMap;
	std::map<BLECharacteristic*, std::string>::iterator m_iterator;
};


/**
 * @brief The model of a %BLE service.
 *
 */
class BLEService {
public:
	void               addCharacteristic(BLECharacteristic* pCharacteristic);
	BLECharacteristic* createCharacteristic(const char* uuid, uint32_t properties);
	BLECharacteristic* createCharacteristic(BLEUUID uuid, uint32_t properties);
	void               dump();
	void               executeCreate(BLEServer* pServer);
	void			   executeDelete();
	BLECharacteristic* getCharacteristic(const char* uuid);
	BLECharacteristic* getCharacteristic(BLEUUID uuid);
	BLEUUID            getUUID();
	BLEServer*         getServer();
	void               start();
	void			   stop();
	std::string        toString();
	uint16_t           getHandle();
	uint8_t			   m_instId = 0;

private:
	BLEService(const char* uuid, uint16_t numHandles);
	BLEService(BLEUUID uuid, uint16_t numHandles);
	friend class BLEServer;
	friend class BLEServiceMap;
	friend class BLEDescriptor;
	friend class BLECharacteristic;
	friend class BLEDevice;

	BLECharacteristicMap m_characteristicMap;
	uint16_t             m_handle;
	BLECharacteristic*   m_lastCreatedCharacteristic = nullptr;
	BLEServer*           m_pServer = nullptr;
	BLEUUID              m_uuid;

	FreeRTOS::Semaphore  m_semaphoreCreateEvt = FreeRTOS::Semaphore("CreateEvt");
	FreeRTOS::Semaphore  m_semaphoreDeleteEvt = FreeRTOS::Semaphore("DeleteEvt");
	FreeRTOS::Semaphore  m_semaphoreStartEvt  = FreeRTOS::Semaphore("StartEvt");
	FreeRTOS::Semaphore  m_semaphoreStopEvt   = FreeRTOS::Semaphore("StopEvt");

	uint16_t             m_numHandles;

	BLECharacteristic* getLastCreatedCharacteristic();
	void handleGATTServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param);
	void               setHandle(uint16_t handle);
	//void               setService(esp_gatt_srvc_id_t srvc_id);
}; // BLEService


#endif // CONFIG_BT_ENABLED
#endif /* COMPONENTS_CPP_UTILS_BLESERVICE_H_ */
