/*
 * BLE2904.h
 *
 *  Created on: Dec 23, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_BLE2904_H_
#define COMPONENTS_CPP_UTILS_BLE2904_H_
#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)

#include "BLEDescriptor.h"

struct BLE2904_Data {
  uint8_t m_format;
  int8_t m_exponent;
  uint16_t m_unit;  // See https://www.bluetooth.com/specifications/assigned-numbers/units
  uint8_t m_namespace;
  uint16_t m_description;

} __attribute__((packed));

/**
 * @brief Descriptor for Characteristic Presentation Format.
 *
 * This is a convenience descriptor for the Characteristic Presentation Format which has a UUID of 0x2904.
 *
 * See also:
 * https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.characteristic_presentation_format.xml
 */
class BLE2904 : public BLEDescriptor {
public:
  BLE2904();
  static const uint8_t FORMAT_BOOLEAN = 1;
  static const uint8_t FORMAT_UINT2 = 2;
  static const uint8_t FORMAT_UINT4 = 3;
  static const uint8_t FORMAT_UINT8 = 4;
  static const uint8_t FORMAT_UINT12 = 5;
  static const uint8_t FORMAT_UINT16 = 6;
  static const uint8_t FORMAT_UINT24 = 7;
  static const uint8_t FORMAT_UINT32 = 8;
  static const uint8_t FORMAT_UINT48 = 9;
  static const uint8_t FORMAT_UINT64 = 10;
  static const uint8_t FORMAT_UINT128 = 11;
  static const uint8_t FORMAT_SINT8 = 12;
  static const uint8_t FORMAT_SINT12 = 13;
  static const uint8_t FORMAT_SINT16 = 14;
  static const uint8_t FORMAT_SINT24 = 15;
  static const uint8_t FORMAT_SINT32 = 16;
  static const uint8_t FORMAT_SINT48 = 17;
  static const uint8_t FORMAT_SINT64 = 18;
  static const uint8_t FORMAT_SINT128 = 19;
  static const uint8_t FORMAT_FLOAT32 = 20;
  static const uint8_t FORMAT_FLOAT64 = 21;
  static const uint8_t FORMAT_SFLOAT16 = 22;
  static const uint8_t FORMAT_SFLOAT32 = 23;
  static const uint8_t FORMAT_IEEE20601 = 24;
  static const uint8_t FORMAT_UTF8 = 25;
  static const uint8_t FORMAT_UTF16 = 26;
  static const uint8_t FORMAT_OPAQUE = 27;

  void setDescription(uint16_t);
  void setExponent(int8_t exponent);
  void setFormat(uint8_t format);
  void setNamespace(uint8_t namespace_value);
  void setUnit(uint16_t unit);

private:
  BLE2904_Data m_data;
};  // BLE2904

#endif /* CONFIG_BLUEDROID_ENABLED */
#endif /* SOC_BLE_SUPPORTED */
#endif /* COMPONENTS_CPP_UTILS_BLE2904_H_ */
