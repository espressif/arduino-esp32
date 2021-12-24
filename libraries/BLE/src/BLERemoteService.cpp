/*
 * BLERemoteService.cpp
 *
 *  Created on: Jul 8, 2017
 *      Author: kolban
 */
#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)

#include <sstream>
#include "BLERemoteService.h"
#include "BLEUtils.h"
#include "GeneralUtils.h"
#include <esp_err.h>
#include "esp32-hal-log.h"

#pragma GCC diagnostic warning "-Wunused-but-set-parameter"

BLERemoteService::BLERemoteService(
		esp_gatt_id_t srvcId,
		BLEClient*    pClient,
		uint16_t      startHandle,
		uint16_t      endHandle
	) {

	log_v(">> BLERemoteService()");
	m_srvcId  = srvcId;
	m_pClient = pClient;
	m_uuid    = BLEUUID(m_srvcId);
	m_haveCharacteristics = false;
	m_startHandle = startHandle;
	m_endHandle = endHandle;

	log_v("<< BLERemoteService()");
}


BLERemoteService::~BLERemoteService() {
	removeCharacteristics();
}

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

/**
 * @brief Handle GATT Client events
 */
void BLERemoteService::gattClientEventHandler(
	esp_gattc_cb_event_t      event,
	esp_gatt_if_t             gattc_if,
	esp_ble_gattc_cb_param_t* evtParam) {
	switch (event) {
		//
		// ESP_GATTC_GET_CHAR_EVT
		//
		// get_char:
		// - esp_gatt_status_t    status
		// - uin1t6_t             conn_id
		// - esp_gatt_srvc_id_t   srvc_id
		// - esp_gatt_id_t        char_id
		// - esp_gatt_char_prop_t char_prop
		//
	/*
		case ESP_GATTC_GET_CHAR_EVT: {
			// Is this event for this service?  If yes, then the local srvc_id and the event srvc_id will be
			// the same.
			if (compareSrvcId(m_srvcId, evtParam->get_char.srvc_id) == false) {
				break;
			}

			// If the status is NOT OK then we have a problem and continue.
			if (evtParam->get_char.status != ESP_GATT_OK) {
				m_semaphoreGetCharEvt.give();
				break;
			}

			// This is an indication that we now have the characteristic details for a characteristic owned
			// by this service so remember it.
			m_characteristicMap.insert(std::pair<std::string, BLERemoteCharacteristic*>(
					BLEUUID(evtParam->get_char.char_id.uuid).toString(),
					new BLERemoteCharacteristic(evtParam->get_char.char_id, evtParam->get_char.char_prop, this)	));


			// Now that we have received a characteristic, lets ask for the next one.
			esp_err_t errRc = ::esp_ble_gattc_get_characteristic(
					m_pClient->getGattcIf(),
					m_pClient->getConnId(),
					&m_srvcId,
					&evtParam->get_char.char_id);
			if (errRc != ESP_OK) {
				log_e("esp_ble_gattc_get_characteristic: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
				break;
			}

			//m_semaphoreGetCharEvt.give();
			break;
		} // ESP_GATTC_GET_CHAR_EVT
*/
		default:
			break;
	} // switch

	// Send the event to each of the characteristics owned by this service.
	for (auto &myPair : m_characteristicMapByHandle) {
	   myPair.second->gattClientEventHandler(event, gattc_if, evtParam);
	}
} // gattClientEventHandler


/**
 * @brief Get the remote characteristic object for the characteristic UUID.
 * @param [in] uuid Remote characteristic uuid.
 * @return Reference to the remote characteristic object.
 * @throws BLEUuidNotFoundException
 */
BLERemoteCharacteristic* BLERemoteService::getCharacteristic(const char* uuid) {
    return getCharacteristic(BLEUUID(uuid));
} // getCharacteristic
	
/**
 * @brief Get the characteristic object for the UUID.
 * @param [in] uuid Characteristic uuid.
 * @return Reference to the characteristic object.
 * @throws BLEUuidNotFoundException
 */
