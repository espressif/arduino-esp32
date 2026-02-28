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
#include "esp32-hal-log.h"
#include <inttypes.h>

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <Preferences.h>
#endif

#define BLE2902_UUID        0x2902
#define BLE_CCCD_NVS_NS     "ble_cccd"
#define BLE_CCCD_KEY_PREFIX "c"

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

/***************************************************************************
 *                       Bluedroid CCCD persistence                        *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)

/**
 * @brief Generate NVS key for CCCD persistence.
 * Format: "c" + last 4 chars of MAC (no colons) + "_" + handle in hex
 * Example: "c5AC2_002a" for address ending in :5A:C2 and handle 0x002a
 * Max length: 1 (prefix) + 4 (addr) + 1 (_) + 4 (handle) + 1 (null) = 11 chars
 */
String BLE2902::getNvsKey(const BLEAddress &peerAddress, uint16_t charHandle) {
  String addrStr = peerAddress.toString();
  // Get last 4 characters of address (without colons) - e.g., "5AC2" from "XX:XX:XX:XX:5A:C2"
  String shortAddr = addrStr.substring(addrStr.length() - 5);
  shortAddr.replace(":", "");

  char key[24];  // Sized generously to avoid truncation warnings
  snprintf(key, sizeof(key), "%s%s_%04x", BLE_CCCD_KEY_PREFIX, shortAddr.c_str(), charHandle);
  return String(key);
}

/**
 * @brief Persist CCCD value to NVS for a bonded device.
 */
bool BLE2902::persistValue(const BLEAddress &peerAddress, uint16_t charHandle) {
  Preferences prefs;
  if (!prefs.begin(BLE_CCCD_NVS_NS, false)) {
    log_e("Failed to open NVS namespace for CCCD persistence");
    return false;
  }

  String key = getNvsKey(peerAddress, charHandle);
  uint8_t *pValue = getValue();
  uint16_t cccdValue = pValue[0] | (pValue[1] << 8);

  size_t written = prefs.putUShort(key.c_str(), cccdValue);
  prefs.end();

  if (written == 0) {
    log_e("Failed to persist CCCD value to NVS");
    return false;
  }

  log_i("Persisted CCCD value 0x%04X for peer %s, handle 0x%04X (key: %s)", cccdValue, peerAddress.toString().c_str(), charHandle, key.c_str());
  return true;
}

/**
 * @brief Restore CCCD value from NVS for a bonded device.
 */
bool BLE2902::restoreValue(const BLEAddress &peerAddress, uint16_t charHandle) {
  Preferences prefs;
  if (!prefs.begin(BLE_CCCD_NVS_NS, true)) {  // Read-only
    log_d("No CCCD persistence namespace found (first boot?)");
    return false;
  }

  String key = getNvsKey(peerAddress, charHandle);

  if (!prefs.isKey(key.c_str())) {
    prefs.end();
    log_d("No persisted CCCD value for peer %s, handle 0x%04X", peerAddress.toString().c_str(), charHandle);
    return false;
  }

  uint16_t cccdValue = prefs.getUShort(key.c_str(), 0);
  prefs.end();

  // Set the value in the descriptor
  uint8_t data[2] = {(uint8_t)(cccdValue & 0xFF), (uint8_t)((cccdValue >> 8) & 0xFF)};
  setValue(data, 2);

  log_i("Restored CCCD value 0x%04X for peer %s, handle 0x%04X (key: %s)", cccdValue, peerAddress.toString().c_str(), charHandle, key.c_str());
  return true;
}

/**
 * @brief Delete persisted CCCD value from NVS for a device.
 */
bool BLE2902::deletePersistedValue(const BLEAddress &peerAddress, uint16_t charHandle) {
  Preferences prefs;
  if (!prefs.begin(BLE_CCCD_NVS_NS, false)) {
    return false;
  }

  String key = getNvsKey(peerAddress, charHandle);
  bool result = prefs.remove(key.c_str());
  prefs.end();

  if (result) {
    log_i("Deleted persisted CCCD value for peer %s, handle 0x%04X", peerAddress.toString().c_str(), charHandle);
  }
  return result;
}

/**
 * @brief Delete all persisted CCCD values from NVS.
 */
bool BLE2902::deleteAllPersistedValues() {
  Preferences prefs;
  if (!prefs.begin(BLE_CCCD_NVS_NS, false)) {
    return false;
  }

  bool result = prefs.clear();
  prefs.end();

  if (result) {
    log_i("Deleted all persisted CCCD values");
  }
  return result;
}

#endif /* CONFIG_BLUEDROID_ENABLED */

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
