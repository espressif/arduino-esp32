/*
 * BLECharacteristic.cpp
 *
 *  Created on: Jun 22, 2017
 *      Author: kolban
 */
#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)
#include <sstream>
#include <string.h>
#include <iomanip>
#include <stdlib.h>
#include "sdkconfig.h"
#include <esp_err.h>
#include "BLECharacteristic.h"
#include "BLEService.h"
#include "BLEDevice.h"
#include "BLEUtils.h"
#include "BLE2902.h"
#include "GeneralUtils.h"
#include "esp32-hal-log.h"

#define NULL_HANDLE (0xffff)

static BLECharacteristicCallbacks defaultCallback; //null-object-pattern

/**
 * @brief Construct a characteristic
 * @param [in] uuid - UUID (const char*) for the characteristic.
 * @param [in] properties - Properties for the characteristic.
 */
BLECharacteristic::BLECharacteristic(const char* uuid, uint32_t properties) : BLECharacteristic(BLEUUID(uuid), properties) {
}

/**
 * @brief Construct a characteristic
 * @param [in] uuid - UUID for the characteristic.
 * @param [in] properties - Properties for the characteristic.
 */
BLECharacteristic::BLECharacteristic(BLEUUID uuid, uint32_t properties) {
	m_bleUUID    = uuid;
	m_handle     = NULL_HANDLE;
	m_properties = (esp_gatt_char_prop_t)0;
	m_pCallbacks = &defaultCallback;

	setBroadcastProperty((properties & PROPERTY_BROADCAST) != 0);
	setReadProperty((properties & PROPERTY_READ) != 0);
	setWriteProperty((properties & PROPERTY_WRITE) != 0);
	setNotifyProperty((properties & PROPERTY_NOTIFY) != 0);
	setIndicateProperty((properties & PROPERTY_INDICATE) != 0);
	setWriteNoResponseProperty((properties & PROPERTY_WRITE_NR) != 0);
} // BLECharacteristic

/**
 * @brief Destructor.
 */
BLECharacteristic::~BLECharacteristic() {
	//free(m_value.attr_value); // Release the storage for the value.
} // ~BLECharacteristic


/**
 * @brief Associate a descriptor with this characteristic.
 * @param [in] pDescriptor
 * @return N/A.
 */
void BLECharacteristic::addDescriptor(BLEDescriptor* pDescriptor) {
	log_v(">> addDescriptor(): Adding %s to %s", pDescriptor->toString().c_str(), toString().c_str());
	m_descriptorMap.setByUUID(pDescriptor->getUUID(), pDescriptor);
	log_v("<< addDescriptor()");
} // addDescriptor


/**
 * @brief Register a new characteristic with the ESP runtime.
 * @param [in] pService The service with which to associate this characteristic.
 */
