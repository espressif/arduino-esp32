/*
 * BLERemoteCharacteristic.cpp
 *
 *  Created on: Jul 8, 2017
 *      Author: kolban
 */

#include "BLERemoteCharacteristic.h"

#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include <esp_gattc_api.h>
#include <esp_err.h>

#include <sstream>
#include "BLEExceptions.h"
#include "BLEUtils.h"
#include "GeneralUtils.h"
#include "BLERemoteDescriptor.h"
#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "BLERemoteCharacteristic";   // The logging tag for this class.
#endif



/**
 * @brief Constructor.
 * @param [in] handle The BLE server side handle of this characteristic.
 * @param [in] uuid The UUID of this characteristic.
 * @param [in] charProp The properties of this characteristic.
 * @param [in] pRemoteService A reference to the remote service to which this remote characteristic pertains.
 */
BLERemoteCharacteristic::BLERemoteCharacteristic(
		uint16_t             handle,
		BLEUUID              uuid,
		esp_gatt_char_prop_t charProp,
		BLERemoteService*    pRemoteService) {
	ESP_LOGD(LOG_TAG, ">> BLERemoteCharacteristic: handle: %d 0x%d, uuid: %s", handle, handle, uuid.toString().c_str());
	m_handle         = handle;
	m_uuid           = uuid;
	m_charProp       = charProp;
	m_pRemoteService = pRemoteService;
	m_notifyCallback = nullptr;

	retrieveDescriptors(); // Get the descriptors for this characteristic
	ESP_LOGD(LOG_TAG, "<< BLERemoteCharacteristic");
} // BLERemoteCharacteristic


/**
 *@brief Destructor.
 */
BLERemoteCharacteristic::~BLERemoteCharacteristic() {
	removeDescriptors();   // Release resources for any descriptor information we may have allocated.
} // ~BLERemoteCharacteristic


/**
 * @brief Does the characteristic support broadcasting?
 * @return True if the characteristic supports broadcasting.
 */
bool BLERemoteCharacteristic::canBroadcast() {
	return (m_charProp & ESP_GATT_CHAR_PROP_BIT_BROADCAST) != 0;
} // canBroadcast


/**
 * @brief Does the characteristic support indications?
 * @return True if the characteristic supports indications.
 */
bool BLERemoteCharacteristic::canIndicate() {
	return (m_charProp & ESP_GATT_CHAR_PROP_BIT_INDICATE) != 0;
} // canIndicate


/**
 * @brief Does the characteristic support notifications?
 * @return True if the characteristic supports notifications.
 */
bool BLERemoteCharacteristic::canNotify() {
	return (m_charProp & ESP_GATT_CHAR_PROP_BIT_NOTIFY) != 0;
} // canNotify


/**
 * @brief Does the characteristic support reading?
 * @return True if the characteristic supports reading.
 */
bool BLERemoteCharacteristic::canRead() {
	return (m_charProp & ESP_GATT_CHAR_PROP_BIT_READ) != 0;
} // canRead


/**
 * @brief Does the characteristic support writing?
 * @return True if the characteristic supports writing.
 */
bool BLERemoteCharacteristic::canWrite() {
	return (m_charProp & ESP_GATT_CHAR_PROP_BIT_WRITE) != 0;
} // canWrite


/**
 * @brief Does the characteristic support writing with no response?
 * @return True if the characteristic supports writing with no response.
 */
bool BLERemoteCharacteristic::canWriteNoResponse() {
	return (m_charProp & ESP_GATT_CHAR_PROP_BIT_WRITE_NR) != 0;
} // canWriteNoResponse


/*
static bool compareSrvcId(esp_gatt_srvc_id_t id1, esp_gatt_srvc_id_t id2) {
	if (id1.id.inst_id != id2.id.inst_id) {
		return false;
	}
	if (!BLEUUID(id1.id.uuid).equals(BLEUUID(id2.id.uuid))) {
		return false;
	}
	return true;
} // compareSrvcId
*/

/*
static bool compareGattId(esp_gatt_id_t id1, esp_gatt_id_t id2) {
	if (id1.inst_id != id2.inst_id) {
		return false;
	}
	if (!BLEUUID(id1.uuid).equals(BLEUUID(id2.uuid))) {
		return false;
	}
	return true;
} // compareCharId
*/


