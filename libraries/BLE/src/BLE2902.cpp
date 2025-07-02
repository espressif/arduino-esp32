/*
 * BLE2902.cpp
 *
 *  Created on: Jun 25, 2017
 *      Author: kolban
 */

/*
 * See also:
 * https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
 */
#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)

#include "BLE2902.h"

BLE2902::BLE2902() : BLEDescriptor(BLEUUID((uint16_t)0x2902)) {
  uint8_t data[2] = {0, 0};
  setValue(data, 2);
}  // BLE2902

/**
 * @brief Get the notifications value.
 * @return The notifications value.  True if notifications are enabled and false if not.
 */
bool BLE2902::getNotifications() {
  return (getValue()[0] & (1 << 0)) != 0;
}  // getNotifications

/**
 * @brief Get the indications value.
 * @return The indications value.  True if indications are enabled and false if not.
 */
bool BLE2902::getIndications() {
  return (getValue()[0] & (1 << 1)) != 0;
}  // getIndications

/**
 * @brief Set the indications flag.
 * @param [in] flag The indications flag.
 */
void BLE2902::setIndications(bool flag) {
  uint8_t *pValue = getValue();
  if (flag) {
    pValue[0] |= 1 << 1;
  } else {
    pValue[0] &= ~(1 << 1);
  }
  setValue(pValue, 2);
}  // setIndications

/**
 * @brief Set the notifications flag.
 * @param [in] flag The notifications flag.
 */
void BLE2902::setNotifications(bool flag) {
  uint8_t *pValue = getValue();
  if (flag) {
    pValue[0] |= 1 << 0;
  } else {
    pValue[0] &= ~(1 << 0);
  }
  setValue(pValue, 2);
}  // setNotifications

#endif
#endif /* SOC_BLE_SUPPORTED */
