/*
 * BTAddress.h
 *
 *  Created on: Jul 2, 2017
 *      Author: kolban
 *  Ported  on: Feb 5, 2021
 *      Author: Thomas M. (ArcticSnowSky)
 */

#ifndef COMPONENTS_CPP_UTILS_BTADDRESS_H_
#define COMPONENTS_CPP_UTILS_BTADDRESS_H_
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED) && defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gap_bt_api.h>  // ESP32 BT
#include <Arduino.h>


/**
 * @brief A %BT device address.
 *
 * Every %BT device has a unique address which can be used to identify it and form connections.
 */
class BTAddress {
public:
  BTAddress();
  BTAddress(esp_bd_addr_t address);
  BTAddress(String stringAddress);
  bool equals(BTAddress otherAddress);
  operator bool() const;

  esp_bd_addr_t* getNative() const;
  String toString(bool capital = false) const;

private:
  esp_bd_addr_t m_address;
};

#endif /* CONFIG_BT_ENABLED */
#endif /* COMPONENTS_CPP_UTILS_BTADDRESS_H_ */
