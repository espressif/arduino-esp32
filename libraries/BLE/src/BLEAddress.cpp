/*
 * BLEAddress.cpp
 *
 *  Created on: Jul 2, 2017
 *      Author: kolban
 */
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

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


/**
 * @brief Create an address from the native ESP32 representation.
 * @param [in] address The native representation.
 */
BLEAddress::BLEAddress(esp_bd_addr_t address) {
	memcpy(m_address, address, ESP_BD_ADDR_LEN);
} // BLEAddress


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
BLEAddress::BLEAddress(std::string stringAddress) {
	if (stringAddress.length() != 17) return;

	int data[6];
	sscanf(stringAddress.c_str(), "%x:%x:%x:%x:%x:%x", &data[0], &data[1], &data[2], &data[3], &data[4], &data[5]);
	m_address[0] = (uint8_t) data[0];
	m_address[1] = (uint8_t) data[1];
	m_address[2] = (uint8_t) data[2];
	m_address[3] = (uint8_t) data[3];
	m_address[4] = (uint8_t) data[4];
	m_address[5] = (uint8_t) data[5];
} // BLEAddress


/**
 * @brief Determine if this address equals another.
 * @param [in] otherAddress The other address to compare against.
 * @return True if the addresses are equal.
 */
bool BLEAddress::equals(BLEAddress otherAddress) {
	return memcmp(otherAddress.getNative(), m_address, ESP_BD_ADDR_LEN) == 0;
} // equals

bool BLEAddress::operator==(const BLEAddress& otherAddress) const {
	return memcmp(otherAddress.m_address, m_address, ESP_BD_ADDR_LEN) == 0;
}

bool BLEAddress::operator!=(const BLEAddress& otherAddress) const {
  return !(*this == otherAddress);
}

bool BLEAddress::operator<(const BLEAddress& otherAddress) const {
  return memcmp(otherAddress.m_address, m_address, ESP_BD_ADDR_LEN) < 0;
}

bool BLEAddress::operator<=(const BLEAddress& otherAddress) const {
  return !(*this > otherAddress);
}

bool BLEAddress::operator>=(const BLEAddress& otherAddress) const {
  return !(*this < otherAddress);
}

bool BLEAddress::operator>(const BLEAddress& otherAddress) const {
  return memcmp(otherAddress.m_address, m_address, ESP_BD_ADDR_LEN) > 0;
}

/**
 * @brief Return the native representation of the address.
 * @return The native representation of the address.
 */   
esp_bd_addr_t *BLEAddress::getNative() {
	return &m_address;
} // getNative


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
std::string BLEAddress::toString() {
	auto size = 18;
	char *res = (char*)malloc(size);
	snprintf(res, size, "%02x:%02x:%02x:%02x:%02x:%02x", m_address[0], m_address[1], m_address[2], m_address[3], m_address[4], m_address[5]);
	std::string ret(res);
	free(res);
	return ret;
} // toString
#endif
