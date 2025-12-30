/*
 * BLEScan.h
 *
 *  Created on: Jul 1, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#ifndef COMPONENTS_CPP_UTILS_BLESCAN_H_
#define COMPONENTS_CPP_UTILS_BLESCAN_H_

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                         Common includes                                 *
 ***************************************************************************/

#include <string>
#include "BLEAdvertisedDevice.h"
#include "BLEClient.h"
#include "BLEUtils.h"
#include "RTOS.h"

/***************************************************************************
 *                         Bluedroid includes                              *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_gap_ble_api.h>
#endif

/***************************************************************************
 *                           NimBLE includes                               *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_gap.h>
#endif

/***************************************************************************
 *                         Forward declarations                            *
 ***************************************************************************/

class BLEAdvertisedDevice;
class BLEAdvertisedDeviceCallbacks;
class BLEExtAdvertisingCallbacks;
class BLEClient;
class BLEScan;
class BLEPeriodicScanCallbacks;
struct BLETaskData;

/***************************************************************************
 *                       Bluedroid type definitions                        *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
struct esp_ble_periodic_adv_sync_estab_param_t {
  uint8_t status;                    /*!< periodic advertising sync status */
  uint16_t sync_handle;              /*!< periodic advertising sync handle */
  uint8_t sid;                       /*!< periodic advertising sid */
  esp_ble_addr_type_t adv_addr_type; /*!< periodic advertising address type */
  esp_bd_addr_t adv_addr;            /*!< periodic advertising address */
  esp_ble_gap_phy_t adv_phy;         /*!< periodic advertising phy type */
  uint16_t period_adv_interval;      /*!< periodic advertising interval */
  uint8_t adv_clk_accuracy;          /*!< periodic advertising clock accuracy */
};
#endif

/**
 * @brief The result of having performed a scan.
 * When a scan completes, we have a set of found devices.  Each device is described
 * by a BLEAdvertisedDevice object.  The number of items in the set is given by
 * getCount().  We can retrieve a device by calling getDevice() passing in the
 * index (starting at 0) of the desired device.
 */
class BLEScanResults {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  void dump();
  int getCount();
  BLEAdvertisedDevice getDevice(uint32_t i);

private:
  friend BLEScan;

  /***************************************************************************
   *                       Common private properties                         *
   ***************************************************************************/

  std::map<std::string, BLEAdvertisedDevice *> m_vectorAdvertisedDevices;
};

/**
 * @brief Perform and manage %BLE scans.
 *
 * Scanning is associated with a %BLE client that is attempting to locate BLE servers.
 */
class BLEScan {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  ~BLEScan();
  void setActiveScan(bool active);
  void setAdvertisedDeviceCallbacks(BLEAdvertisedDeviceCallbacks *pAdvertisedDeviceCallbacks, bool wantDuplicates = false, bool shouldParse = true);
  void setInterval(uint16_t intervalMSecs);
  void setWindow(uint16_t windowMSecs);
  bool start(uint32_t duration, void (*scanCompleteCB)(BLEScanResults), bool is_continue = false);
  BLEScanResults *start(uint32_t duration, bool is_continue = false);
  bool stop();
  void erase(BLEAddress address);
  BLEScanResults *getResults();
  void clearResults();

  /***************************************************************************
   *                       Bluedroid public declarations                     *
   ***************************************************************************/

#if defined(SOC_BLE_50_SUPPORTED) && defined(CONFIG_BLUEDROID_ENABLED)
  void setExtendedScanCallback(BLEExtAdvertisingCallbacks *cb);
  void setPeriodicScanCallback(BLEPeriodicScanCallbacks *cb);
  esp_err_t stopExtScan();
  esp_err_t setExtScanParams();
  esp_err_t startExtScan(uint32_t duration, uint16_t period);
  esp_err_t setExtScanParams(esp_ble_ext_scan_params_t *ext_scan_params);
#endif  // SOC_BLE_50_SUPPORTED

  /***************************************************************************
   *                       NimBLE public declarations                        *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  void setDuplicateFilter(bool enabled);
  bool isScanning();
  void clearDuplicateCache();
#endif

private:
  friend class BLEDevice;

  /***************************************************************************
   *                       Common private properties                         *
   ***************************************************************************/

  BLEAdvertisedDeviceCallbacks *m_pAdvertisedDeviceCallbacks = nullptr;
  bool m_stopped = true;
  bool m_shouldParse = true;
  FreeRTOS::Semaphore m_semaphoreScanEnd = FreeRTOS::Semaphore("ScanEnd");
  BLEScanResults m_scanResults;
  bool m_wantDuplicates;
  void (*m_scanCompleteCB)(BLEScanResults scanResults);

  /***************************************************************************
   *                       Bluedroid private properties                       *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  esp_ble_scan_params_t m_scan_params;
#if defined(SOC_BLE_50_SUPPORTED)
  BLEExtAdvertisingCallbacks *m_pExtendedScanCb = nullptr;
  BLEPeriodicScanCallbacks *m_pPeriodicScanCb = nullptr;
#endif  // SOC_BLE_50_SUPPORTED
#endif

  /***************************************************************************
   *                       NimBLE private properties                         *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  uint32_t m_duration;
  ble_gap_disc_params m_scan_params;
  bool m_ignoreResults;
  BLETaskData *m_pTaskData;
  uint8_t m_maxResults;
#endif

  /***************************************************************************
   *                       Common private definitions                        *
   ***************************************************************************/

  BLEScan();  // One doesn't create a new instance instead one asks the BLEDevice for the singleton.

  /***************************************************************************
   *                       Bluedroid private definitions                     *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  void handleGAPEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
#endif

  /***************************************************************************
   *                       NimBLE private definitions                        *
   ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
  void onHostReset();
  void onHostSync();
  static int handleGAPEvent(ble_gap_event *event, void *arg);
#endif
};  // BLEScan

class BLEPeriodicScanCallbacks {
public:
  /***************************************************************************
   *                       Common public declarations                        *
   ***************************************************************************/

  virtual ~BLEPeriodicScanCallbacks() {}
  virtual void onLostSync(uint16_t sync_handle) {}

  /***************************************************************************
   *                       Bluedroid public declarations                     *
   ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
  virtual void onCreateSync(esp_bt_status_t status) {}
  virtual void onCancelSync(esp_bt_status_t status) {}
  virtual void onTerminateSync(esp_bt_status_t status) {}
  virtual void onSync(esp_ble_periodic_adv_sync_estab_param_t) {}
  virtual void onReport(esp_ble_gap_periodic_adv_report_t params) {}
  virtual void onStop(esp_bt_status_t status) {}
#endif
};

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */

#endif /* COMPONENTS_CPP_UTILS_BLESCAN_H_ */
