/*
 * BLERemoteDescriptor.cpp
 *
 *  Created on: Jul 8, 2017
 *      Author: kolban
 */
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)
#include <sstream>
#include "BLERemoteDescriptor.h"
#include "GeneralUtils.h"
#include "esp32-hal-log.h"

BLERemoteDescriptor::BLERemoteDescriptor(
	uint16_t                 handle,
	BLEUUID                  uuid,
	BLERemoteCharacteristic* pRemoteCharacteristic) {

	m_handle                = handle;
	m_uuid                  = uuid;
	m_pRemoteCharacteristic = pRemoteCharacteristic;
}


/**
 * @brief Retrieve the handle associated with this remote descriptor.
 * @return The handle associated with this remote descriptor.
 */
uint16_t BLERemoteDescriptor::getHandle() {
	return m_handle;
} // getHandle


/**
 * @brief Get the characteristic that owns this descriptor.
 * @return The characteristic that owns this descriptor.
 */
BLERemoteCharacteristic* BLERemoteDescriptor::getRemoteCharacteristic() {
	return m_pRemoteCharacteristic;
} // getRemoteCharacteristic


/**
 * @brief Retrieve the UUID associated this remote descriptor.
 * @return The UUID associated this remote descriptor.
 */
BLEUUID BLERemoteDescriptor::getUUID() {
	return m_uuid;
} // getUUID


std::string BLERemoteDescriptor::readValue() {
	log_v(">> readValue: %s", toString().c_str());

	// Check to see that we are connected.
	if (!getRemoteCharacteristic()->getRemoteService()->getClient()->isConnected()) {
		log_e("Disconnected");
		return std::string();
	}

	m_semaphoreReadDescrEvt.take("readValue");

	// Ask the BLE subsystem to retrieve the value for the remote hosted characteristic.
	esp_err_t errRc = ::esp_ble_gattc_read_char_descr(
		m_pRemoteCharacteristic->getRemoteService()->getClient()->getGattcIf(),
		m_pRemoteCharacteristic->getRemoteService()->getClient()->getConnId(),    // The connection ID to the BLE server
		getHandle(),                                   // The handle of this characteristic
		ESP_GATT_AUTH_REQ_NONE);                       // Security

	if (errRc != ESP_OK) {
		log_e("esp_ble_gattc_read_char: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
		return "";
	}

	// Block waiting for the event that indicates that the read has completed.  When it has, the std::string found
	// in m_value will contain our data.
	m_semaphoreReadDescrEvt.wait("readValue");

	log_v("<< readValue(): length: %d", m_value.length());
	return m_value;
} // readValue


uint8_t BLERemoteDescriptor::readUInt8() {
	std::string value = readValue();
	if (value.length() >= 1) {
		return (uint8_t) value[0];
	}
	return 0;
} // readUInt8


uint16_t BLERemoteDescriptor::readUInt16() {
	std::string value = readValue();
	if (value.length() >= 2) {
		return *(uint16_t*) value.data();
	}
	return 0;
} // readUInt16


uint32_t BLERemoteDescriptor::readUInt32() {
	std::string value = readValue();
	if (value.length() >= 4) {
		return *(uint32_t*) value.data();
	}
	return 0;
} // readUInt32


/**
 * @brief Return a string representation of this BLE Remote Descriptor.
 * @retun A string representation of this BLE Remote Descriptor.
 */
std::string BLERemoteDescriptor::toString() {
	char val[6];
	snprintf(val, sizeof(val), "%d", getHandle());
	std::string res = "handle: ";
	res += val;
	res += ", uuid: " + getUUID().toString();
	return res;
} // toString


/**
 * @brief Write data to the BLE Remote Descriptor.
 * @param [in] data The data to send to the remote descriptor.
 * @param [in] length The length of the data to send.
 * @param [in] response True if we expect a response.
 */
void BLERemoteDescriptor::writeValue(uint8_t* data, size_t length, bool response) {
	log_v(">> writeValue: %s", toString().c_str());
	// Check to see that we are connected.
	if (!getRemoteCharacteristic()->getRemoteService()->getClient()->isConnected()) {
		log_e("Disconnected");
		return;
	}

	esp_err_t errRc = ::esp_ble_gattc_write_char_descr(
		m_pRemoteCharacteristic->getRemoteService()->getClient()->getGattcIf(),
		m_pRemoteCharacteristic->getRemoteService()->getClient()->getConnId(),
		getHandle(),
		length,                           // Data length
		data,                             // Data
		response ? ESP_GATT_WRITE_TYPE_RSP : ESP_GATT_WRITE_TYPE_NO_RSP,
		ESP_GATT_AUTH_REQ_NONE
	);
	if (errRc != ESP_OK) {
		log_e("esp_ble_gattc_write_char_descr: %d", errRc);
	}
	log_v("<< writeValue");
} // writeValue


/**
 * @brief Write data represented as a string to the BLE Remote Descriptor.
 * @param [in] newValue The data to send to the remote descriptor.
 * @param [in] response True if we expect a response.
 */
void BLERemoteDescriptor::writeValue(std::string newValue, bool response) {
	writeValue((uint8_t*) newValue.data(), newValue.length(), response);
} // writeValue


/**
 * @brief Write a byte value to the Descriptor.
 * @param [in] The single byte to write.
 * @param [in] True if we expect a response.
 */
void BLERemoteDescriptor::writeValue(uint8_t newValue, bool response) {
	writeValue(&newValue, 1, response);
} // writeValue


#endif /* CONFIG_BT_ENABLED */
