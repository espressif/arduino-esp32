/*
 * BTAddress.cpp
 *
 *  Created on: Jul 2, 2017
 *      Author: kolban
 *  Ported  on: Feb 5, 2021
 *      Author: Thomas M. (ArcticSnowSky)
 */
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED) && defined(CONFIG_BLUEDROID_ENABLED)

#include "BTAddress.h"
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
BTAddress::BTAddress(esp_bd_addr_t address) {
  memcpy(m_address, address, ESP_BD_ADDR_LEN);
}  // BTAddress

BTAddress::BTAddress() {
  bzero(m_address, ESP_BD_ADDR_LEN);
}  // BTAddress

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
BTAddress::BTAddress(String stringAddress) {
  if (stringAddress.length() != 17) {
    return;
  }

  int data[6];
  sscanf(stringAddress.c_str(), "%x:%x:%x:%x:%x:%x", &data[0], &data[1], &data[2], &data[3], &data[4], &data[5]);
  m_address[0] = (uint8_t)data[0];
  m_address[1] = (uint8_t)data[1];
  m_address[2] = (uint8_t)data[2];
  m_address[3] = (uint8_t)data[3];
  m_address[4] = (uint8_t)data[4];
  m_address[5] = (uint8_t)data[5];
}  // BTAddress

/**
 * @brief Determine if this address equals another.
 * @param [in] otherAddress The other address to compare against.
 * @return True if the addresses are equal.
 */
bool BTAddress::equals(BTAddress otherAddress) {
  return memcmp(otherAddress.getNative(), m_address, 6) == 0;
}  // equals

BTAddress::operator bool() const {
  for (int i = 0; i < ESP_BD_ADDR_LEN; i++) {
    if (this->m_address[i]) {
      return true;
    }
  }
  return false;
}  // operator ()

/**
 * @brief Return the native representation of the address.
 * @return The native representation of the address.
 */
esp_bd_addr_t *BTAddress::getNative() const {
  return const_cast<esp_bd_addr_t *>(&m_address);
}  // getNative

/**
 * @brief Convert a BT address to a string.
 * @param [in] capital changes the letter size
 * By default the parameter `capital` == false and the string representation of an address is in the format:
 * ```
 * xx:xx:xx:xx:xx:xx
 * ```
 * When the parameter `capital` == true the format uses capital letters:
 * ```
 * XX:XX:XX:XX:XX:XX
 * ```
 * @return The string representation of the address.
 */
String BTAddress::toString(bool capital) const {
  auto size = 18;
  char *res = (char *)malloc(size);
  if (capital) {
    snprintf(res, size, "%02X:%02X:%02X:%02X:%02X:%02X", m_address[0], m_address[1], m_address[2], m_address[3], m_address[4], m_address[5]);
  } else {
    snprintf(res, size, "%02x:%02x:%02x:%02x:%02x:%02x", m_address[0], m_address[1], m_address[2], m_address[3], m_address[4], m_address[5]);
  }
  String ret(res);
  free(res);
  return ret;
}  // toString
#endif
