/*
 * BLEUUID.h
 *
 *  Created on: Jun 21, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef COMPONENTS_CPP_UTILS_BLEUUID_H_
#define COMPONENTS_CPP_UTILS_BLEUUID_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/*****************************************************************************
 *                             Common includes                               *
 *****************************************************************************/

#include "WString.h"

/*****************************************************************************
 *                     Bluedroid includes and definitions                    *
 *****************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gatt_defs.h>
#define BLE_UUID_16_BITS  ESP_UUID_LEN_16
#define BLE_UUID_32_BITS  ESP_UUID_LEN_32
#define BLE_UUID_128_BITS ESP_UUID_LEN_128
#define UUID_LEN(s)       s.len
#define UUID_VAL_16(s)    s.uuid.uuid16
#define UUID_VAL_32(s)    s.uuid.uuid32
#define UUID_VAL_128(s)   s.uuid.uuid128
#endif

/*****************************************************************************
 *                       NimBLE includes and definitions                     *
 *****************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_uuid.h>
#define BLE_UUID_16_BITS  BLE_UUID_TYPE_16
#define BLE_UUID_32_BITS  BLE_UUID_TYPE_32
#define BLE_UUID_128_BITS BLE_UUID_TYPE_128
#define UUID_LEN(s)       s.u.type
#define UUID_VAL_16(s)    s.u16.value
#define UUID_VAL_32(s)    s.u32.value
#define UUID_VAL_128(s)   s.u128.value
#endif

/**
 * @brief A model of a %BLE UUID.
 */
class BLEUUID {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  BLEUUID(String uuid);
  BLEUUID(uint16_t uuid);
  BLEUUID(uint32_t uuid);
  BLEUUID(uint8_t *pData, size_t size, bool msbFirst);
  BLEUUID(uint32_t first, uint16_t second, uint16_t third, uint64_t fourth);
  BLEUUID();
  uint8_t bitSize();  // Get the number of bits in this uuid.
  bool equals(const BLEUUID &uuid) const;
  BLEUUID to128();
  BLEUUID to16();
  String toString() const;
  static BLEUUID fromString(String uuid);  // Create a BLEUUID from a string

  bool operator==(const BLEUUID &rhs) const;
  bool operator!=(const BLEUUID &rhs) const;

  /***************************************************************************
   *                      Bluedroid public declarations                      *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  BLEUUID(esp_bt_uuid_t uuid);
  BLEUUID(esp_gatt_id_t gattId);
  esp_bt_uuid_t *getNative();
#endif

  /***************************************************************************
   *                        NimBLE public declarations                       *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  BLEUUID(ble_uuid_any_t uuid);
  BLEUUID(const ble_uuid128_t *uuid);
  const ble_uuid_any_t *getNative() const;
#endif

private:
  /***************************************************************************
   *                        Common private properties                        *
   ***************************************************************************/

  bool m_valueSet = false;  // Is there a value set for this instance.

  /***************************************************************************
   *                       Bluedroid private properties                      *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  esp_bt_uuid_t m_uuid;  // The underlying UUID structure that this class wraps.
#endif

  /***************************************************************************
   *                       NimBLE private properties                         *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  ble_uuid_any_t m_uuid;  // The underlying UUID structure that this class wraps.
#endif
};  // BLEUUID

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* COMPONENTS_CPP_UTILS_BLEUUID_H_ */