/**
 * @brief Handle GATT Client events.
 * When an event arrives for a GATT client we give this characteristic the opportunity to
 * take a look at it to see if there is interest in it.
 * @param [in] event The type of event.
 * @param [in] gattc_if The interface on which the event was received.
 * @param [in] evtParam Payload data for the event.
 * @returns N/A
 */
void BLERemoteCharacteristic::gattClientEventHandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t* evtParam) {
	switch(event) {
		// ESP_GATTC_NOTIFY_EVT
		//
		// notify
		// - uint16_t           conn_id    - The connection identifier of the server.
		// - esp_bd_addr_t      remote_bda - The device address of the BLE server.
		// - uint16_t           handle     - The handle of the characteristic for which the event is being received.
		// - uint16_t           value_len  - The length of the received data.
		// - uint8_t*           value      - The received data.
		// - bool               is_notify  - True if this is a notify, false if it is an indicate.
		//
		// We have received a notification event which means that the server wishes us to know about a notification
		// piece of data.  What we must now do is find the characteristic with the associated handle and then
		// invoke its notification callback (if it has one).
		case ESP_GATTC_NOTIFY_EVT: {
			if (evtParam->notify.handle != getHandle()) break;
			if (m_notifyCallback != nullptr) {
				ESP_LOGD(LOG_TAG, "Invoking callback for notification on characteristic %s", toString().c_str());
				m_notifyCallback(this, evtParam->notify.value, evtParam->notify.value_len, evtParam->notify.is_notify);
			} // End we have a callback function ...
			break;
		} // ESP_GATTC_NOTIFY_EVT

		// ESP_GATTC_READ_CHAR_EVT
		// This event indicates that the server has responded to the read request.
		//
		// read:
		// - esp_gatt_status_t  status
		// - uint16_t           conn_id
		// - uint16_t           handle
		// - uint8_t*           value
		// - uint16_t           value_len
		case ESP_GATTC_READ_CHAR_EVT: {
			// If this event is not for us, then nothing further to do.
			if (evtParam->read.handle != getHandle()) break;

			// At this point, we have determined that the event is for us, so now we save the value
			// and unlock the semaphore to ensure that the requestor of the data can continue.
			if (evtParam->read.status == ESP_GATT_OK) {
				m_value = std::string((char*) evtParam->read.value, evtParam->read.value_len);
				if(m_rawData != nullptr) free(m_rawData);
				m_rawData = (uint8_t*) calloc(evtParam->read.value_len, sizeof(uint8_t));
				memcpy(m_rawData, evtParam->read.value, evtParam->read.value_len);
			} else {
				m_value = "";
			}

			m_semaphoreReadCharEvt.give();
			break;
		} // ESP_GATTC_READ_CHAR_EVT

		// ESP_GATTC_REG_FOR_NOTIFY_EVT
		//
		// reg_for_notify:
		// - esp_gatt_status_t status
		// - uint16_t          handle
		case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
			// If the request is not for this BLERemoteCharacteristic then move on to the next.
			if (evtParam->reg_for_notify.handle != getHandle()) break;

			// We have processed the notify registration and can unlock the semaphore.
			m_semaphoreRegForNotifyEvt.give();
			break;
		} // ESP_GATTC_REG_FOR_NOTIFY_EVT

		// ESP_GATTC_UNREG_FOR_NOTIFY_EVT
		//
		// unreg_for_notify:
		// - esp_gatt_status_t status
		// - uint16_t          handle
		case ESP_GATTC_UNREG_FOR_NOTIFY_EVT: {
			if (evtParam->unreg_for_notify.handle != getHandle()) break;
			// We have processed the notify un-registration and can unlock the semaphore.
			m_semaphoreRegForNotifyEvt.give();
			break;
		} // ESP_GATTC_UNREG_FOR_NOTIFY_EVT:

		// ESP_GATTC_WRITE_CHAR_EVT
		//
		// write:
		// - esp_gatt_status_t status
		// - uint16_t          conn_id
		// - uint16_t          handle
		case ESP_GATTC_WRITE_CHAR_EVT: {
			// Determine if this event is for us and, if not, pass onwards.
			if (evtParam->write.handle != getHandle()) break;

			// There is nothing further we need to do here.  This is merely an indication
			// that the write has completed and we can unlock the caller.
			m_semaphoreWriteCharEvt.give();
			break;
		} // ESP_GATTC_WRITE_CHAR_EVT


		default:
			break;
	} // End switch
}; // gattClientEventHandler


