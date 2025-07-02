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
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)

#include "BLE2901.h"

BLE2901::BLE2901() : BLEDescriptor(BLEUUID((uint16_t)0x2901)) {}  // BLE2901

/**
 * @brief Set the Characteristic User Description
 */
void BLE2901::setDescription(String userDesc) {
  if (userDesc.length() > ESP_GATT_MAX_ATTR_LEN) {
    log_e("Size %d too large, must be no bigger than %d", userDesc.length(), ESP_GATT_MAX_ATTR_LEN);
    return;
  }
  setValue(userDesc);
}

#endif
#endif /* SOC_BLE_SUPPORTED */
