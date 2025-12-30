/*
 * BLE2902.cpp
 *
 *  Created on: Jun 25, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

/*
 * See also:
 * https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
 */

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                     Common includes and definitions                     *
 ***************************************************************************/

#include "BLE2902.h"

#define BLE2902_UUID 0x2902

/***************************************************************************
 *                           Common functions                              *
 ***************************************************************************/

BLE2902::BLE2902() : BLEDescriptor(BLEUUID((uint16_t)BLE2902_UUID)) {
#if defined(CONFIG_BLUEDROID_ENABLED)
  uint8_t data[2] = {0, 0};
  setValue(data, 2);
#endif
}

/**
 * @brief Get the notifications value.
 * @return The notifications value.  True if notifications are enabled and false if not.
 */
bool BLE2902::getNotifications() {
#if defined(CONFIG_BLUEDROID_ENABLED)
  return (getValue()[0] & (1 << 0)) != 0;
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  if (m_pCharacteristic != nullptr) {
    return (m_pCharacteristic->getProperties() & BLECharacteristic::PROPERTY_NOTIFY) != 0;
  } else {
    log_w("BLE2902::getNotifications() called on an uninitialized descriptor");
    return false;
  }
#endif
}

/**
 * @brief Get the indications value.
 * @return The indications value.  True if indications are enabled and false if not.
 */
bool BLE2902::getIndications() {
#if defined(CONFIG_BLUEDROID_ENABLED)
  return (getValue()[0] & (1 << 1)) != 0;
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  if (m_pCharacteristic != nullptr) {
    return (m_pCharacteristic->getProperties() & BLECharacteristic::PROPERTY_INDICATE) != 0;
  } else {
    log_w("BLE2902::getIndications() called on an uninitialized descriptor");
    return false;
  }
#endif
}

/**
 * @brief Set the indications flag.
 * @param [in] flag The indications flag.
 */
void BLE2902::setIndications(bool flag) {
#if defined(CONFIG_BLUEDROID_ENABLED)
  uint8_t *pValue = getValue();
  if (flag) {
    pValue[0] |= 1 << 1;
  } else {
    pValue[0] &= ~(1 << 1);
  }
  setValue(pValue, 2);
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  if (m_pCharacteristic != nullptr) {
    m_pCharacteristic->setIndicateProperty(flag);
  } else {
    log_w("BLE2902::setIndications() called on an uninitialized descriptor");
  }
#endif
}

/**
 * @brief Set the notifications flag.
 * @param [in] flag The notifications flag.
 */
void BLE2902::setNotifications(bool flag) {
#if defined(CONFIG_BLUEDROID_ENABLED)
  uint8_t *pValue = getValue();
  if (flag) {
    pValue[0] |= 1 << 0;
  } else {
    pValue[0] &= ~(1 << 0);
  }
  setValue(pValue, 2);
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  if (m_pCharacteristic != nullptr) {
    m_pCharacteristic->setNotifyProperty(flag);
  } else {
    log_w("BLE2902::setNotifications() called on an uninitialized descriptor");
  }
#endif
}

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
