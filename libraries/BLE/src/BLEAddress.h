/*
 * BLEAddress.h
 *
 *  Created on: Jul 2, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef COMPONENTS_CPP_UTILS_BLEADDRESS_H_
#define COMPONENTS_CPP_UTILS_BLEADDRESS_H_
#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include "WString.h"
#include <esp_bt.h>
#include <string>

/***************************************************************************
 *                           Bluedroid includes                            *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gap_ble_api.h>
#endif

/***************************************************************************
 *                     NimBLE includes and definitions                     *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <nimble/ble.h>
#define ESP_BD_ADDR_LEN BLE_DEV_ADDR_LEN
#endif

/***************************************************************************
 *                            NimBLE types                                 *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
typedef uint8_t esp_bd_addr_t[ESP_BD_ADDR_LEN];
#endif

/**
 * @brief A %BLE device address.
 *
 * Every %BLE device has a unique address which can be used to identify it and form connections.
 */
class BLEAddress {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  BLEAddress();
  bool equals(BLEAddress otherAddress);
  bool operator==(const BLEAddress &otherAddress) const;
  bool operator!=(const BLEAddress &otherAddress) const;
  bool operator<(const BLEAddress &otherAddress) const;
  bool operator<=(const BLEAddress &otherAddress) const;
  bool operator>(const BLEAddress &otherAddress) const;
  bool operator>=(const BLEAddress &otherAddress) const;
  uint8_t *getNative();
  String toString();

  /***************************************************************************
   *                       Bluedroid public declarations                     *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  BLEAddress(esp_bd_addr_t address);
  BLEAddress(String stringAddress);
#endif

  /***************************************************************************
   *                       NimBLE public declarations                        *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  BLEAddress(ble_addr_t address);
  BLEAddress(String stringAddress, uint8_t type = BLE_ADDR_PUBLIC);
  BLEAddress(uint8_t address[ESP_BD_ADDR_LEN], uint8_t type = BLE_ADDR_PUBLIC);
  uint8_t getType();
#endif

private:
  /***************************************************************************
   *                       Common private properties                         *
   ***************************************************************************/

  uint8_t m_address[ESP_BD_ADDR_LEN];

  /***************************************************************************
   *                       NimBLE private properties                         *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  uint8_t m_addrType;
#endif
};

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED */
#endif /* COMPONENTS_CPP_UTILS_BLEADDRESS_H_ */
