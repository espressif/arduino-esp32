/*
 * BLE2904.cpp
 *
 *  Created on: Dec 23, 2017
 *      Author: kolban
 */

/*
 * See also:
 * https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.characteristic_presentation_format.xml
 */
#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)

#include "BLE2904.h"

BLE2904::BLE2904() : BLEDescriptor(BLEUUID((uint16_t)0x2904)) {
  m_data.m_format = 0;
  m_data.m_exponent = 0;
  m_data.m_namespace = 1;  // 1 = Bluetooth SIG Assigned Numbers
  m_data.m_unit = 0;
  m_data.m_description = 0;
  setValue((uint8_t *)&m_data, sizeof(m_data));
}  // BLE2902

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
}  // setExponent

/**
 * @brief Set the format.
 */
void BLE2904::setFormat(uint8_t format) {
  m_data.m_format = format;
  setValue((uint8_t *)&m_data, sizeof(m_data));
}  // setFormat

/**
 * @brief Set the namespace.
 */
void BLE2904::setNamespace(uint8_t namespace_value) {
  m_data.m_namespace = namespace_value;
  setValue((uint8_t *)&m_data, sizeof(m_data));
}  // setNamespace

/**
 * @brief Set the units for this value.  It should be one of the encoded values defined here:
 * https://www.bluetooth.com/specifications/assigned-numbers/units
 * @param [in] unit The type of units of this characteristic as defined by assigned numbers.
 */
void BLE2904::setUnit(uint16_t unit) {
  m_data.m_unit = unit;
  setValue((uint8_t *)&m_data, sizeof(m_data));
}  // setUnit

#endif
#endif /* SOC_BLE_SUPPORTED */