/**
 * @brief Populate the descriptors (if any) for this characteristic.
 */
void BLERemoteCharacteristic::retrieveDescriptors() {
	ESP_LOGD(LOG_TAG, ">> retrieveDescriptors() for characteristic: %s", getUUID().toString().c_str());

	removeDescriptors();   // Remove any existing descriptors.

	// Loop over each of the descriptors within the service associated with this characteristic.
	// For each descriptor we find, create a BLERemoteDescriptor instance.
	uint16_t offset = 0;
	esp_gattc_descr_elem_t result;
	while(true) {
		uint16_t count = 10;
		esp_gatt_status_t status = ::esp_ble_gattc_get_all_descr(
			getRemoteService()->getClient()->getGattcIf(),
			getRemoteService()->getClient()->getConnId(),
			getHandle(),
			&result,
			&count,
			offset
		);

		if (status == ESP_GATT_INVALID_OFFSET) {   // We have reached the end of the entries.
			break;
		}

		if (status != ESP_GATT_OK) {
			ESP_LOGE(LOG_TAG, "esp_ble_gattc_get_all_descr: %s", BLEUtils::gattStatusToString(status).c_str());
			break;
		}

		if (count == 0) break;

		ESP_LOGD(LOG_TAG, "Found a descriptor: Handle: %d, UUID: %s", result.handle, BLEUUID(result.uuid).toString().c_str());

		// We now have a new characteristic ... let us add that to our set of known characteristics
		BLERemoteDescriptor* pNewRemoteDescriptor = new BLERemoteDescriptor(
			result.handle,
			BLEUUID(result.uuid),
			this
		);

		m_descriptorMap.insert(std::pair<std::string, BLERemoteDescriptor*>(pNewRemoteDescriptor->getUUID().toString(), pNewRemoteDescriptor));

		offset++;
	} // while true
	//m_haveCharacteristics = true; // Remember that we have received the characteristics.
	ESP_LOGD(LOG_TAG, "<< retrieveDescriptors(): Found %d descriptors.", offset);
} // getDescriptors


/**
 * @brief Retrieve the map of descriptors keyed by UUID.
 */
std::map<std::string, BLERemoteDescriptor*>* BLERemoteCharacteristic::getDescriptors() {
	return &m_descriptorMap;
} // getDescriptors


/**
 * @brief Get the handle for this characteristic.
 * @return The handle for this characteristic.
 */
uint16_t BLERemoteCharacteristic::getHandle() {
	//ESP_LOGD(LOG_TAG, ">> getHandle: Characteristic: %s", getUUID().toString().c_str());
	//ESP_LOGD(LOG_TAG, "<< getHandle: %d 0x%.2x", m_handle, m_handle);
	return m_handle;
} // getHandle


/**
 * @brief Get the descriptor instance with the given UUID that belongs to this characteristic.
 * @param [in] uuid The UUID of the descriptor to find.
 * @return The Remote descriptor (if present) or null if not present.
 */
BLERemoteDescriptor* BLERemoteCharacteristic::getDescriptor(BLEUUID uuid) {
	ESP_LOGD(LOG_TAG, ">> getDescriptor: uuid: %s", uuid.toString().c_str());
	std::string v = uuid.toString();
	for (auto &myPair : m_descriptorMap) {
		if (myPair.first == v) {
			ESP_LOGD(LOG_TAG, "<< getDescriptor: found");
			return myPair.second;
		}
	}
	ESP_LOGD(LOG_TAG, "<< getDescriptor: Not found");
	return nullptr;
} // getDescriptor


/**
 * @brief Get the remote service associated with this characteristic.
 * @return The remote service associated with this characteristic.
 */
BLERemoteService* BLERemoteCharacteristic::getRemoteService() {
	return m_pRemoteService;
} // getRemoteService


