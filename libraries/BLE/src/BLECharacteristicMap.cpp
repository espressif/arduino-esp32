/*
 * BLECharacteristicMap.cpp
 *
 *  Created on: Jun 22, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include <sstream>
#include <iomanip>
#include "BLEService.h"
#include "BLEUtils.h"
#ifdef ARDUINO_ARCH_ESP32
#include "esp32-hal-log.h"
#endif

/***************************************************************************
 *                           Common functions                              *
 ***************************************************************************/

/**
 * @brief Return the characteristic by handle.
 * @param [in] handle The handle to look up the characteristic.
 * @return The characteristic.
 */
BLECharacteristic *BLECharacteristicMap::getByHandle(uint16_t handle) const {
  return m_handleMap.at(handle);
}  // getByHandle

/**
 * @brief Return the characteristic by UUID.
 * @param [in] UUID The UUID to look up the characteristic.
 * @return The characteristic.
 */
BLECharacteristic *BLECharacteristicMap::getByUUID(const char *uuid) const {
  return getByUUID(BLEUUID(uuid));
}

/**
 * @brief Return the characteristic by UUID.
 * @param [in] UUID The UUID to look up the characteristic.
 * @return The characteristic.
 */
BLECharacteristic *BLECharacteristicMap::getByUUID(BLEUUID uuid) const {
  for (auto &myPair : m_uuidMap) {
    if (myPair.first->getUUID().equals(uuid)) {
      return myPair.first;
    }
  }
  //return m_uuidMap.at(uuid.toString());
  return nullptr;
}  // getByUUID

/**
 * @brief Get the first characteristic in the map.
 * @return The first characteristic in the map.
 */
BLECharacteristic *BLECharacteristicMap::getFirst() {
  m_iterator = m_uuidMap.begin();
  if (m_iterator == m_uuidMap.end()) {
    return nullptr;
  }
  BLECharacteristic *pRet = m_iterator->first;
  m_iterator++;
  return pRet;
}  // getFirst

/**
 * @brief Get the next characteristic in the map.
 * @return The next characteristic in the map.
 */
BLECharacteristic *BLECharacteristicMap::getNext() {
  if (m_iterator == m_uuidMap.end()) {
    return nullptr;
  }
  BLECharacteristic *pRet = m_iterator->first;
  m_iterator++;
  return pRet;
}  // getNext

/**
 * @brief Get the number of registered characteristics.
 * @return The number of registered characteristics.
 */
int BLECharacteristicMap::getRegisteredCharacteristicCount() const {
  return m_uuidMap.size();
}  // getRegisteredCharacteristicCount

/**
 * @brief Removes characteristic from maps.
 * @param [in] characteristic The characteristic to remove.
 * @return N/A.
 */
void BLECharacteristicMap::removeCharacteristic(BLECharacteristic *characteristic) {
  m_handleMap.erase(characteristic->getHandle());
  m_uuidMap.erase(characteristic);
}  // removeCharacteristic

/**
 * @brief Set the characteristic by handle.
 * @param [in] handle The handle of the characteristic.
 * @param [in] characteristic The characteristic to cache.
 * @return N/A.
 */
void BLECharacteristicMap::setByHandle(uint16_t handle, BLECharacteristic *characteristic) {
  m_handleMap.insert(std::pair<uint16_t, BLECharacteristic *>(handle, characteristic));
}  // setByHandle

/**
 * @brief Set the characteristic by UUID.
 * @param [in] uuid The uuid of the characteristic.
 * @param [in] characteristic The characteristic to cache.
 * @return N/A.
 */
void BLECharacteristicMap::setByUUID(BLECharacteristic *pCharacteristic, BLEUUID uuid) {
  m_uuidMap.insert(std::pair<BLECharacteristic *, String>(pCharacteristic, uuid.toString()));
}  // setByUUID

/**
 * @brief Return a string representation of the characteristic map.
 * @return A string representation of the characteristic map.
 */
String BLECharacteristicMap::toString() const {
  String res;
  int count = 0;
  char hex[5];
  for (auto &myPair : m_uuidMap) {
    if (count > 0) {
      res += "\n";
    }
    snprintf(hex, sizeof(hex), "%04x", myPair.first->getHandle());
    count++;
    res += "handle: 0x";
    res += hex;
    res += ", uuid: " + myPair.first->getUUID().toString();
  }
  return res;
}  // toString

/***************************************************************************
 *                           Bluedroid functions                           *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
/**
 * @brief Pass the GATT server event onwards to each of the characteristics found in the mapping
 * @param [in] event
 * @param [in] gatts_if
 * @param [in] param
 */
void BLECharacteristicMap::handleGATTServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
  // Invoke the handler for every Service we have.
  for (auto &myPair : m_uuidMap) {
    myPair.first->handleGATTServerEvent(event, gatts_if, param);
  }
}  // handleGATTServerEvent
#endif  // CONFIG_BLUEDROID_ENABLED

/***************************************************************************
 *                           NimBLE functions                              *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
void BLECharacteristicMap::handleGATTServerEvent(uint16_t conn_handle, uint16_t attr_handle, ble_gatt_access_ctxt *ctxt, void *arg) {
  // Invoke the handler for every Service we have.
  for (auto &myPair : m_uuidMap) {
    myPair.first->handleGATTServerEvent(conn_handle, attr_handle, ctxt, arg);
  }
}
#endif  // CONFIG_NIMBLE_ENABLED

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
