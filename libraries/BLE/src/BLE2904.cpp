/*
 * BLE2904.cpp
 *
 *  Created on: Dec 23, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

/*
 * See also:
 * https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.characteristic_presentation_format.xml
 */

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                     Common includes and definitions                     *
 ***************************************************************************/

#include "BLE2904.h"

#define BLE2904_UUID              0x2904
#define BLE2904_DEFAULT_NAMESPACE 1       // 1 = Bluetooth SIG Assigned Numbers
#define BLE2904_DEFAULT_UNIT      0x2700  // 0x2700 = Unitless

/***************************************************************************
 *                           Common functions                              *
 ***************************************************************************/

BLE2904::BLE2904() : BLEDescriptor(BLEUUID((uint16_t)BLE2904_UUID)) {
  m_data.m_format = 0;
  m_data.m_exponent = 0;
  m_data.m_namespace = BLE2904_DEFAULT_NAMESPACE;
  m_data.m_unit = BLE2904_DEFAULT_UNIT;
  m_data.m_description = 0;
  setValue((uint8_t *)&m_data, sizeof(m_data));
}

/**
 * @brief Set the description.
 */
void BLE2904::setDescription(uint16_t description) {
  m_data.m_description = description;
  setValue((uint8_t *)&m_data, sizeof(m_data));
}

/**
 * @brief Set the exponent.
 */
void BLE2904::setExponent(int8_t exponent) {
  m_data.m_exponent = exponent;
  setValue((uint8_t *)&m_data, sizeof(m_data));
}

/**
 * @brief Set the format.
 */
void BLE2904::setFormat(uint8_t format) {
  m_data.m_format = format;
  setValue((uint8_t *)&m_data, sizeof(m_data));
}

/**
 * @brief Set the namespace.
 */
void BLE2904::setNamespace(uint8_t namespace_value) {
  m_data.m_namespace = namespace_value;
  setValue((uint8_t *)&m_data, sizeof(m_data));
}

/**
 * @brief Set the units for this value.  It should be one of the encoded values defined here:
 * https://www.bluetooth.com/specifications/assigned-numbers/units
 * @param [in] unit The type of units of this characteristic as defined by assigned numbers.
 */
void BLE2904::setUnit(uint16_t unit) {
  m_data.m_unit = unit;
  setValue((uint8_t *)&m_data, sizeof(m_data));
}

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
