/*
 * BLEAdvertisedDevice.h
 *
 *  Created on: Jul 3, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef COMPONENTS_CPP_UTILS_BLEADVERTISEDDEVICE_H_
#define COMPONENTS_CPP_UTILS_BLEADVERTISEDDEVICE_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include <map>
#include "BLEAddress.h"
#include "BLEScan.h"
#include "BLEUUID.h"

/***************************************************************************
 *                           Bluedroid includes                            *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gattc_api.h>
#endif

/***************************************************************************
 *                           NimBLE includes                               *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <nimble/ble.h>
#include <host/ble_gap.h>
#endif

/***************************************************************************
 *                            Common types                                 *
 ***************************************************************************/

typedef enum {
  BLE_UNKNOWN_FRAME,
  BLE_EDDYSTONE_UUID_FRAME,
  BLE_EDDYSTONE_URL_FRAME,
  BLE_EDDYSTONE_TLM_FRAME,
  BLE_FRAME_MAX
} ble_frame_type_t;

/***************************************************************************
 *                           Forward declarations                          *
 ***************************************************************************/

class BLEScan;

/**
 * @brief A representation of a %BLE advertised device found by a scan.
 *
 * When we perform a %BLE scan, the result will be a set of devices that are advertising.  This
 * class provides a model of a detected device.
 */
class BLEAdvertisedDevice {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  BLEAdvertisedDevice();
  BLEAddress getAddress();
  uint16_t getAppearance();
  String getManufacturerData();
  String getName();
  int getRSSI();
  BLEScan *getScan();
  String getServiceData();
  String getServiceData(int i);
  BLEUUID getServiceDataUUID();
  BLEUUID getServiceDataUUID(int i);
  BLEUUID getServiceUUID();
  BLEUUID getServiceUUID(int i);
  int getServiceDataCount();
  int getServiceDataUUIDCount();
  int getServiceUUIDCount();
  int8_t getTXPower();
  uint8_t *getPayload();
  size_t getPayloadLength();
  uint8_t getAddressType();
  ble_frame_type_t getFrameType();
  void setAddressType(uint8_t type);
  void setAdvType(uint8_t type);
  uint8_t getAdvType();

  bool isLegacyAdvertisement();
  bool isScannable();
  bool isConnectable();
  bool isAdvertisingService(BLEUUID uuid);
  bool haveAppearance();
  bool haveManufacturerData();
  bool haveName();
  bool haveRSSI();
  bool haveServiceData();
  bool haveServiceUUID();
  bool haveTXPower();

  String toString();

private:
  friend class BLEScan;

  /***************************************************************************
   *                       Common private properties                         *
   ***************************************************************************/

  bool m_haveAppearance;
  bool m_haveManufacturerData;
  bool m_haveName;
  bool m_haveRSSI;
  bool m_haveTXPower;
  BLEAddress m_address = BLEAddress((uint8_t *)"\0\0\0\0\0\0");
  uint8_t m_adFlag;
  uint16_t m_appearance;
  int m_deviceType;
  String m_manufacturerData;
  String m_name;
  BLEScan *m_pScan;
  int m_rssi;
  std::vector<BLEUUID> m_serviceUUIDs;
  int8_t m_txPower;
  std::vector<String> m_serviceData;
  std::vector<BLEUUID> m_serviceDataUUIDs;
  uint8_t *m_payload;
  size_t m_payloadLength = 0;
  uint8_t m_addressType;
  uint8_t m_advType;
  bool m_isLegacyAdv;

  /***************************************************************************
   *                       NimBLE private properties                         *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  bool m_callbackSent;
#endif

  /***************************************************************************
   *                       Common private declarations                       *
   ***************************************************************************/

  void parseAdvertisement(uint8_t *payload, size_t total_len = 62);
  void setPayload(uint8_t *payload, size_t total_len = 62, bool append = false);
  void setAddress(BLEAddress address);
  void setAdFlag(uint8_t adFlag);
  void setAdvertizementResult(uint8_t *payload);
  void setAppearance(uint16_t appearance);
  void setManufacturerData(String manufacturerData);
  void setName(String name);
  void setRSSI(int rssi);
  void setScan(BLEScan *pScan);
  void setServiceData(String data);
  void setServiceDataUUID(BLEUUID uuid);
  void setServiceUUID(const char *serviceUUID);
  void setServiceUUID(BLEUUID serviceUUID);
  void setTXPower(int8_t txPower);
};

/**
 * @brief A callback handler for callbacks associated device scanning.
 *
 * When we are performing a scan as a %BLE client, we may wish to know when a new device that is advertising
 * has been found.  This class can be sub-classed and registered such that when a scan is performed and
 * a new advertised device has been found, we will be called back to be notified.
 */
class BLEAdvertisedDeviceCallbacks {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  virtual ~BLEAdvertisedDeviceCallbacks() {}
  /**
   * @brief Called when a new scan result is detected.
   *
   * As we are scanning, we will find new devices.  When found, this call back is invoked with a reference to the
   * device that was found.  During any individual scan, a device will only be detected one time.
   */
  virtual void onResult(BLEAdvertisedDevice advertisedDevice) = 0;
};

#if defined(SOC_BLE_50_SUPPORTED) && defined(CONFIG_BLUEDROID_ENABLED)
class BLEExtAdvertisingCallbacks {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  virtual ~BLEExtAdvertisingCallbacks() {}

  /***************************************************************************
   *                      Bluedroid public declarations                      *
   ***************************************************************************/

  /**
   * @brief Called when a new scan result is detected.
   *
   * As we are scanning, we will find new devices.  When found, this call back is invoked with a reference to the
   * device that was found.  During any individual scan, a device will only be detected one time.
   */

#if defined(CONFIG_BLUEDROID_ENABLED)
  virtual void onResult(esp_ble_gap_ext_adv_report_t report) = 0;
#endif

  /***************************************************************************
   *                      NimBLE public declarations                        *
   ***************************************************************************/

  // Extended advertising for NimBLE is not supported yet.
#if defined(CONFIG_NIMBLE_ENABLED)
  virtual void onResult(struct ble_gap_ext_disc_desc report) = 0;
#endif
};
#endif  // SOC_BLE_50_SUPPORTED && CONFIG_BLUEDROID_ENABLED

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* COMPONENTS_CPP_UTILS_BLEADVERTISEDDEVICE_H_ */