void BLECharacteristic::executeCreate(BLEService* pService) {
	log_v(">> executeCreate()");

	if (m_handle != NULL_HANDLE) {
		log_e("Characteristic already has a handle.");
		return;
	}

	m_pService = pService; // Save the service to which this characteristic belongs.

	log_d("Registering characteristic (esp_ble_gatts_add_char): uuid: %s, service: %s",
		getUUID().toString().c_str(),
		m_pService->toString().c_str());

	esp_attr_control_t control;
	control.auto_rsp = ESP_GATT_RSP_BY_APP;

	m_semaphoreCreateEvt.take("executeCreate");
	esp_err_t errRc = ::esp_ble_gatts_add_char(
		m_pService->getHandle(),
		getUUID().getNative(),
		static_cast<esp_gatt_perm_t>(m_permissions),
		getProperties(),
		nullptr,
		&control); // Whether to auto respond or not.

	if (errRc != ESP_OK) {
		log_e("<< esp_ble_gatts_add_char: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
		return;
	}
	m_semaphoreCreateEvt.wait("executeCreate");

	BLEDescriptor* pDescriptor = m_descriptorMap.getFirst();
	while (pDescriptor != nullptr) {
		pDescriptor->executeCreate(this);
		pDescriptor = m_descriptorMap.getNext();
	} // End while

	log_v("<< executeCreate");
} // executeCreate


/**
 * @brief Return the BLE Descriptor for the given UUID if associated with this characteristic.
 * @param [in] descriptorUUID The UUID of the descriptor that we wish to retrieve.
 * @return The BLE Descriptor.  If no such descriptor is associated with the characteristic, nullptr is returned.
 */
BLEDescriptor* BLECharacteristic::getDescriptorByUUID(const char* descriptorUUID) {
	return m_descriptorMap.getByUUID(BLEUUID(descriptorUUID));
} // getDescriptorByUUID


/**
 * @brief Return the BLE Descriptor for the given UUID if associated with this characteristic.
 * @param [in] descriptorUUID The UUID of the descriptor that we wish to retrieve.
 * @return The BLE Descriptor.  If no such descriptor is associated with the characteristic, nullptr is returned.
 */
BLEDescriptor* BLECharacteristic::getDescriptorByUUID(BLEUUID descriptorUUID) {
	return m_descriptorMap.getByUUID(descriptorUUID);
} // getDescriptorByUUID


/**
 * @brief Get the handle of the characteristic.
 * @return The handle of the characteristic.
 */
uint16_t BLECharacteristic::getHandle() {
	return m_handle;
} // getHandle

void BLECharacteristic::setAccessPermissions(esp_gatt_perm_t perm) {
	m_permissions = perm;
}

esp_gatt_char_prop_t BLECharacteristic::getProperties() {
	return m_properties;
} // getProperties


/**
 * @brief Get the service associated with this characteristic.
 */
BLEService* BLECharacteristic::getService() {
	return m_pService;
} // getService


/**
 * @brief Get the UUID of the characteristic.
 * @return The UUID of the characteristic.
 */
BLEUUID BLECharacteristic::getUUID() {
	return m_bleUUID;
} // getUUID


/**
 * @brief Retrieve the current value of the characteristic.
 * @return A pointer to storage containing the current characteristic value.
 */
std::string BLECharacteristic::getValue() {
	return m_value.getValue();
} // getValue

/**
 * @brief Retrieve the current raw data of the characteristic.
 * @return A pointer to storage containing the current characteristic data.
 */
uint8_t* BLECharacteristic::getData() {
	return m_value.getData();
} // getData


/**
 * Handle a GATT server event.
 */
void BLECharacteristic::handleGATTServerEvent(
		esp_gatts_cb_event_t      event,
		esp_gatt_if_t             gatts_if,
		esp_ble_gatts_cb_param_t* param) {
	log_v(">> handleGATTServerEvent: %s", BLEUtils::gattServerEventTypeToString(event).c_str());

	switch(event) {
	// Events handled:
	//
	// ESP_GATTS_ADD_CHAR_EVT
	// ESP_GATTS_CONF_EVT
	// ESP_GATTS_CONNECT_EVT
	// ESP_GATTS_DISCONNECT_EVT
	// ESP_GATTS_EXEC_WRITE_EVT
	// ESP_GATTS_READ_EVT
	// ESP_GATTS_WRITE_EVT

		//
		// ESP_GATTS_EXEC_WRITE_EVT
		// When we receive this event it is an indication that a previous write long needs to be committed.
		//
		// exec_write:
		// - uint16_t conn_id
		// - uint32_t trans_id
		// - esp_bd_addr_t bda
		// - uint8_t exec_write_flag - Either ESP_GATT_PREP_WRITE_EXEC or ESP_GATT_PREP_WRITE_CANCEL
		//
		case ESP_GATTS_EXEC_WRITE_EVT: {
			if(m_writeEvt){
				m_writeEvt = false;
				if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC) {
					m_value.commit();
					// Invoke the onWrite callback handler.
					m_pCallbacks->onWrite(this, param);
				} else {
					m_value.cancel();
				}
	// ???
				esp_err_t errRc = ::esp_ble_gatts_send_response(
						gatts_if,
						param->write.conn_id,
						param->write.trans_id, ESP_GATT_OK, nullptr);
				if (errRc != ESP_OK) {
					log_e("esp_ble_gatts_send_response: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
				}
			}
			break;
		} // ESP_GATTS_EXEC_WRITE_EVT


		// ESP_GATTS_ADD_CHAR_EVT - Indicate that a characteristic was added to the service.
		// add_char:
		// - esp_gatt_status_t status
		// - uint16_t attr_handle
		// - uint16_t service_handle
		// - esp_bt_uuid_t char_uuid
		case ESP_GATTS_ADD_CHAR_EVT: {
			if (getHandle() == param->add_char.attr_handle) {
				// we have created characteristic, now we can create descriptors
				// BLEDescriptor* pDescriptor = m_descriptorMap.getFirst();
				// while (pDescriptor != nullptr) {
				// 	pDescriptor->executeCreate(this);
				// 	pDescriptor = m_descriptorMap.getNext();
				// } // End while
				m_semaphoreCreateEvt.give();
			}
			break;
		} // ESP_GATTS_ADD_CHAR_EVT


		// ESP_GATTS_WRITE_EVT - A request to write the value of a characteristic has arrived.
		//
		// write:
		// - uint16_t      conn_id
		// - uint16_t      trans_id
		// - esp_bd_addr_t bda
		// - uint16_t      handle
		// - uint16_t      offset
		// - bool          need_rsp
		// - bool          is_prep
		// - uint16_t      len
		// - uint8_t      *value
		//
		case ESP_GATTS_WRITE_EVT: {
// We check if this write request is for us by comparing the handles in the event.  If it is for us
// we save the new value.  Next we look at the need_rsp flag which indicates whether or not we need
// to send a response.  If we do, then we formulate a response and send it.
			if (param->write.handle == m_handle) {
				if (param->write.is_prep) {
					m_value.addPart(param->write.value, param->write.len);
					m_writeEvt = true;
				} else {
					setValue(param->write.value, param->write.len);
				}

				log_d(" - Response to write event: New value: handle: %.2x, uuid: %s",
						getHandle(), getUUID().toString().c_str());

				char* pHexData = BLEUtils::buildHexData(nullptr, param->write.value, param->write.len);
				log_d(" - Data: length: %d, data: %s", param->write.len, pHexData);
				free(pHexData);

				if (param->write.need_rsp) {
					esp_gatt_rsp_t rsp;

					rsp.attr_value.len      = param->write.len;
					rsp.attr_value.handle   = m_handle;
					rsp.attr_value.offset   = param->write.offset;
					rsp.attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
					memcpy(rsp.attr_value.value, param->write.value, param->write.len);

					esp_err_t errRc = ::esp_ble_gatts_send_response(
							gatts_if,
							param->write.conn_id,
							param->write.trans_id, ESP_GATT_OK, &rsp);
					if (errRc != ESP_OK) {
						log_e("esp_ble_gatts_send_response: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
					}
				} // Response needed

				if (param->write.is_prep != true) {
					// Invoke the onWrite callback handler.
					m_pCallbacks->onWrite(this, param);
				}
			} // Match on handles.
			break;
		} // ESP_GATTS_WRITE_EVT


		// ESP_GATTS_READ_EVT - A request to read the value of a characteristic has arrived.
		//
		// read:
		// - uint16_t      conn_id
		// - uint32_t      trans_id
		// - esp_bd_addr_t bda
		// - uint16_t      handle
		// - uint16_t      offset
		// - bool          is_long
		// - bool          need_rsp
		//
		case ESP_GATTS_READ_EVT: {
			if (param->read.handle == m_handle) {



// Here's an interesting thing.  The read request has the option of saying whether we need a response
// or not.  What would it "mean" to receive a read request and NOT send a response back?  That feels like
// a very strange read.
//
// We have to handle the case where the data we wish to send back to the client is greater than the maximum
// packet size of 22 bytes.  In this case, we become responsible for chunking the data into units of 22 bytes.
// The apparent algorithm is as follows:
//
// If the is_long flag is set then this is a follow on from an original read and we will already have sent at least 22 bytes.
// If the is_long flag is not set then we need to check how much data we are going to send.  If we are sending LESS than
// 22 bytes, then we "just" send it and thats the end of the story.
// If we are sending 22 bytes exactly, we just send it BUT we will get a follow on request.
// If we are sending more than 22 bytes, we send the first 22 bytes and we will get a follow on request.
// Because of follow on request processing, we need to maintain an offset of how much data we have already sent
// so that when a follow on request arrives, we know where to start in the data to send the next sequence.
// Note that the indication that the client will send a follow on request is that we sent exactly 22 bytes as a response.
// If our payload is divisible by 22 then the last response will be a response of 0 bytes in length.
//
// The following code has deliberately not been factored to make it fewer statements because this would cloud the
// the logic flow comprehension.
//

				// get mtu for peer device that we are sending read request to
				uint16_t maxOffset =  getService()->getServer()->getPeerMTU(param->read.conn_id) - 1;
				log_d("mtu value: %d", maxOffset);
				if (param->read.need_rsp) {
					log_d("Sending a response (esp_ble_gatts_send_response)");
					esp_gatt_rsp_t rsp;

					if (param->read.is_long) {
						std::string value = m_value.getValue();

						if (value.length() - m_value.getReadOffset() < maxOffset) {
							// This is the last in the chain
							rsp.attr_value.len    = value.length() - m_value.getReadOffset();
							rsp.attr_value.offset = m_value.getReadOffset();
							memcpy(rsp.attr_value.value, value.data() + rsp.attr_value.offset, rsp.attr_value.len);
							m_value.setReadOffset(0);
						} else {
							// There will be more to come.
							rsp.attr_value.len    = maxOffset;
							rsp.attr_value.offset = m_value.getReadOffset();
							memcpy(rsp.attr_value.value, value.data() + rsp.attr_value.offset, rsp.attr_value.len);
							m_value.setReadOffset(rsp.attr_value.offset + maxOffset);
						}
					} else { // read.is_long == false

						// If is.long is false then this is the first (or only) request to read data, so invoke the callback
						// Invoke the read callback.
						m_pCallbacks->onRead(this, param);

						std::string value = m_value.getValue();

						if (value.length() + 1 > maxOffset) {
							// Too big for a single shot entry.
							m_value.setReadOffset(maxOffset);
							rsp.attr_value.len    = maxOffset;
							rsp.attr_value.offset = 0;
							memcpy(rsp.attr_value.value, value.data(), rsp.attr_value.len);
						} else {
							// Will fit in a single packet with no callbacks required.
							rsp.attr_value.len    = value.length();
							rsp.attr_value.offset = 0;
							memcpy(rsp.attr_value.value, value.data(), rsp.attr_value.len);
						}
					}
					rsp.attr_value.handle   = param->read.handle;
					rsp.attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;

					char *pHexData = BLEUtils::buildHexData(nullptr, rsp.attr_value.value, rsp.attr_value.len);
					log_d(" - Data: length=%d, data=%s, offset=%d", rsp.attr_value.len, pHexData, rsp.attr_value.offset);
					free(pHexData);

					esp_err_t errRc = ::esp_ble_gatts_send_response(
							gatts_if, param->read.conn_id,
							param->read.trans_id,
							ESP_GATT_OK,
							&rsp);
					if (errRc != ESP_OK) {
						log_e("esp_ble_gatts_send_response: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
					}
				} // Response needed
			} // Handle matches this characteristic.
			break;
		} // ESP_GATTS_READ_EVT


		// ESP_GATTS_CONF_EVT
		//
		// conf:
		// - esp_gatt_status_t status  – The status code.
		// - uint16_t          conn_id – The connection used.
		//
		case ESP_GATTS_CONF_EVT: {
			// log_d("m_handle = %d, conf->handle = %d", m_handle, param->conf.handle);
			if(param->conf.conn_id == getService()->getServer()->getConnId()) // && param->conf.handle == m_handle) // bug in esp-idf and not implemented in arduino yet
				m_semaphoreConfEvt.give(param->conf.status);
			break;
		}

		case ESP_GATTS_CONNECT_EVT: {
			break;
		}

		case ESP_GATTS_DISCONNECT_EVT: {
			m_semaphoreConfEvt.give();
			break;
		}

		default: {
			break;
		} // default

	} // switch event

	// Give each of the descriptors associated with this characteristic the opportunity to handle the
	// event.

	m_descriptorMap.handleGATTServerEvent(event, gatts_if, param);
	log_v("<< handleGATTServerEvent");
} // handleGATTServerEvent


/**
 * @brief Send an indication.
 * An indication is a transmission of up to the first 20 bytes of the characteristic value.  An indication
 * will block waiting a positive confirmation from the client.
 * @return N/A
 */
void BLECharacteristic::indicate() {

	log_v(">> indicate: length: %d", m_value.getValue().length());
	notify(false);
	log_v("<< indicate");
} // indicate


/**
 * @brief Send a notify.
 * A notification is a transmission of up to the first 20 bytes of the characteristic value.  An notification
 * will not block; it is a fire and forget.
 * @return N/A.
 */
void BLECharacteristic::notify(bool is_notification) {
	log_v(">> notify: length: %d", m_value.getValue().length());

	assert(getService() != nullptr);
	assert(getService()->getServer() != nullptr);

	m_pCallbacks->onNotify(this);   // Invoke the notify callback.

	GeneralUtils::hexDump((uint8_t*)m_value.getValue().data(), m_value.getValue().length());

	if (getService()->getServer()->getConnectedCount() == 0) {
		log_v("<< notify: No connected clients.");
		m_pCallbacks->onStatus(this, BLECharacteristicCallbacks::Status::ERROR_NO_CLIENT, 0);
		return;
	}

	// Test to see if we have a 0x2902 descriptor.  If we do, then check to see if notification is enabled
	// and, if not, prevent the notification.

	BLE2902 *p2902 = (BLE2902*)getDescriptorByUUID((uint16_t)0x2902);
	if(is_notification) {
		if (p2902 != nullptr && !p2902->getNotifications()) {
			log_v("<< notifications disabled; ignoring");
			m_pCallbacks->onStatus(this, BLECharacteristicCallbacks::Status::ERROR_NOTIFY_DISABLED, 0);   // Invoke the notify callback.
			return;
		}
	}
	else{
		if (p2902 != nullptr && !p2902->getIndications()) {
			log_v("<< indications disabled; ignoring");
			m_pCallbacks->onStatus(this, BLECharacteristicCallbacks::Status::ERROR_INDICATE_DISABLED, 0);   // Invoke the notify callback.
			return;
		}
	}
	for (auto &myPair : getService()->getServer()->getPeerDevices(false)) {
		uint16_t _mtu = (myPair.second.mtu);
		if (m_value.getValue().length() > _mtu - 3) {
			log_w("- Truncating to %d bytes (maximum notify size)", _mtu - 3);
		}

		size_t length = m_value.getValue().length();
		if(!is_notification) // is indication
			m_semaphoreConfEvt.take("indicate");
		esp_err_t errRc = ::esp_ble_gatts_send_indicate(
				getService()->getServer()->getGattsIf(),
				myPair.first,
				getHandle(), length, (uint8_t*)m_value.getValue().data(), !is_notification); // The need_confirm = false makes this a notify.
		if (errRc != ESP_OK) {
			log_e("<< esp_ble_gatts_send_ %s: rc=%d %s",is_notification?"notify":"indicate", errRc, GeneralUtils::errorToString(errRc));
			m_semaphoreConfEvt.give();
			m_pCallbacks->onStatus(this, BLECharacteristicCallbacks::Status::ERROR_GATT, errRc);   // Invoke the notify callback.
			return;
		}
		if(!is_notification){ // is indication
			if(!m_semaphoreConfEvt.timedWait("indicate", indicationTimeout)){
				m_pCallbacks->onStatus(this, BLECharacteristicCallbacks::Status::ERROR_INDICATE_TIMEOUT, 0);   // Invoke the notify callback.
			} else {
				auto code = (esp_gatt_status_t) m_semaphoreConfEvt.value();
				if(code == ESP_GATT_OK) {
					m_pCallbacks->onStatus(this, BLECharacteristicCallbacks::Status::SUCCESS_INDICATE, code);   // Invoke the notify callback.
				} else {
					m_pCallbacks->onStatus(this, BLECharacteristicCallbacks::Status::ERROR_INDICATE_FAILURE, code);
				}
			}
		} else {
			m_pCallbacks->onStatus(this, BLECharacteristicCallbacks::Status::SUCCESS_NOTIFY, 0);   // Invoke the notify callback.
		}
	}
	log_v("<< notify");
} // Notify


/**
 * @brief Set the permission to broadcast.
 * A characteristics has properties associated with it which define what it is capable of doing.
 * One of these is the broadcast flag.
 * @param [in] value The flag value of the property.
 * @return N/A
 */
void BLECharacteristic::setBroadcastProperty(bool value) {
	//log_d("setBroadcastProperty(%d)", value);
	if (value) {
		m_properties = (esp_gatt_char_prop_t)(m_properties | ESP_GATT_CHAR_PROP_BIT_BROADCAST);
	} else {
		m_properties = (esp_gatt_char_prop_t)(m_properties & ~ESP_GATT_CHAR_PROP_BIT_BROADCAST);
	}
} // setBroadcastProperty


/**
 * @brief Set the callback handlers for this characteristic.
 * @param [in] pCallbacks An instance of a callbacks structure used to define any callbacks for the characteristic.
 */
void BLECharacteristic::setCallbacks(BLECharacteristicCallbacks* pCallbacks) {
	log_v(">> setCallbacks: 0x%x", (uint32_t)pCallbacks);
	if (pCallbacks != nullptr){
		m_pCallbacks = pCallbacks;
	} else {
		m_pCallbacks = &defaultCallback;
	}
	log_v("<< setCallbacks");
} // setCallbacks


/**
 * @brief Set the BLE handle associated with this characteristic.
 * A user program will request that a characteristic be created against a service.  When the characteristic has been
 * registered, the service will be given a "handle" that it knows the characteristic as.  This handle is unique to the
 * server/service but it is told to the service, not the characteristic associated with the service.  This internally
 * exposed function can be invoked by the service against this model of the characteristic to allow the characteristic
 * to learn its own handle.  Once the characteristic knows its own handle, it will be able to see incoming GATT events
 * that will be propagated down to it which contain a handle value and now know that the event is destined for it.
 * @param [in] handle The handle associated with this characteristic.
 */
void BLECharacteristic::setHandle(uint16_t handle) {
	log_v(">> setHandle: handle=0x%.2x, characteristic uuid=%s", handle, getUUID().toString().c_str());
	m_handle = handle;
	log_v("<< setHandle");
} // setHandle


/**
 * @brief Set the Indicate property value.
 * @param [in] value Set to true if we are to allow indicate messages.
 */
void BLECharacteristic::setIndicateProperty(bool value) {
	//log_d("setIndicateProperty(%d)", value);
	if (value) {
		m_properties = (esp_gatt_char_prop_t)(m_properties | ESP_GATT_CHAR_PROP_BIT_INDICATE);
	} else {
		m_properties = (esp_gatt_char_prop_t)(m_properties & ~ESP_GATT_CHAR_PROP_BIT_INDICATE);
	}
} // setIndicateProperty


/**
 * @brief Set the Notify property value.
 * @param [in] value Set to true if we are to allow notification messages.
 */
void BLECharacteristic::setNotifyProperty(bool value) {
	//log_d("setNotifyProperty(%d)", value);
	if (value) {
		m_properties = (esp_gatt_char_prop_t)(m_properties | ESP_GATT_CHAR_PROP_BIT_NOTIFY);
	} else {
		m_properties = (esp_gatt_char_prop_t)(m_properties & ~ESP_GATT_CHAR_PROP_BIT_NOTIFY);
	}
} // setNotifyProperty


/**
 * @brief Set the Read property value.
 * @param [in] value Set to true if we are to allow reads.
 */
void BLECharacteristic::setReadProperty(bool value) {
	//log_d("setReadProperty(%d)", value);
	if (value) {
		m_properties = (esp_gatt_char_prop_t)(m_properties | ESP_GATT_CHAR_PROP_BIT_READ);
	} else {
		m_properties = (esp_gatt_char_prop_t)(m_properties & ~ESP_GATT_CHAR_PROP_BIT_READ);
	}
} // setReadProperty


/**
 * @brief Set the value of the characteristic.
 * @param [in] data The data to set for the characteristic.
 * @param [in] length The length of the data in bytes.
 */
void BLECharacteristic::setValue(uint8_t* data, size_t length) {
	char* pHex = BLEUtils::buildHexData(nullptr, data, length);
	log_v(">> setValue: length=%d, data=%s, characteristic UUID=%s", length, pHex, getUUID().toString().c_str());
	free(pHex);
	if (length > ESP_GATT_MAX_ATTR_LEN) {
		log_e("Size %d too large, must be no bigger than %d", length, ESP_GATT_MAX_ATTR_LEN);
		return;
	}
	m_semaphoreSetValue.take();  
	m_value.setValue(data, length);
	m_semaphoreSetValue.give();  
	log_v("<< setValue");
} // setValue


/**
 * @brief Set the value of the characteristic from string data.
 * We set the value of the characteristic from the bytes contained in the
 * string.
 * @param [in] Set the value of the characteristic.
 * @return N/A.
 */
void BLECharacteristic::setValue(std::string value) {
	setValue((uint8_t*)(value.data()), value.length());
} // setValue

void BLECharacteristic::setValue(uint16_t& data16) {
	uint8_t temp[2];
	temp[0] = data16;
	temp[1] = data16 >> 8;
	setValue(temp, 2);
} // setValue

void BLECharacteristic::setValue(uint32_t& data32) {
	uint8_t temp[4];
	temp[0] = data32;
	temp[1] = data32 >> 8;
	temp[2] = data32 >> 16;
	temp[3] = data32 >> 24;
	setValue(temp, 4);
} // setValue

void BLECharacteristic::setValue(int& data32) {
	uint8_t temp[4];
	temp[0] = data32;
	temp[1] = data32 >> 8;
	temp[2] = data32 >> 16;
	temp[3] = data32 >> 24;
	setValue(temp, 4);
} // setValue

void BLECharacteristic::setValue(float& data32) {
	float temp = data32;
	setValue((uint8_t*)&temp, 4);
} // setValue

void BLECharacteristic::setValue(double& data64) {
	double temp = data64;
	setValue((uint8_t*)&temp, 8);
} // setValue


/**
 * @brief Set the Write No Response property value.
 * @param [in] value Set to true if we are to allow writes with no response.
 */
void BLECharacteristic::setWriteNoResponseProperty(bool value) {
	//log_d("setWriteNoResponseProperty(%d)", value);
	if (value) {
		m_properties = (esp_gatt_char_prop_t)(m_properties | ESP_GATT_CHAR_PROP_BIT_WRITE_NR);
	} else {
		m_properties = (esp_gatt_char_prop_t)(m_properties & ~ESP_GATT_CHAR_PROP_BIT_WRITE_NR);
	}
} // setWriteNoResponseProperty


/**
 * @brief Set the Write property value.
 * @param [in] value Set to true if we are to allow writes.
 */
void BLECharacteristic::setWriteProperty(bool value) {
	//log_d("setWriteProperty(%d)", value);
	if (value) {
		m_properties = (esp_gatt_char_prop_t)(m_properties | ESP_GATT_CHAR_PROP_BIT_WRITE);
	} else {
		m_properties = (esp_gatt_char_prop_t)(m_properties & ~ESP_GATT_CHAR_PROP_BIT_WRITE);
	}
} // setWriteProperty


/**
 * @brief Return a string representation of the characteristic.
 * @return A string representation of the characteristic.
 */
std::string BLECharacteristic::toString() {
	std::string res = "UUID: " + m_bleUUID.toString() + ", handle : 0x";
	char hex[5];
	snprintf(hex, sizeof(hex), "%04x", m_handle);
	res += hex;
	res += " ";
	if (m_properties & ESP_GATT_CHAR_PROP_BIT_READ) res += "Read ";
	if (m_properties & ESP_GATT_CHAR_PROP_BIT_WRITE) res += "Write ";
	if (m_properties & ESP_GATT_CHAR_PROP_BIT_WRITE_NR) res += "WriteNoResponse ";
	if (m_properties & ESP_GATT_CHAR_PROP_BIT_BROADCAST) res += "Broadcast ";
	if (m_properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY) res += "Notify ";
	if (m_properties & ESP_GATT_CHAR_PROP_BIT_INDICATE) res += "Indicate ";
	return res;
} // toString


BLECharacteristicCallbacks::~BLECharacteristicCallbacks() {}

void BLECharacteristicCallbacks::onRead(BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param) {
	onRead(pCharacteristic);
} // onRead

void BLECharacteristicCallbacks::onRead(BLECharacteristic* pCharacteristic) {
	log_d(">> onRead: default");
	log_d("<< onRead");
} // onRead


void BLECharacteristicCallbacks::onWrite(BLECharacteristic* pCharacteristic, esp_ble_gatts_cb_param_t* param) {
	onWrite(pCharacteristic);
} // onWrite

void BLECharacteristicCallbacks::onWrite(BLECharacteristic* pCharacteristic) {
	log_d(">> onWrite: default");
	log_d("<< onWrite");
} // onWrite


void BLECharacteristicCallbacks::onNotify(BLECharacteristic* pCharacteristic) {
	log_d(">> onNotify: default");
	log_d("<< onNotify");
} // onNotify


void BLECharacteristicCallbacks::onStatus(BLECharacteristic* pCharacteristic, Status s, uint32_t code) {
	log_d(">> onStatus: default");
	log_d("<< onStatus");
} // onStatus


#endif /* CONFIG_BLUEDROID_ENABLED */
