/*
 * BLEServiceMap.cpp
 *
 *  Created on: Jun 22, 2017
 *      Author: kolban
 */
#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)
#include <stdio.h>
#include <iomanip>
#include "BLEService.h"

/**
 * @brief Return the service by UUID.
 * @param [in] UUID The UUID to look up the service.
 * @return The characteristic.
 */
BLEService *BLEServiceMap::getByUUID(const char *uuid) {
  return getByUUID(BLEUUID(uuid));
}

/**
 * @brief Return the service by UUID.
 * @param [in] UUID The UUID to look up the service.
 * @return The characteristic.
 */
BLEService *BLEServiceMap::getByUUID(BLEUUID uuid, uint8_t inst_id) {
  for (auto &myPair : m_uuidMap) {
    if (myPair.first->getUUID().equals(uuid)) {
      return myPair.first;
    }
  }
  //return m_uuidMap.at(uuid.toString());
  return nullptr;
}  // getByUUID

/**
 * @brief Return the service by handle.
 * @param [in] handle The handle to look up the service.
 * @return The service.
 */
BLEService *BLEServiceMap::getByHandle(uint16_t handle) {
  return m_handleMap.at(handle);
}  // getByHandle

/**
 * @brief Set the service by UUID.
 * @param [in] uuid The uuid of the service.
 * @param [in] characteristic The service to cache.
 * @return N/A.
 */
void BLEServiceMap::setByUUID(BLEUUID uuid, BLEService *service) {
  m_uuidMap.insert(std::pair<BLEService *, String>(service, uuid.toString()));
}  // setByUUID

/**
 * @brief Set the service by handle.
 * @param [in] handle The handle of the service.
 * @param [in] service The service to cache.
 * @return N/A.
 */
void BLEServiceMap::setByHandle(uint16_t handle, BLEService *service) {
  m_handleMap.insert(std::pair<uint16_t, BLEService *>(handle, service));
}  // setByHandle

/**
 * @brief Return a string representation of the service map.
 * @return A string representation of the service map.
 */
String BLEServiceMap::toString() {
  String res;
  char hex[5];
  for (auto &myPair : m_handleMap) {
    res += "handle: 0x";
    snprintf(hex, sizeof(hex), "%04x", myPair.first);
    res += hex;
    res += ", uuid: " + myPair.second->getUUID().toString() + "\n";
  }
  return res;
}  // toString

void BLEServiceMap::handleGATTServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
  // Invoke the handler for every Service we have.
  for (auto &myPair : m_uuidMap) {
    myPair.first->handleGATTServerEvent(event, gatts_if, param);
  }
}

/**
 * @brief Get the first service in the map.
 * @return The first service in the map.
 */
BLEService *BLEServiceMap::getFirst() {
  m_iterator = m_uuidMap.begin();
  if (m_iterator == m_uuidMap.end()) {
    return nullptr;
  }
  BLEService *pRet = m_iterator->first;
  m_iterator++;
  return pRet;
}  // getFirst

/**
 * @brief Get the next service in the map.
 * @return The next service in the map.
 */
BLEService *BLEServiceMap::getNext() {
  if (m_iterator == m_uuidMap.end()) {
    return nullptr;
  }
  BLEService *pRet = m_iterator->first;
  m_iterator++;
  return pRet;
}  // getNext

/**
 * @brief Removes service from maps.
 * @return N/A.
 */
void BLEServiceMap::removeService(BLEService *service) {
  m_handleMap.erase(service->getHandle());
  m_uuidMap.erase(service);
}  // removeService

/**
 * @brief Returns the amount of registered services
 * @return amount of registered services
 */
int BLEServiceMap::getRegisteredServiceCount() {
  return m_handleMap.size();
}

#endif /* CONFIG_BLUEDROID_ENABLED */
#endif /* SOC_BLE_SUPPORTED */
