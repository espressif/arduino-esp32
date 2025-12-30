/*
 * BLE2902.h
 *
 *  Created on: Jun 25, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 28, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef COMPONENTS_CPP_UTILS_BLE2902_H_
#define COMPONENTS_CPP_UTILS_BLE2902_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include "BLEDescriptor.h"

/**
 * @brief Descriptor for Client Characteristic Configuration.
 *
 * This is a convenience descriptor for the Client Characteristic Configuration which has a UUID of 0x2902.
 *
 * See also:
 * https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
 */

// Class declaration for Bluedroid
#if defined(CONFIG_BLUEDROID_ENABLED)
class BLE2902 : public BLEDescriptor {
#endif

// Class declaration for NimBLE (deprecated)
#if defined(CONFIG_NIMBLE_ENABLED)
  class [[deprecated("NimBLE does not support manually adding 2902 descriptors as they \
are automatically added when the characteristic has notifications or indications enabled. \
Get/Set the notifications/indications properties of the characteristic instead. \
This class will be removed in a future version.")]] BLE2902 : public BLEDescriptor {
#endif

  public:
    /***************************************************************************
   *                         Common public functions                         *
   ***************************************************************************/

    BLE2902();
    bool getNotifications();
    bool getIndications();
    void setNotifications(bool flag);
    void setIndications(bool flag);
  };  // BLE2902

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* COMPONENTS_CPP_UTILS_BLE2902_H_ */
