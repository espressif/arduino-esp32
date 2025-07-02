/*
 * BLEValue.h
 *
 *  Created on: Jul 17, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_BLEVALUE_H_
#define COMPONENTS_CPP_UTILS_BLEVALUE_H_
#include "soc/soc_caps.h"
#include "WString.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)

/**
 * @brief The model of a %BLE value.
 */
class BLEValue {
public:
  BLEValue();
  void addPart(String part);
  void addPart(uint8_t *pData, size_t length);
  void cancel();
  void commit();
  uint8_t *getData();
  size_t getLength();
  uint16_t getReadOffset();
  String getValue();
  void setReadOffset(uint16_t readOffset);
  void setValue(String value);
  void setValue(uint8_t *pData, size_t length);

private:
  String m_accumulation;
  uint16_t m_readOffset;
  String m_value;
};
#endif /* CONFIG_BLUEDROID_ENABLED */
#endif /* SOC_BLE_SUPPORTED */
#endif /* COMPONENTS_CPP_UTILS_BLEVALUE_H_ */
