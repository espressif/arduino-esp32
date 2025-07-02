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

#ifndef COMPONENTS_CPP_UTILS_BLE2901_H_
#define COMPONENTS_CPP_UTILS_BLE2901_H_
#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)

#include "BLEDescriptor.h"

class BLE2901 : public BLEDescriptor {
public:
  BLE2901();
  void setDescription(String desc);
};  // BLE2901

#endif /* CONFIG_BLUEDROID_ENABLED */
#endif /* SOC_BLE_SUPPORTED */
#endif /* COMPONENTS_CPP_UTILS_BLE2901_H_ */
