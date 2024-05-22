/*
 * BLE2902.h
 *
 *  Created on: Jun 25, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_BLE2902_H_
#define COMPONENTS_CPP_UTILS_BLE2902_H_
#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)

#include "BLEDescriptor.h"

/**
 * @brief Descriptor for Client Characteristic Configuration.
 *
 * This is a convenience descriptor for the Client Characteristic Configuration which has a UUID of 0x2902.
 *
 * See also:
 * https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
 */
class BLE2902 : public BLEDescriptor {
public:
  BLE2902();
  bool getNotifications();
  bool getIndications();
  void setNotifications(bool flag);
  void setIndications(bool flag);

};  // BLE2902

#endif /* CONFIG_BLUEDROID_ENABLED */
#endif /* SOC_BLE_SUPPORTED */
#endif /* COMPONENTS_CPP_UTILS_BLE2902_H_ */
