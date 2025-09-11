/*
 * BLEValue.cpp
 *
 *  Created on: Jul 17, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/*****************************************************************************
 *                             Common includes                               *
 *****************************************************************************/

#include "BLEValue.h"
#include "esp32-hal-log.h"

/*****************************************************************************
 *                             Common functions                              *
 *****************************************************************************/

BLEValue::BLEValue() {
  m_accumulation = "";
  m_value = "";
  m_readOffset = 0;
}  // BLEValue

/**
 * @brief Add a message part to the accumulation.
 * The accumulation is a growing set of data that is added to until a commit or cancel.
 * @param [in] part A message part being added.
 */
void BLEValue::addPart(const String &part) {
  log_v(">> addPart: length=%d", part.length());
  m_accumulation += part;
}  // addPart

/**
 * @brief Add a message part to the accumulation.
 * The accumulation is a growing set of data that is added to until a commit or cancel.
 * @param [in] pData A message part being added.
 * @param [in] length The number of bytes being added.
 */
void BLEValue::addPart(const uint8_t *pData, size_t length) {
  log_v(">> addPart: length=%d", length);
  m_accumulation += String((char *)pData, length);
}  // addPart

/**
 * @brief Cancel the current accumulation.
 */
void BLEValue::cancel() {
  log_v(">> cancel");
  m_accumulation = "";
  m_readOffset = 0;
}  // cancel

/**
 * @brief Commit the current accumulation.
 * When writing a value, we may find that we write it in "parts" meaning that the writes come in in pieces
 * of the overall message.  After the last part has been received, we may perform a commit which means that
 * we now have the complete message and commit the change as a unit.
 */
void BLEValue::commit() {
  log_v(">> commit");
  // If there is nothing to commit, do nothing.
  if (m_accumulation.length() == 0) {
    return;
  }
  setValue(m_accumulation);
  m_accumulation = "";
  m_readOffset = 0;
}  // commit

/**
 * @brief Get a pointer to the data.
 * @return A pointer to the data.
 */
uint8_t *BLEValue::getData() {
  return (uint8_t *)m_value.c_str();
}

/**
 * @brief Get the length of the data in bytes.
 * @return The length of the data in bytes.
 */
size_t BLEValue::getLength() const {
  return m_value.length();
}  // getLength

/**
 * @brief Get the read offset.
 * @return The read offset into the read.
 */
uint16_t BLEValue::getReadOffset() const {
  return m_readOffset;
}  // getReadOffset

/**
 * @brief Get the current value.
 */
String BLEValue::getValue() const {
  return m_value;
}  // getValue

/**
 * @brief Set the read offset
 * @param [in] readOffset The offset into the read.
 */
void BLEValue::setReadOffset(uint16_t readOffset) {
  m_readOffset = readOffset;
}  // setReadOffset

/**
 * @brief Set the current value.
 */
void BLEValue::setValue(const String &value) {
  m_value = value;
}  // setValue

/**
 * @brief Set the current value.
 * @param [in] pData The data for the current value.
 * @param [in] The length of the new current value.
 */
void BLEValue::setValue(const uint8_t *pData, size_t length) {
  m_value = String((char *)pData, length);
}  // setValue

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
