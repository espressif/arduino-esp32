/*
 * BLEValue.h
 *
 *  Created on: Jul 17, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef COMPONENTS_CPP_UTILS_BLEVALUE_H_
#define COMPONENTS_CPP_UTILS_BLEVALUE_H_

#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/*****************************************************************************
 *                              Common includes                              *
 *****************************************************************************/

#include "WString.h"

/**
 * @brief The model of a %BLE value.
 */
class BLEValue {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

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
  /***************************************************************************
   *                        Common private properties                        *
   ***************************************************************************/

  String m_accumulation;
  uint16_t m_readOffset;
  String m_value;
};

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED */
#endif /* COMPONENTS_CPP_UTILS_BLEVALUE_H_ */
