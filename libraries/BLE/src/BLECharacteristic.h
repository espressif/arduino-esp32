/*
 * BLECharacteristic.h
 *
 *  Created on: Jun 22, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_BLECHARACTERISTIC_H_
#define COMPONENTS_CPP_UTILS_BLECHARACTERISTIC_H_
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)
#include <string>
#include <map>
#include "BLEUUID.h"
#include <esp_gatts_api.h>
#include <esp_gap_ble_api.h>
#include "BLEDescriptor.h"
#include "BLEValue.h"
#include "FreeRTOS.h"

class BLEService;
class BLEDescriptor;
class BLECharacteristicCallbacks;

/**
 * @brief A management structure for %BLE descriptors.
 */
class BLEDescriptorMap {
public:
	void setByUUID(const char* uuid, BLEDescriptor* pDescriptor);
	void setByUUID(BLEUUID uuid, BLEDescriptor* pDescriptor);
	void setByHandle(uint16_t handle, BLEDescriptor* pDescriptor);
	BLEDescriptor* getByUUID(const char* uuid);
	BLEDescriptor* getByUUID(BLEUUID uuid);
	BLEDescriptor* getByHandle(uint16_t handle);
	std::string	toString();
	void handleGATTServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param);
	BLEDescriptor* getFirst();
	BLEDescriptor* getNext();
private:
	std::map<BLEDescriptor*, std::string> m_uuidMap;
	std::map<uint16_t, BLEDescriptor*> m_handleMap;
	std::map<BLEDescriptor*, std::string>::iterator m_iterator;
};


/**
 * @brief The model of a %BLE Characteristic.
 *
 * A BLE Characteristic is an identified value container that manages a value. It is exposed by a BLE server and
 * can be read and written to by a %BLE client.
 */
class BLECharacteristic {
public:
	BLECharacteristic(const char* uuid, uint32_t properties = 0);
	BLECharacteristic(BLEUUID uuid, uint32_t properties = 0);
	virtual ~BLECharacteristic();

	void           addDescriptor(BLEDescriptor* pDescriptor);
	BLEDescriptor* getDescriptorByUUID(const char* descriptorUUID);
	BLEDescriptor* getDescriptorByUUID(BLEUUID descriptorUUID);
	BLEUUID        getUUID();
	std::string    getValue();
	uint8_t*       getData();

	void indicate();
	void notify(bool is_notification = true);
	void setBroadcastProperty(bool value);
	void setCallbacks(BLECharacteristicCallbacks* pCallbacks);
	void setIndicateProperty(bool value);
	void setNotifyProperty(bool value);
	void setReadProperty(bool value);
	void setValue(uint8_t* data, size_t size);
	void setValue(std::string value);
	void setValue(uint16_t& data16);
	void setValue(uint32_t& data32);
	void setValue(int& data32);
	void setValue(float& data32);
	void setValue(double& data64); 
	void setWriteProperty(bool value);
	void setWriteNoResponseProperty(bool value);
	std::string toString();
	uint16_t getHandle();
	void setAccessPermissions(esp_gatt_perm_t perm);

	static const uint32_t PROPERTY_READ      = 1<<0;
	static const uint32_t PROPERTY_WRITE     = 1<<1;
	static const uint32_t PROPERTY_NOTIFY    = 1<<2;
	static const uint32_t PROPERTY_BROADCAST = 1<<3;
	static const uint32_t PROPERTY_INDICATE  = 1<<4;
	static const uint32_t PROPERTY_WRITE_NR  = 1<<5;

	static const uint32_t indicationTimeout = 1000;

private:

	friend class BLEServer;
	friend class BLEService;
	friend class BLEDescriptor;
	friend class BLECharacteristicMap;

	BLEUUID                     m_bleUUID;
	BLEDescriptorMap            m_descriptorMap;
	uint16_t                    m_handle;
	esp_gatt_char_prop_t        m_properties;
	BLECharacteristicCallbacks* m_pCallbacks;
	BLEService*                 m_pService;
	BLEValue                    m_value;
	esp_gatt_perm_t             m_permissions = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE;

	void handleGATTServerEvent(
			esp_gatts_cb_event_t      event,
			esp_gatt_if_t             gatts_if,
			esp_ble_gatts_cb_param_t* param);

	void                 executeCreate(BLEService* pService);
	esp_gatt_char_prop_t getProperties();
	BLEService*          getService();
	void                 setHandle(uint16_t handle);
	FreeRTOS::Semaphore m_semaphoreCreateEvt = FreeRTOS::Semaphore("CreateEvt");
	FreeRTOS::Semaphore m_semaphoreConfEvt   = FreeRTOS::Semaphore("ConfEvt");
	FreeRTOS::Semaphore m_semaphoreSetValue  = FreeRTOS::Semaphore("SetValue");  
}; // BLECharacteristic


/**
 * @brief Callbacks that can be associated with a %BLE characteristic to inform of events.
 *
 * When a server application creates a %BLE characteristic, we may wish to be informed when there is either
 * a read or write request to the characteristic's value. An application can register a
 * sub-classed instance of this class and will be notified when such an event happens.
 */
class BLECharacteristicCallbacks {
public:
	typedef enum {
		SUCCESS_INDICATE,
		SUCCESS_NOTIFY,
		ERROR_INDICATE_DISABLED,
		ERROR_NOTIFY_DISABLED,
		ERROR_GATT,
		ERROR_NO_CLIENT,
		ERROR_INDICATE_TIMEOUT,
		ERROR_INDICATE_FAILURE
	}Status;

	virtual ~BLECharacteristicCallbacks();
	virtual void onRead(BLECharacteristic* pCharacteristic);
	virtual void onWrite(BLECharacteristic* pCharacteristic);
	virtual void onNotify(BLECharacteristic* pCharacteristic);
	virtual void onStatus(BLECharacteristic* pCharacteristic, Status s, uint32_t code);
};
#endif /* CONFIG_BT_ENABLED */
#endif /* COMPONENTS_CPP_UTILS_BLECHARACTERISTIC_H_ */
