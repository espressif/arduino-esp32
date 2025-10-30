/*
 * BLEDescriptorMap.cpp
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
#include "BLECharacteristic.h"
#include "BLEDescriptor.h"
#ifdef ARDUINO_ARCH_ESP32
#include "esp32-hal-log.h"
#endif

/***************************************************************************
 *                           Bluedroid includes                            *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gatts_api.h>  // ESP32 BLE
#endif

/***************************************************************************
 *                           NimBLE includes                               *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include "host/ble_gatt.h"
#endif

/***************************************************************************
 *                           Common functions                             *
 ***************************************************************************/

/**
 * @brief Return the descriptor by UUID.
 * @param [in] UUID The UUID to look up the descriptor.
 * @return The descriptor.  If not present, then nullptr is returned.
 */
BLEDescriptor *BLEDescriptorMap::getByUUID(const char *uuid) const {
  return getByUUID(BLEUUID(uuid));
}

/**
 * @brief Return the descriptor by UUID.
 * @param [in] UUID The UUID to look up the descriptor.
 * @return The descriptor.  If not present, then nullptr is returned.
 */
BLEDescriptor *BLEDescriptorMap::getByUUID(BLEUUID uuid) const {
  for (auto &myPair : m_uuidMap) {
    if (myPair.first->getUUID().equals(uuid)) {
      return myPair.first;
    }
  }
  //return m_uuidMap.at(uuid.toString());
  return nullptr;
}  // getByUUID

/**
 * @brief Return the descriptor by handle.
 * @param [in] handle The handle to look up the descriptor.
 * @return The descriptor.
 */
BLEDescriptor *BLEDescriptorMap::getByHandle(uint16_t handle) const {
  return m_handleMap.at(handle);
}  // getByHandle

/**
 * @brief Set the descriptor by UUID.
 * @param [in] uuid The uuid of the descriptor.
 * @param [in] characteristic The descriptor to cache.
 * @return N/A.
 */
void BLEDescriptorMap::setByUUID(const char *uuid, BLEDescriptor *pDescriptor) {
  m_uuidMap.insert(std::pair<BLEDescriptor *, String>(pDescriptor, uuid));
}  // setByUUID

/**
 * @brief Set the descriptor by UUID.
 * @param [in] uuid The uuid of the descriptor.
 * @param [in] characteristic The descriptor to cache.
 * @return N/A.
 */
void BLEDescriptorMap::setByUUID(BLEUUID uuid, BLEDescriptor *pDescriptor) {
  m_uuidMap.insert(std::pair<BLEDescriptor *, String>(pDescriptor, uuid.toString()));
}  // setByUUID

/**
 * @brief Set the descriptor by handle.
 * @param [in] handle The handle of the descriptor.
 * @param [in] descriptor The descriptor to cache.
 * @return N/A.
 */
void BLEDescriptorMap::setByHandle(uint16_t handle, BLEDescriptor *pDescriptor) {
  m_handleMap.insert(std::pair<uint16_t, BLEDescriptor *>(handle, pDescriptor));
}  // setByHandle

/**
 * @brief Get the number of registered descriptors.
 * @return The number of registered descriptors.
 */
int BLEDescriptorMap::getRegisteredDescriptorCount() const {
  return m_uuidMap.size();
}

/**
 * @brief Remove a descriptor from the map.
 * @param [in] pDescriptor The descriptor to remove.
 * @return N/A.
 */
void BLEDescriptorMap::removeDescriptor(BLEDescriptor *pDescriptor) {
  m_uuidMap.erase(pDescriptor);
  m_handleMap.erase(pDescriptor->getHandle());
}

/**
 * @brief Return a string representation of the descriptor map.
 * @return A string representation of the descriptor map.
 */
String BLEDescriptorMap::toString() const {
  String res;
  char hex[5];
  int count = 0;
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

/**
 * @brief Get the first descriptor in the map.
 * @return The first descriptor in the map.
 */
BLEDescriptor *BLEDescriptorMap::getFirst() {
  m_iterator = m_uuidMap.begin();
  if (m_iterator == m_uuidMap.end()) {
    return nullptr;
  }
  BLEDescriptor *pRet = m_iterator->first;
  m_iterator++;
  return pRet;
}  // getFirst

/**
 * @brief Get the next descriptor in the map.
 * @return The next descriptor in the map.
 */
BLEDescriptor *BLEDescriptorMap::getNext() {
  if (m_iterator == m_uuidMap.end()) {
    return nullptr;
  }
  BLEDescriptor *pRet = m_iterator->first;
  m_iterator++;
  return pRet;
}  // getNext

/***************************************************************************
 *                           Bluedroid functions                           *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)

/**
 * @brief Pass the GATT server event onwards to each of the descriptors found in the mapping
 * @param [in] event
 * @param [in] gatts_if
 * @param [in] param
 */
void BLEDescriptorMap::handleGATTServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
  // Invoke the handler for every descriptor we have.
  for (auto &myPair : m_uuidMap) {
    myPair.first->handleGATTServerEvent(event, gatts_if, param);
  }
}  // handleGATTServerEvent

#endif

/***************************************************************************
 *                           NimBLE functions                             *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)

void BLEDescriptorMap::handleGATTServerEvent(uint16_t conn_handle, uint16_t attr_handle, ble_gatt_access_ctxt *ctxt, void *arg) {
  // Invoke the handler for every descriptor we have.
  for (auto &myPair : m_uuidMap) {
    myPair.first->handleGATTServerEvent(conn_handle, attr_handle, ctxt, arg);
  }
}

#endif

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
