/*
 * BLEAddress.h
 *
 *  Created on: Jul 2, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_BLEADDRESS_H_
#define COMPONENTS_CPP_UTILS_BLEADDRESS_H_
#include "soc/soc_caps.h"
#include "WString.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gap_ble_api.h>  // ESP32 BLE
#include <string>

/**
 * @brief A %BLE device address.
 *
 * Every %BLE device has a unique address which can be used to identify it and form connections.
 */
class BLEAddress {
public:
  BLEAddress(esp_bd_addr_t address);
  BLEAddress(String stringAddress);
  bool equals(BLEAddress otherAddress);
  bool operator==(const BLEAddress &otherAddress) const;
  bool operator!=(const BLEAddress &otherAddress) const;
  bool operator<(const BLEAddress &otherAddress) const;
  bool operator<=(const BLEAddress &otherAddress) const;
  bool operator>(const BLEAddress &otherAddress) const;
  bool operator>=(const BLEAddress &otherAddress) const;
  esp_bd_addr_t *getNative();
  String toString();

private:
  esp_bd_addr_t m_address;
};

#endif /* CONFIG_BLUEDROID_ENABLED */
#endif /* SOC_BLE_SUPPORTED */
#endif /* COMPONENTS_CPP_UTILS_BLEADDRESS_H_ */
