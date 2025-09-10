/*
 * BLEAddress.cpp
 *
 *  Created on: Jul 2, 2017
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

#include "BLEAddress.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <string.h>
#include <stdio.h>
#include <malloc.h>

#ifdef ARDUINO_ARCH_ESP32
#include "esp32-hal-log.h"
#endif

/***************************************************************************
 *                           NimBLE includes                               *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
/*************************************************
 * NOTE: NimBLE address bytes are in INVERSE ORDER!
 * We will accommodate that fact in these methods.
*************************************************/
#include <algorithm>
#endif

/***************************************************************************
 *                           Common functions                              *
 ***************************************************************************/

BLEAddress::BLEAddress() {
  memset(m_address, 0, ESP_BD_ADDR_LEN);
#if defined(CONFIG_NIMBLE_ENABLED)
  m_addrType = 0;
#endif
}

/**
 * @brief Determine if this address equals another.
 * @param [in] otherAddress The other address to compare against.
 * @return True if the addresses are equal.
 */
bool BLEAddress::equals(const BLEAddress &otherAddress) const {
  return *this == otherAddress;
}

bool BLEAddress::operator==(const BLEAddress &otherAddress) const {
#if defined(CONFIG_NIMBLE_ENABLED)
  if (m_addrType != otherAddress.m_addrType) {
    return false;
  }
#endif
  return memcmp(otherAddress.m_address, m_address, ESP_BD_ADDR_LEN) == 0;
}

bool BLEAddress::operator!=(const BLEAddress &otherAddress) const {
  return !(*this == otherAddress);
}

bool BLEAddress::operator<(const BLEAddress &otherAddress) const {
  return memcmp(m_address, otherAddress.m_address, ESP_BD_ADDR_LEN) < 0;
}

bool BLEAddress::operator<=(const BLEAddress &otherAddress) const {
  return !(*this > otherAddress);
}

bool BLEAddress::operator>=(const BLEAddress &otherAddress) const {
  return !(*this < otherAddress);
}

bool BLEAddress::operator>(const BLEAddress &otherAddress) const {
  return memcmp(m_address, otherAddress.m_address, ESP_BD_ADDR_LEN) > 0;
}

/**
 * @brief Return the native representation of the address.
 * @return The native representation of the address.
 */
uint8_t *BLEAddress::getNative() {
  return m_address;
}

/**
 * @brief Convert a BLE address to a string.
 *
 * A string representation of an address is in the format:
 *
 * ```
 * xx:xx:xx:xx:xx:xx
 * ```
 *
 * @return The string representation of the address.
 */
String BLEAddress::toString() const {
  constexpr size_t size = 18;
  char res[size];

#if defined(CONFIG_BLUEDROID_ENABLED)
  snprintf(res, size, "%02x:%02x:%02x:%02x:%02x:%02x", m_address[0], m_address[1], m_address[2], m_address[3], m_address[4], m_address[5]);
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  snprintf(res, size, "%02x:%02x:%02x:%02x:%02x:%02x", m_address[5], m_address[4], m_address[3], m_address[2], m_address[1], m_address[0]);
#endif

  String ret(res);
  return ret;
}

/***************************************************************************
 *                          Bluedroid functions                            *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)

/**
 * @brief Create an address from the native ESP32 representation.
 * @param [in] address The native representation.
 */
BLEAddress::BLEAddress(esp_bd_addr_t address) {
  memcpy(m_address, address, ESP_BD_ADDR_LEN);
}

/**
 * @brief Create an address from a hex string
 *
 * A hex string is of the format:
 * ```
 * 00:00:00:00:00:00
 * ```
 * which is 17 characters in length.
 *
 * @param [in] stringAddress The hex representation of the address.
 */
BLEAddress::BLEAddress(const String &stringAddress) {
  if (stringAddress.length() != 17) {
    return;
  }

  int data[6];
  sscanf(stringAddress.c_str(), "%x:%x:%x:%x:%x:%x", &data[0], &data[1], &data[2], &data[3], &data[4], &data[5]);

  for (size_t index = 0; index < sizeof(m_address); index++) {
    m_address[index] = (uint8_t)data[index];
  }
}

#endif

/***************************************************************************
 *                          NimBLE functions                               *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
/*************************************************
 * NOTE: NimBLE address bytes are in INVERSE ORDER!
 * We will accommodate that fact in these methods.
*************************************************/

BLEAddress::BLEAddress(uint8_t address[ESP_BD_ADDR_LEN], uint8_t type) {
  std::reverse_copy(address, address + sizeof(m_address), m_address);
  m_addrType = type;
}

BLEAddress::BLEAddress(ble_addr_t address) {
  memcpy(m_address, address.val, ESP_BD_ADDR_LEN);
  m_addrType = address.type;
}

uint8_t BLEAddress::getType() const {
  return m_addrType;
}

/**
 * @brief Create an address from a hex string
 *
 * A hex string is of the format:
 * ```
 * 00:00:00:00:00:00
 * ```
 * which is 17 characters in length.
 *
 * @param [in] stringAddress The hex representation of the address.
 * @param [in] type The address type.
 */
BLEAddress::BLEAddress(const String &stringAddress, uint8_t type) {
  if (stringAddress.length() != 17) {
    return;
  }

  int data[6];
  m_addrType = type;
  // NimBLE addresses are in INVERSE ORDER!
  sscanf(stringAddress.c_str(), "%x:%x:%x:%x:%x:%x", &data[5], &data[4], &data[3], &data[2], &data[1], &data[0]);

  for (size_t index = 0; index < sizeof(m_address); index++) {
    m_address[index] = (uint8_t)data[index];
  }
}

#endif

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