BLERemoteCharacteristic* BLERemoteService::getCharacteristic(BLEUUID uuid) {
// Design
// ------
// We wish to retrieve the characteristic given its UUID.  It is possible that we have not yet asked the
// device what characteristics it has in which case we have nothing to match against.  If we have not
// asked the device about its characteristics, then we do that now.  Once we get the results we can then
// examine the characteristics map to see if it has the characteristic we are looking for.
	if (!m_haveCharacteristics) {
		retrieveCharacteristics();
	}
	std::string v = uuid.toString();
	for (auto &myPair : m_characteristicMap) {
		if (myPair.first == v) {
			return myPair.second;
		}
	}
	// throw new BLEUuidNotFoundException();  // <-- we dont want exception here, which will cause app crash, we want to search if any characteristic can be found one after another
	return nullptr;
} // getCharacteristic


/**
 * @brief Retrieve all the characteristics for this service.
 * This function will not return until we have all the characteristics.
 * @return N/A
 */
void BLERemoteService::retrieveCharacteristics() {
	log_v(">> getCharacteristics() for service: %s", getUUID().toString().c_str());

	removeCharacteristics(); // Forget any previous characteristics.

	uint16_t offset = 0;
	esp_gattc_char_elem_t result;
	while (true) {
		uint16_t count = 1; // only room for 1 result allocated, so go one by one
		esp_gatt_status_t status = ::esp_ble_gattc_get_all_char(
			getClient()->getGattcIf(),
			getClient()->getConnId(),
			m_startHandle,
			m_endHandle,
			&result,
			&count,
			offset
		);

		if (status == ESP_GATT_INVALID_OFFSET) {   // We have reached the end of the entries.
			break;
		}

		if (status != ESP_GATT_OK) {   // If we got an error, end.
			log_e("esp_ble_gattc_get_all_char: %s", BLEUtils::gattStatusToString(status).c_str());
			break;
		}

		if (count == 0) {   // If we failed to get any new records, end.
			break;
		}

		log_d("Found a characteristic: Handle: %d, UUID: %s", result.char_handle, BLEUUID(result.uuid).toString().c_str());

		// We now have a new characteristic ... let us add that to our set of known characteristics
		BLERemoteCharacteristic *pNewRemoteCharacteristic = new BLERemoteCharacteristic(
			result.char_handle,
			BLEUUID(result.uuid),
			result.properties,
			this
		);

		m_characteristicMap.insert(std::pair<std::string, BLERemoteCharacteristic*>(pNewRemoteCharacteristic->getUUID().toString(), pNewRemoteCharacteristic));
		m_characteristicMapByHandle.insert(std::pair<uint16_t, BLERemoteCharacteristic*>(result.char_handle, pNewRemoteCharacteristic));
		offset++;   // Increment our count of number of descriptors found.
	} // Loop forever (until we break inside the loop).

	m_haveCharacteristics = true; // Remember that we have received the characteristics.
	log_v("<< getCharacteristics()");
} // getCharacteristics


/**
 * @brief Retrieve a map of all the characteristics of this service.
 * @return A map of all the characteristics of this service.
 */
std::map<std::string, BLERemoteCharacteristic*>* BLERemoteService::getCharacteristics() {
	log_v(">> getCharacteristics() for service: %s", getUUID().toString().c_str());
	// If is possible that we have not read the characteristics associated with the service so do that
	// now.  The request to retrieve the characteristics by calling "retrieveCharacteristics" is a blocking
	// call and does not return until all the characteristics are available.
	if (!m_haveCharacteristics) {
		retrieveCharacteristics();
	}
	log_v("<< getCharacteristics() for service: %s", getUUID().toString().c_str());
	return &m_characteristicMap;
} // getCharacteristics

/**
 * @brief Retrieve a map of all the characteristics of this service.
 * @return A map of all the characteristics of this service.
 */
std::map<uint16_t, BLERemoteCharacteristic*>* BLERemoteService::getCharacteristicsByHandle() {
	// If is possible that we have not read the characteristics associated with the service so do that
	// now.  The request to retrieve the characteristics by calling "retrieveCharacteristics" is a blocking
	// call and does not return until all the characteristics are available.
	if (!m_haveCharacteristics) {
		retrieveCharacteristics();
	}
	return &m_characteristicMapByHandle;
} // getCharacteristicsByHandle

