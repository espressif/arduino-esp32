/*
 * BLERemoteCharacteristic.h
 *
 *  Created on: Jul 8, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_BLEREMOTECHARACTERISTIC_H_
#define COMPONENTS_CPP_UTILS_BLEREMOTECHARACTERISTIC_H_
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include <string>

#include <esp_gattc_api.h>

#include "BLERemoteService.h"
#include "BLERemoteDescriptor.h"
#include "BLEUUID.h"
#include "RTOS.h"

class BLERemoteService;
class BLERemoteDescriptor;
typedef void (*notify_callback)(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);

/**
 * @brief A model of a remote %BLE characteristic.
 */
class BLERemoteCharacteristic {
public:
	~BLERemoteCharacteristic();

	// Public member functions
	bool        canBroadcast();
	bool        canIndicate();
	bool        canNotify();
	bool        canRead();
	bool        canWrite();
	bool        canWriteNoResponse();
	BLERemoteDescriptor* getDescriptor(BLEUUID uuid);
	std::map<std::string, BLERemoteDescriptor*>* getDescriptors();
	uint16_t    getHandle();
	BLEUUID     getUUID();
	std::string readValue();
	uint8_t     readUInt8();
	uint16_t    readUInt16();
	uint32_t    readUInt32();
	float       readFloat();
	void        registerForNotify(notify_callback _callback, bool notifications = true);
	void        writeValue(uint8_t* data, size_t length, bool response = false);
	void        writeValue(std::string newValue, bool response = false);
	void        writeValue(uint8_t newValue, bool response = false);
	std::string toString();
	uint8_t*	readRawData();
    void        setAuth(esp_gatt_auth_req_t auth);

private:
	BLERemoteCharacteristic(uint16_t handle, BLEUUID uuid, esp_gatt_char_prop_t charProp, BLERemoteService* pRemoteService);
	friend class BLEClient;
	friend class BLERemoteService;
	friend class BLERemoteDescriptor;

	// Private member functions
	void gattClientEventHandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t* evtParam);

	BLERemoteService* getRemoteService();
	void              removeDescriptors();
	void              retrieveDescriptors();

	// Private properties
	BLEUUID              m_uuid;
	esp_gatt_char_prop_t m_charProp;
    esp_gatt_auth_req_t  m_auth;
	uint16_t             m_handle;
	BLERemoteService*    m_pRemoteService;
	FreeRTOS::Semaphore  m_semaphoreReadCharEvt      = FreeRTOS::Semaphore("ReadCharEvt");
	FreeRTOS::Semaphore  m_semaphoreRegForNotifyEvt  = FreeRTOS::Semaphore("RegForNotifyEvt");
	FreeRTOS::Semaphore  m_semaphoreWriteCharEvt     = FreeRTOS::Semaphore("WriteCharEvt");
	std::string          m_value;
	uint8_t 			 *m_rawData;
	notify_callback		 m_notifyCallback;

	// We maintain a map of descriptors owned by this characteristic keyed by a string representation of the UUID.
	std::map<std::string, BLERemoteDescriptor*> m_descriptorMap;
}; // BLERemoteCharacteristic
#endif /* CONFIG_BT_ENABLED */
#endif /* COMPONENTS_CPP_UTILS_BLEREMOTECHARACTERISTIC_H_ */