/**
 * @brief Get the UUID for this characteristic.
 * @return The UUID for this characteristic.
 */
BLEUUID BLERemoteCharacteristic::getUUID() {
	return m_uuid;
} // getUUID


/**
 * @brief Read an unsigned 16 bit value
 * @return The unsigned 16 bit value.
 */
uint16_t BLERemoteCharacteristic::readUInt16() {
	std::string value = readValue();
	if (value.length() >= 2) {
		return *(uint16_t*)(value.data());
	}
	return 0;
} // readUInt16


/**
 * @brief Read an unsigned 32 bit value.
 * @return the unsigned 32 bit value.
 */
uint32_t BLERemoteCharacteristic::readUInt32() {
	std::string value = readValue();
	if (value.length() >= 4) {
		return *(uint32_t*)(value.data());
	}
	return 0;
} // readUInt32


/**
 * @brief Read a byte value
 * @return The value as a byte
 */
uint8_t BLERemoteCharacteristic::readUInt8() {
	std::string value = readValue();
	if (value.length() >= 1) {
		return (uint8_t)value[0];
	}
	return 0;
} // readUInt8


/**
 * @brief Read the value of the remote characteristic.
 * @return The value of the remote characteristic.
 */
std::string BLERemoteCharacteristic::readValue() {
	ESP_LOGD(LOG_TAG, ">> readValue(): uuid: %s, handle: %d 0x%.2x", getUUID().toString().c_str(), getHandle(), getHandle());

	// Check to see that we are connected.
	if (!getRemoteService()->getClient()->isConnected()) {
		ESP_LOGE(LOG_TAG, "Disconnected");
		throw BLEDisconnectedException();
	}

	m_semaphoreReadCharEvt.take("readValue");

	// Ask the BLE subsystem to retrieve the value for the remote hosted characteristic.
	// This is an asynchronous request which means that we must block waiting for the response
	// to become available.
	esp_err_t errRc = ::esp_ble_gattc_read_char(
		m_pRemoteService->getClient()->getGattcIf(),
		m_pRemoteService->getClient()->getConnId(),    // The connection ID to the BLE server
		getHandle(),                                   // The handle of this characteristic
		ESP_GATT_AUTH_REQ_NONE);                       // Security

	if (errRc != ESP_OK) {
		ESP_LOGE(LOG_TAG, "esp_ble_gattc_read_char: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
		return "";
	}

	// Block waiting for the event that indicates that the read has completed.  When it has, the std::string found
	// in m_value will contain our data.
	m_semaphoreReadCharEvt.wait("readValue");

	ESP_LOGD(LOG_TAG, "<< readValue(): length: %d", m_value.length());
	return m_value;
} // readValue


/**
 * @brief Register for notifications.
 * @param [in] notifyCallback A callback to be invoked for a notification.  If NULL is provided then we are
 * unregistering a notification.
 * @return N/A.
 */
void BLERemoteCharacteristic::registerForNotify(notify_callback notifyCallback, bool notifications) {
	ESP_LOGD(LOG_TAG, ">> registerForNotify(): %s", toString().c_str());

	m_notifyCallback = notifyCallback;   // Save the notification callback.

	m_semaphoreRegForNotifyEvt.take("registerForNotify");

	if (notifyCallback != nullptr) {   // If we have a callback function, then this is a registration.
		esp_err_t errRc = ::esp_ble_gattc_register_for_notify(
			m_pRemoteService->getClient()->getGattcIf(),
			*m_pRemoteService->getClient()->getPeerAddress().getNative(),
			getHandle()
		);

		if (errRc != ESP_OK) {
			ESP_LOGE(LOG_TAG, "esp_ble_gattc_register_for_notify: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
		}

		uint8_t val[] = {0x01, 0x00};
		if(!notifications) val[0] = 0x02;
		BLERemoteDescriptor* desc = getDescriptor(BLEUUID((uint16_t)0x2902));
		desc->writeValue(val, 2);
	} // End Register
	else {   // If we weren't passed a callback function, then this is an unregistration.
		esp_err_t errRc = ::esp_ble_gattc_unregister_for_notify(
			m_pRemoteService->getClient()->getGattcIf(),
			*m_pRemoteService->getClient()->getPeerAddress().getNative(),
			getHandle()
		);

		if (errRc != ESP_OK) {
			ESP_LOGE(LOG_TAG, "esp_ble_gattc_unregister_for_notify: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
		}

		uint8_t val[] = {0x00, 0x00};
		BLERemoteDescriptor* desc = getDescriptor((uint16_t)0x2902);
		desc->writeValue(val, 2);
	} // End Unregister

	m_semaphoreRegForNotifyEvt.wait("registerForNotify");

	ESP_LOGD(LOG_TAG, "<< registerForNotify()");
} // registerForNotify


/**
 * @brief Delete the descriptors in the descriptor map.
 * We maintain a map called m_descriptorMap that contains pointers to BLERemoteDescriptors
 * object references.  Since we allocated these in this class, we are also responsible for deleteing
 * them.  This method does just that.
 * @return N/A.
 */
void BLERemoteCharacteristic::removeDescriptors() {
	// Iterate through all the descriptors releasing their storage and erasing them from the map.
	for (auto &myPair : m_descriptorMap) {
	   m_descriptorMap.erase(myPair.first);
	   delete myPair.second;
	}
	m_descriptorMap.clear();   // Technically not neeeded, but just to be sure.
} // removeCharacteristics


/**
 * @brief Convert a BLERemoteCharacteristic to a string representation;
 * @return a String representation.
 */
std::string BLERemoteCharacteristic::toString() {
	std::ostringstream ss;
	ss << "Characteristic: uuid: " << m_uuid.toString() <<
		", handle: " << getHandle() << " 0x" << std::hex << getHandle() <<
		", props: " << BLEUtils::characteristicPropertiesToString(m_charProp);
	return ss.str();
} // toString


/**
 * @brief Write the new value for the characteristic.
 * @param [in] newValue The new value to write.
 * @param [in] response Do we expect a response?
 * @return N/A.
 */
void BLERemoteCharacteristic::writeValue(std::string newValue, bool response) {
	writeValue((uint8_t*)newValue.c_str(), strlen(newValue.c_str()), response);
} // writeValue


/**
 * @brief Write the new value for the characteristic.
 *
 * This is a convenience function.  Many BLE characteristics are a single byte of data.
 * @param [in] newValue The new byte value to write.
 * @param [in] response Whether we require a response from the write.
 * @return N/A.
 */
void BLERemoteCharacteristic::writeValue(uint8_t newValue, bool response) {
	writeValue(&newValue, 1, response);
} // writeValue


/**
 * @brief Write the new value for the characteristic from a data buffer.
 * @param [in] data A pointer to a data buffer.
 * @param [in] length The length of the data in the data buffer.
 * @param [in] response Whether we require a response from the write.
 */
void BLERemoteCharacteristic::writeValue(uint8_t* data, size_t length, bool response) {
	// writeValue(std::string((char*)data, length), response);
	ESP_LOGD(LOG_TAG, ">> writeValue(), length: %d", length);

	// Check to see that we are connected.
	if (!getRemoteService()->getClient()->isConnected()) {
		ESP_LOGE(LOG_TAG, "Disconnected");
		throw BLEDisconnectedException();
	}

	m_semaphoreWriteCharEvt.take("writeValue");
	// Invoke the ESP-IDF API to perform the write.
	esp_err_t errRc = ::esp_ble_gattc_write_char(
		m_pRemoteService->getClient()->getGattcIf(),
		m_pRemoteService->getClient()->getConnId(),
		getHandle(),
		length,
		data,
		response?ESP_GATT_WRITE_TYPE_RSP:ESP_GATT_WRITE_TYPE_NO_RSP,
		ESP_GATT_AUTH_REQ_NONE
	);

	if (errRc != ESP_OK) {
		ESP_LOGE(LOG_TAG, "esp_ble_gattc_write_char: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
		return;
	}

	m_semaphoreWriteCharEvt.wait("writeValue");

	ESP_LOGD(LOG_TAG, "<< writeValue");
} // writeValue

/**
 * @brief Read raw data from remote characteristic as hex bytes
 * @return return pointer data read
 */
uint8_t* BLERemoteCharacteristic::readRawData() {
	return m_rawData;
}

#endif /* CONFIG_BT_ENABLED */