/**
 * @brief This function is designed to get characteristics map when we have multiple characteristics with the same UUID
 */
void BLERemoteService::getCharacteristics(std::map<uint16_t, BLERemoteCharacteristic*>** pCharacteristicMap) {
	log_v(">> getCharacteristics() for service: %s", getUUID().toString().c_str());
	(void)pCharacteristicMap;
	// If is possible that we have not read the characteristics associated with the service so do that
	// now.  The request to retrieve the characteristics by calling "retrieveCharacteristics" is a blocking
	// call and does not return until all the characteristics are available.
	if (!m_haveCharacteristics) {
		retrieveCharacteristics();
	}
	log_v("<< getCharacteristics() for service: %s", getUUID().toString().c_str());
	*pCharacteristicMap = &m_characteristicMapByHandle;
}  // Get the characteristics map.

/**
 * @brief Get the client associated with this service.
 * @return A reference to the client associated with this service.
 */
BLEClient* BLERemoteService::getClient() {
	return m_pClient;
} // getClient


uint16_t BLERemoteService::getEndHandle() {
	return m_endHandle;
} // getEndHandle


esp_gatt_id_t* BLERemoteService::getSrvcId() {
	return &m_srvcId;
} // getSrvcId


uint16_t BLERemoteService::getStartHandle() {
	return m_startHandle;
} // getStartHandle


uint16_t BLERemoteService::getHandle() {
	log_v(">> getHandle: service: %s", getUUID().toString().c_str());
	log_v("<< getHandle: %d 0x%.2x", getStartHandle(), getStartHandle());
	return getStartHandle();
} // getHandle


BLEUUID BLERemoteService::getUUID() {
	return m_uuid;
}

/**
 * @brief Read the value of a characteristic associated with this service.
 */
std::string BLERemoteService::getValue(BLEUUID characteristicUuid) {
	log_v(">> readValue: uuid: %s", characteristicUuid.toString().c_str());
	std::string ret =  getCharacteristic(characteristicUuid)->readValue();
	log_v("<< readValue");
	return ret;
} // readValue



/**
 * @brief Delete the characteristics in the characteristics map.
 * We maintain a map called m_characteristicsMap that contains pointers to BLERemoteCharacteristic
 * object references.  Since we allocated these in this class, we are also responsible for deleteing
 * them.  This method does just that.
 * @return N/A.
 */
void BLERemoteService::removeCharacteristics() {
	m_characteristicMap.clear();   // Clear the map
	for (auto &myPair : m_characteristicMapByHandle) {
	   delete myPair.second;
	   // delete the characteristics only once
	}
	m_characteristicMapByHandle.clear();   // Clear the map
} // removeCharacteristics


/**
 * @brief Set the value of a characteristic.
 * @param [in] characteristicUuid The characteristic to set.
 * @param [in] value The value to set.
 * @throws BLEUuidNotFound
 */
void BLERemoteService::setValue(BLEUUID characteristicUuid, std::string value) {
	log_v(">> setValue: uuid: %s", characteristicUuid.toString().c_str());
	getCharacteristic(characteristicUuid)->writeValue(value);
	log_v("<< setValue");
} // setValue


/**
 * @brief Create a string representation of this remote service.
 * @return A string representation of this remote service.
 */
std::string BLERemoteService::toString() {
	std::string res = "Service: uuid: " + m_uuid.toString();
	char val[6];
	res += ", start_handle: ";
	snprintf(val, sizeof(val), "%d", m_startHandle);
	res += val;
	snprintf(val, sizeof(val), "%04x", m_startHandle);
	res += " 0x";
	res += val;
	res += ", end_handle: ";
	snprintf(val, sizeof(val), "%d", m_endHandle);
	res += val;
	snprintf(val, sizeof(val), "%04x", m_endHandle);
	res += " 0x";
	res += val;
	for (auto &myPair : m_characteristicMap) {
		res += "\n" + myPair.second->toString();
	   // myPair.second is the value
	}
	return res;
} // toString


#endif /* CONFIG_BLUEDROID_ENABLED */
