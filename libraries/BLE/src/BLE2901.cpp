/*
  BLE2901.h

  GATT Descriptor 0x2901 Characteristic User Description

  The value of this description is a user-readable string
  describing the characteristic.

  The Characteristic User Description descriptor
  provides a textual user description for a characteristic
  value.
  If the Writable Auxiliary bit of the Characteristics
  Properties is set then this descriptor is written. Only one
  User Description descriptor exists in a characteristic
  definition.
*/

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                     Common includes and definitions                     *
 ***************************************************************************/

#include "BLE2901.h"

#define BLE2901_UUID 0x2901

/***************************************************************************
 *                     NimBLE includes and definitions                     *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_att.h>
#define ESP_GATT_MAX_ATTR_LEN BLE_ATT_ATTR_MAX_LEN
#endif

/***************************************************************************
 *                           Common functions                              *
 ***************************************************************************/

BLE2901::BLE2901() : BLEDescriptor(BLEUUID((uint16_t)BLE2901_UUID)) {}

/**
 * @brief Set the Characteristic User Description
 */
void BLE2901::setDescription(const String &userDesc) {
  if (userDesc.length() > ESP_GATT_MAX_ATTR_LEN) {
    log_e("Size %d too large, must be no bigger than %d", userDesc.length(), ESP_GATT_MAX_ATTR_LEN);
    return;
  }
  setValue(userDesc);
}

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
