/*
 * BLEDescriptorMap.cpp
 *
 *  Created on: Jun 22, 2017
 *      Author: kolban
 */
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)
#include <sstream>
#include <iomanip>
#include "BLECharacteristic.h"
#include "BLEDescriptor.h"
#include <esp_gatts_api.h>   // ESP32 BLE
#ifdef ARDUINO_ARCH_ESP32
#include "esp32-hal-log.h"
#endif

/**
 * @brief Return the descriptor by UUID.
 * @param [in] UUID The UUID to look up the descriptor.
 * @return The descriptor.  If not present, then nullptr is returned.
 */
BLEDescriptor* BLEDescriptorMap::getByUUID(const char* uuid) {
	return getByUUID(BLEUUID(uuid));
}


/**
 * @brief Return the descriptor by UUID.
 * @param [in] UUID The UUID to look up the descriptor.
 * @return The descriptor.  If not present, then nullptr is returned.
 */
BLEDescriptor* BLEDescriptorMap::getByUUID(BLEUUID uuid) {
	for (auto &myPair : m_uuidMap) {
		if (myPair.first->getUUID().equals(uuid)) {
			return myPair.first;
		}
	}
	//return m_uuidMap.at(uuid.toString());
	return nullptr;
} // getByUUID


/**
 * @brief Return the descriptor by handle.
 * @param [in] handle The handle to look up the descriptor.
 * @return The descriptor.
 */
BLEDescriptor* BLEDescriptorMap::getByHandle(uint16_t handle) {
	return m_handleMap.at(handle);
} // getByHandle


/**
 * @brief Set the descriptor by UUID.
 * @param [in] uuid The uuid of the descriptor.
 * @param [in] characteristic The descriptor to cache.
 * @return N/A.
 */
void BLEDescriptorMap::setByUUID(const char* uuid, BLEDescriptor* pDescriptor){
	m_uuidMap.insert(std::pair<BLEDescriptor*, std::string>(pDescriptor, uuid));
} // setByUUID



/**
 * @brief Set the descriptor by UUID.
 * @param [in] uuid The uuid of the descriptor.
 * @param [in] characteristic The descriptor to cache.
 * @return N/A.
 */
void BLEDescriptorMap::setByUUID(BLEUUID uuid, BLEDescriptor* pDescriptor) {
	m_uuidMap.insert(std::pair<BLEDescriptor*, std::string>(pDescriptor, uuid.toString()));
} // setByUUID


/**
 * @brief Set the descriptor by handle.
 * @param [in] handle The handle of the descriptor.
 * @param [in] descriptor The descriptor to cache.
 * @return N/A.
 */
void BLEDescriptorMap::setByHandle(uint16_t handle, BLEDescriptor* pDescriptor) {
	m_handleMap.insert(std::pair<uint16_t, BLEDescriptor*>(handle, pDescriptor));
} // setByHandle


/**
 * @brief Return a string representation of the descriptor map.
 * @return A string representation of the descriptor map.
 */
std::string BLEDescriptorMap::toString() {
	std::string res;
	char hex[5];
	int count = 0;
	for (auto &myPair : m_uuidMap) {
		if (count > 0) {res += "\n";}
		snprintf(hex, sizeof(hex), "%04x", myPair.first->getHandle());
		count++;
		res += "handle: 0x";
		res += hex;
		res += ", uuid: " + myPair.first->getUUID().toString();
	}
	return res;
} // toString


/**
 * @breif Pass the GATT server event onwards to each of the descriptors found in the mapping
 * @param [in] event
 * @param [in] gatts_if
 * @param [in] param
 */
void BLEDescriptorMap::handleGATTServerEvent(
		esp_gatts_cb_event_t      event,
		esp_gatt_if_t             gatts_if,
		esp_ble_gatts_cb_param_t* param) {
	// Invoke the handler for every descriptor we have.
	for (auto &myPair : m_uuidMap) {
		myPair.first->handleGATTServerEvent(event, gatts_if, param);
	}
} // handleGATTServerEvent


/**
 * @brief Get the first descriptor in the map.
 * @return The first descriptor in the map.
 */
BLEDescriptor* BLEDescriptorMap::getFirst() {
	m_iterator = m_uuidMap.begin();
	if (m_iterator == m_uuidMap.end()) return nullptr;
	BLEDescriptor* pRet = m_iterator->first;
	m_iterator++;
	return pRet;
} // getFirst


/**
 * @brief Get the next descriptor in the map.
 * @return The next descriptor in the map.
 */
BLEDescriptor* BLEDescriptorMap::getNext() {
	if (m_iterator == m_uuidMap.end()) return nullptr;
	BLEDescriptor* pRet = m_iterator->first;
	m_iterator++;
	return pRet;
} // getNext
#endif /* CONFIG_BT_ENABLED */
