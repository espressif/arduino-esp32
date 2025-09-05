/*
 * BLEScan.cpp
 *
 *  Created on: Jul 1, 2017
 *      Author: kolban
 *
 * 	Update: April, 2021
 * 		add BLE5 support
 *
 * 	Modified on: Feb 18, 2025
 * 		Author: lucasssvaz (based on kolban's and h2zero's work)
 * 		Description: Added support for NimBLE
 */

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include <esp_err.h>

#include <map>

#include "BLEAdvertisedDevice.h"
#include "BLEScan.h"
#include "BLEUtils.h"
#include "GeneralUtils.h"
#include "esp32-hal-log.h"

/***************************************************************************
 *                           Common functions                              *
 ***************************************************************************/

/**
 * @brief Constructor
 */
BLEScan::BLEScan() {
  memset(&m_scan_params, 0, sizeof(m_scan_params));  // Initialize all params
#if defined(CONFIG_BLUEDROID_ENABLED)
  m_scan_params.scan_type = BLE_SCAN_TYPE_PASSIVE;  // Default is a passive scan.
  m_scan_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
  m_scan_params.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
  m_scan_params.scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE;
  m_pAdvertisedDeviceCallbacks = nullptr;
  m_stopped = true;
  m_wantDuplicates = false;
  m_shouldParse = true;
  setInterval(100);
  setWindow(100);
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  m_scan_params.filter_policy = BLE_HCI_SCAN_FILT_NO_WL;
  m_scan_params.passive = 1;            // If set, don’t send scan requests to advertisers (i.e., don’t request additional advertising data).
  m_scan_params.limited = 0;            // If set, only discover devices in limited discoverable mode.
  m_scan_params.filter_duplicates = 1;  // If set, the controller ignores all but the first advertisement from each device.
  m_pAdvertisedDeviceCallbacks = nullptr;
  m_ignoreResults = false;
  m_pTaskData = nullptr;
  m_duration = BLE_HS_FOREVER;  // make sure this is non-zero in the event of a host reset
  m_maxResults = 0xFF;
  m_stopped = true;
  m_wantDuplicates = false;
  m_shouldParse = true;
  // This is defined as the time interval from when the Controller started its last LE scan until it begins the subsequent LE scan. (units=0.625 msec)
  setInterval(100);
  // The duration of the LE scan. LE_Scan_Window shall be less than or equal to LE_Scan_Interval (units=0.625 msec)
  setWindow(100);
#endif
}  // BLEScan

/**
 * @brief Scan destructor, release any allocated resources.
 */
BLEScan::~BLEScan() {
  clearResults();
}

/**
 * @brief Should we perform an active or passive scan?
 * The default is a passive scan.  An active scan means that we will wish a scan response.
 * @param [in] active If true, we perform an active scan otherwise a passive scan.
 * @return N/A.
 */
void BLEScan::setActiveScan(bool active) {
#if defined(CONFIG_BLUEDROID_ENABLED)
  if (active) {
    m_scan_params.scan_type = BLE_SCAN_TYPE_ACTIVE;
  } else {
    m_scan_params.scan_type = BLE_SCAN_TYPE_PASSIVE;
  }
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  m_scan_params.passive = !active;
#endif
}  // setActiveScan

/**
 * @brief Set the call backs to be invoked.
 * @param [in] pAdvertisedDeviceCallbacks Call backs to be invoked.
 * @param [in] wantDuplicates  True if we wish to be called back with duplicates.  Default is false.
 * @param [in] shouldParse  True if we wish to parse advertised package or raw payload.  Default is true.
 */
void BLEScan::setAdvertisedDeviceCallbacks(BLEAdvertisedDeviceCallbacks *pAdvertisedDeviceCallbacks, bool wantDuplicates, bool shouldParse) {
  m_wantDuplicates = wantDuplicates;
#if defined(CONFIG_NIMBLE_ENABLED)
  setDuplicateFilter(!wantDuplicates);
#endif
  m_pAdvertisedDeviceCallbacks = pAdvertisedDeviceCallbacks;
  m_shouldParse = shouldParse;
}  // setAdvertisedDeviceCallbacks

/**
 * @brief Set the interval to scan.
 * @param [in] The interval in msecs.
 */
void BLEScan::setInterval(uint16_t intervalMSecs) {
#if defined(CONFIG_BLUEDROID_ENABLED)
  m_scan_params.scan_interval = intervalMSecs / 0.625;
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  m_scan_params.itvl = intervalMSecs / 0.625;
#endif
}  // setInterval

/**
 * @brief Set the window to actively scan.
 * @param [in] windowMSecs How long to actively scan.
 */
void BLEScan::setWindow(uint16_t windowMSecs) {
#if defined(CONFIG_BLUEDROID_ENABLED)
  m_scan_params.scan_window = windowMSecs / 0.625;
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  m_scan_params.window = windowMSecs / 0.625;
#endif
}  // setWindow

// delete peer device from cache after disconnecting, it is required in case we are connecting to devices with not public address
void BLEScan::erase(BLEAddress address) {
  log_i("erase device: %s", address.toString().c_str());
  BLEAdvertisedDevice *advertisedDevice = m_scanResults.m_vectorAdvertisedDevices.find(address.toString().c_str())->second;
  m_scanResults.m_vectorAdvertisedDevices.erase(address.toString().c_str());
  delete advertisedDevice;
}

/**
 * @brief Dump the scan results to the log.
 */
void BLEScanResults::dump() {
  log_v(">> Dump scan results:");
  for (int i = 0; i < getCount(); i++) {
    log_d("- %s", getDevice(i).toString().c_str());
  }
}  // dump

/**
 * @brief Return the count of devices found in the last scan.
 * @return The number of devices found in the last scan.
 */
int BLEScanResults::getCount() {
  return m_vectorAdvertisedDevices.size();
}  // getCount

/**
 * @brief Return the specified device at the given index.
 * The index should be between 0 and getCount()-1.
 * @param [in] i The index of the device.
 * @return The device at the specified index.
 */
BLEAdvertisedDevice BLEScanResults::getDevice(uint32_t i) {
  uint32_t x = 0;
  BLEAdvertisedDevice dev = *m_vectorAdvertisedDevices.begin()->second;
  for (auto it = m_vectorAdvertisedDevices.begin(); it != m_vectorAdvertisedDevices.end(); it++) {
    dev = *it->second;
    if (x == i) {
      break;
    }
    x++;
  }
  return dev;
}

BLEScanResults *BLEScan::getResults() {
  return &m_scanResults;
}

void BLEScan::clearResults() {
  for (auto _dev : m_scanResults.m_vectorAdvertisedDevices) {
    delete _dev.second;
  }
  m_scanResults.m_vectorAdvertisedDevices.clear();
}

/***************************************************************************
 *                         Bluedroid functions                             *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)

#if defined(SOC_BLE_50_SUPPORTED)

void BLEScan::setExtendedScanCallback(BLEExtAdvertisingCallbacks *cb) {
  m_pExtendedScanCb = cb;
}

/**
* @brief           This function is used to enable scanning.
*
* @param[in]       duration : Scan duration
* @param[in]       period  : Time interval from when the Controller started its last Scan Duration until it begins the subsequent Scan Duration.
*
* @return            - ESP_OK : success
*                    - other  : failed
*
*/
esp_err_t BLEScan::startExtScan(uint32_t duration, uint16_t period) {
  esp_err_t rc = esp_ble_gap_start_ext_scan(duration, period);
  if (rc) {
    log_e("extended scan start failed: %d", rc);
  }
  return rc;
}

esp_err_t BLEScan::stopExtScan() {
  esp_err_t rc = esp_ble_gap_stop_ext_scan();
  if (rc) {
    log_e("extended scan stop failed: %d", rc);
  }
  return rc;
}

void BLEScan::setPeriodicScanCallback(BLEPeriodicScanCallbacks *cb) {
  m_pPeriodicScanCb = cb;
}

/**
* @brief           This function is used to set the extended scan parameters to be used on the advertising channels.
*
*
* @return            - ESP_OK : success
*                    - other  : failed
*
*/
esp_err_t BLEScan::setExtScanParams() {
  esp_ble_ext_scan_params_t ext_scan_params = {
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE,
    .cfg_mask = ESP_BLE_GAP_EXT_SCAN_CFG_UNCODE_MASK | ESP_BLE_GAP_EXT_SCAN_CFG_CODE_MASK,
    .uncoded_cfg = {BLE_SCAN_TYPE_ACTIVE, 40, 40},
    .coded_cfg = {BLE_SCAN_TYPE_ACTIVE, 40, 40},
  };

  esp_err_t rc = esp_ble_gap_set_ext_scan_params(&ext_scan_params);
  if (rc) {
    log_e("set extend scan params error, error code = %x", rc);
  }
  return rc;
}

/**
* @brief           This function is used to set the extended scan parameters to be used on the advertising channels.
*
* @param[in]       params : scan parameters
*
* @return            - ESP_OK : success
*                    - other  : failed
*
*/
esp_err_t BLEScan::setExtScanParams(esp_ble_ext_scan_params_t *ext_scan_params) {
  esp_err_t rc = esp_ble_gap_set_ext_scan_params(ext_scan_params);
  if (rc) {
    log_e("set extend scan params error, error code = %x", rc);
  }
  return rc;
}

#endif  // SOC_BLE_50_SUPPORTED

/**
 * @brief Handle GAP events related to scans.
 * @param [in] event The event type for this event.
 * @param [in] param Parameter data for this event.
 */
void BLEScan::handleGAPEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {

    // ---------------------------
    // scan_rst:
    // esp_gap_search_evt_t search_evt
    // esp_bd_addr_t bda
    // esp_bt_dev_type_t dev_type
    // esp_ble_addr_type_t ble_addr_type
    // esp_ble_evt_type_t ble_evt_type
    // int rssi
    // uint8_t ble_adv[ESP_BLE_ADV_DATA_LEN_MAX]
    // int flag
    // int num_resps
    // uint8_t adv_data_len
    // uint8_t scan_rsp_len
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
    {

      switch (param->scan_rst.search_evt) {
        //
        // ESP_GAP_SEARCH_INQ_CMPL_EVT
        //
        // Event that indicates that the duration allowed for the search has completed or that we have been
        // asked to stop.
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
        {
          log_w("ESP_GAP_SEARCH_INQ_CMPL_EVT");
          m_stopped = true;
          m_semaphoreScanEnd.give();
          if (m_scanCompleteCB != nullptr) {
            m_scanCompleteCB(m_scanResults);
          }
          break;
        }  // ESP_GAP_SEARCH_INQ_CMPL_EVT

        //
        // ESP_GAP_SEARCH_INQ_RES_EVT
        //
        // Result that has arrived back from a Scan inquiry.
        case ESP_GAP_SEARCH_INQ_RES_EVT:
        {
          if (m_stopped) {  // If we are not scanning, nothing to do with the extra results.
            break;
          }

          // Examine our list of previously scanned addresses and, if we found this one already,
          // ignore it.
          BLEAddress advertisedAddress(param->scan_rst.bda);
          bool found = false;
          bool shouldDelete = true;

          if (!m_wantDuplicates) {
            if (m_scanResults.m_vectorAdvertisedDevices.count(advertisedAddress.toString().c_str()) != 0) {
              found = true;
            }

            if (found) {  // If we found a previous entry AND we don't want duplicates, then we are done.
              log_d("Ignoring %s, already seen it.", advertisedAddress.toString().c_str());
              vTaskDelay(1);  // <--- allow to switch task in case we scan infinity and dont have new devices to report, or we are blocked here
              break;
            }
          }

          // We now construct a model of the advertised device that we have just found for the first
          // time.
          // ESP_LOG_BUFFER_HEXDUMP((uint8_t*)param->scan_rst.ble_adv, param->scan_rst.adv_data_len + param->scan_rst.scan_rsp_len, ESP_LOG_DEBUG);
          // log_w("bytes length: %d + %d, addr type: %d", param->scan_rst.adv_data_len, param->scan_rst.scan_rsp_len, param->scan_rst.ble_addr_type);
          BLEAdvertisedDevice *advertisedDevice = new BLEAdvertisedDevice();
          advertisedDevice->setAddress(advertisedAddress);
          advertisedDevice->setRSSI(param->scan_rst.rssi);
          advertisedDevice->setAdFlag(param->scan_rst.flag);
          advertisedDevice->setAdvType(param->scan_rst.ble_evt_type);
          if (m_shouldParse) {
            advertisedDevice->parseAdvertisement((uint8_t *)param->scan_rst.ble_adv, param->scan_rst.adv_data_len + param->scan_rst.scan_rsp_len);
          } else {
            advertisedDevice->setPayload((uint8_t *)param->scan_rst.ble_adv, param->scan_rst.adv_data_len + param->scan_rst.scan_rsp_len);
          }
          advertisedDevice->setScan(this);
          advertisedDevice->setAddressType(param->scan_rst.ble_addr_type);

          if (m_pAdvertisedDeviceCallbacks) {  // if has callback, no need to record to vector
            m_pAdvertisedDeviceCallbacks->onResult(*advertisedDevice);
          }
          if (!m_wantDuplicates && !found) {  // if no callback and not want duplicate, and not already in vector, record it
            m_scanResults.m_vectorAdvertisedDevices.insert(std::pair<std::string, BLEAdvertisedDevice *>(advertisedAddress.toString().c_str(), advertisedDevice)
            );
            shouldDelete = false;
          }
          if (shouldDelete) {
            delete advertisedDevice;
          }

          break;
        }  // ESP_GAP_SEARCH_INQ_RES_EVT

        default:
        {
          break;
        }
      }  // switch - search_evt

      break;
    }  // ESP_GAP_BLE_SCAN_RESULT_EVT
#ifdef SOC_BLE_50_SUPPORTED
    case ESP_GAP_BLE_EXT_ADV_REPORT_EVT:
    {
      if (param->ext_adv_report.params.event_type & ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY) {
        log_v("legacy adv, adv type 0x%x data len %d", param->ext_adv_report.params.event_type, param->ext_adv_report.params.adv_data_len);
      } else {
        log_v(
          "extend adv, adv type 0x%x data len %d, data status: %d", param->ext_adv_report.params.event_type, param->ext_adv_report.params.adv_data_len,
          param->ext_adv_report.params.data_status
        );
      }

      if (m_pExtendedScanCb != nullptr) {
        m_pExtendedScanCb->onResult(param->ext_adv_report.params);
      }

      break;
    }

    case ESP_GAP_BLE_SET_EXT_SCAN_PARAMS_COMPLETE_EVT:
    {
      if (param->set_ext_scan_params.status != ESP_BT_STATUS_SUCCESS) {
        log_e("extend scan parameters set failed, error status = %x", param->set_ext_scan_params.status);
        break;
      }
      log_v("extend scan params set successfully");
      break;
    }

    case ESP_GAP_BLE_EXT_SCAN_START_COMPLETE_EVT:
      if (param->ext_scan_start.status != ESP_BT_STATUS_SUCCESS) {
        log_e("scan start failed, error status = %x", param->scan_start_cmpl.status);
        break;
      }
      log_v("Scan start success");
      break;

    case ESP_GAP_BLE_EXT_SCAN_STOP_COMPLETE_EVT:
      if (m_pPeriodicScanCb != nullptr) {
        m_pPeriodicScanCb->onStop(param->ext_scan_stop.status);
      }

      if (param->ext_scan_stop.status != ESP_BT_STATUS_SUCCESS) {
        log_e("extend Scan stop failed, error status = %x", param->ext_scan_stop.status);
        break;
      }
      log_v("Stop extend scan successfully");
      break;

    case ESP_GAP_BLE_PERIODIC_ADV_CREATE_SYNC_COMPLETE_EVT:
      if (m_pPeriodicScanCb != nullptr) {
        m_pPeriodicScanCb->onCreateSync(param->period_adv_create_sync.status);
      }

      log_v("ESP_GAP_BLE_PERIODIC_ADV_CREATE_SYNC_COMPLETE_EVT, status %d", param->period_adv_create_sync.status);
      break;
    case ESP_GAP_BLE_PERIODIC_ADV_SYNC_CANCEL_COMPLETE_EVT:
      if (m_pPeriodicScanCb != nullptr) {
        m_pPeriodicScanCb->onCancelSync(param->period_adv_sync_cancel.status);
      }
      log_v("ESP_GAP_BLE_PERIODIC_ADV_SYNC_CANCEL_COMPLETE_EVT, status %d", param->period_adv_sync_cancel.status);
      break;
    case ESP_GAP_BLE_PERIODIC_ADV_SYNC_TERMINATE_COMPLETE_EVT:
      if (m_pPeriodicScanCb != nullptr) {
        m_pPeriodicScanCb->onTerminateSync(param->period_adv_sync_term.status);
      }
      log_v("ESP_GAP_BLE_PERIODIC_ADV_SYNC_TERMINATE_COMPLETE_EVT, status %d", param->period_adv_sync_term.status);
      break;
    case ESP_GAP_BLE_PERIODIC_ADV_SYNC_LOST_EVT:
      if (m_pPeriodicScanCb != nullptr) {
        m_pPeriodicScanCb->onLostSync(param->periodic_adv_sync_lost.sync_handle);
      }
      log_v("ESP_GAP_BLE_PERIODIC_ADV_SYNC_LOST_EVT, sync handle %d", param->periodic_adv_sync_lost.sync_handle);
      break;
    case ESP_GAP_BLE_PERIODIC_ADV_SYNC_ESTAB_EVT:
      if (m_pPeriodicScanCb != nullptr) {
        m_pPeriodicScanCb->onSync(*(esp_ble_periodic_adv_sync_estab_param_t *)&param->periodic_adv_sync_estab);
      }
      log_v("ESP_GAP_BLE_PERIODIC_ADV_SYNC_ESTAB_EVT, status %d", param->periodic_adv_sync_estab.status);
      break;

    case ESP_GAP_BLE_PERIODIC_ADV_REPORT_EVT:
      if (m_pPeriodicScanCb != nullptr) {
        m_pPeriodicScanCb->onReport(param->period_adv_report.params);
      }
      break;

#endif  // SOC_BLE_50_SUPPORTED

    default:
    {
      break;
    }  // default
  }  // End switch
}  // gapEventHandler

/**
 * @brief Start scanning.
 * @param [in] duration The duration in seconds for which to scan.
 * @param [in] scanCompleteCB A function to be called when scanning has completed.
 * @param [in] are we continue scan (true) or we want to clear stored devices (false)
 * @return True if scan started or false if there was an error.
 */
bool BLEScan::start(uint32_t duration, void (*scanCompleteCB)(BLEScanResults), bool is_continue) {
  log_v(">> start(duration=%d)", duration);

  m_semaphoreScanEnd.take(String("start"));
  m_scanCompleteCB = scanCompleteCB;  // Save the callback to be invoked when the scan completes.

  //  if we are connecting to devices that are advertising even after being connected, multiconnecting peripherals
  //  then we should not clear map or we will connect the same device few times
  if (!is_continue) {
    for (auto _dev : m_scanResults.m_vectorAdvertisedDevices) {
      delete _dev.second;
    }
    m_scanResults.m_vectorAdvertisedDevices.clear();
  }

  esp_err_t errRc = ::esp_ble_gap_set_scan_params(&m_scan_params);

  if (errRc != ESP_OK) {
    log_e("esp_ble_gap_set_scan_params: err: %d, text: %s", errRc, GeneralUtils::errorToString(errRc));
    m_semaphoreScanEnd.give();
    return false;
  }

  errRc = ::esp_ble_gap_start_scanning(duration);

  if (errRc != ESP_OK) {
    log_e("esp_ble_gap_start_scanning: err: %d, text: %s", errRc, GeneralUtils::errorToString(errRc));
    m_semaphoreScanEnd.give();
    return false;
  }

  m_stopped = false;

  log_v("<< start()");
  return true;
}  // start

/**
 * @brief Start scanning and block until scanning has been completed.
 * @param [in] duration The duration in seconds for which to scan.
 * @return The BLEScanResults.
 */
BLEScanResults *BLEScan::start(uint32_t duration, bool is_continue) {
  if (start(duration, nullptr, is_continue)) {
    m_semaphoreScanEnd.wait("start");  // Wait for the semaphore to release.
  }
  return &m_scanResults;
}  // start

/**
 * @brief Stop an in progress scan.
 * @return N/A.
 */
bool BLEScan::stop() {
  log_v(">> stop()");

  esp_err_t errRc = ::esp_ble_gap_stop_scanning();

  m_stopped = true;
  m_semaphoreScanEnd.give();

  if (errRc != ESP_OK) {
    log_e("esp_ble_gap_stop_scanning: err: %d, text: %s", errRc, GeneralUtils::errorToString(errRc));
    return false;
  }

  log_v("<< stop()");
  return true;
}  // stop

#endif  // CONFIG_BLUEDROID_ENABLED

/***************************************************************************
 *                         NimBLE functions                                *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)

/**
 * @brief Set whether or not the BLE controller should only report results
 * from devices it has not already seen.
 * @param [in] enabled If true, scanned devices will only be reported once.
 * @details The controller has a limited buffer and will start reporting
 * duplicate devices once the limit is reached.
 */
void BLEScan::setDuplicateFilter(bool enabled) {
  m_scan_params.filter_duplicates = enabled;
}  // setDuplicateFilter

/**
 * @brief Get the status of the scanner.
 * @return true if scanning or scan starting.
 */
bool BLEScan::isScanning() {
  return ble_gap_disc_active();
}

/**
 * @brief Handle GAP events related to scans.
 * @param [in] event The event type for this event.
 * @param [in] param Parameter data for this event.
 */
int BLEScan::handleGAPEvent(ble_gap_event *event, void *arg) {
  BLEScan *pScan = (BLEScan *)arg;

  switch (event->type) {
    case BLE_GAP_EVENT_DISC:
    {
      if (pScan->m_ignoreResults) {
        log_i("Scan op in progress - ignoring results");
        return 0;
      }

      const auto &disc = event->disc;
      const auto event_type = disc.event_type;
      const bool isLegacyAdv = true;
      BLEAddress advertisedAddress(disc.addr);

      BLEClient *client = BLEDevice::getClientByAddress(advertisedAddress);
      if (client != nullptr && client->isConnected()) {
        log_i("Client %s connected - ignoring event", advertisedAddress.toString().c_str());
        return 0;
      }

      BLEAdvertisedDevice *advertisedDevice = nullptr;

      // If we've seen this device before get a pointer to it from the vector
      for (auto &it : pScan->m_scanResults.m_vectorAdvertisedDevices) {
        if (it.second->getAddress() == advertisedAddress) {
          advertisedDevice = it.second;
          break;
        }
      }

      // If we haven't seen this device before; create a new instance and insert it in the vector.
      // Otherwise just update the relevant parameters of the already known device.
      if (advertisedDevice == nullptr) {
        // Check if we have reach the scan results limit, ignore this one if so.
        // We still need to store each device when maxResults is 0 to be able to append the scan results
        if (pScan->m_maxResults > 0 && pScan->m_maxResults < 0xFF && (pScan->m_scanResults.m_vectorAdvertisedDevices.size() >= pScan->m_maxResults)) {
          return 0;
        }

        if (isLegacyAdv && event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP) {
          log_i("Scan response without advertisement: %s", advertisedAddress.toString().c_str());
        }

        advertisedDevice = new BLEAdvertisedDevice();
        advertisedDevice->setAddress(advertisedAddress);
        advertisedDevice->setAddressType(event->disc.addr.type);
        advertisedDevice->setAdvType(event_type);
        pScan->m_scanResults.m_vectorAdvertisedDevices.insert(
          std::pair<std::string, BLEAdvertisedDevice *>(advertisedAddress.toString().c_str(), advertisedDevice)
        );
        log_i("New advertiser: %s", advertisedAddress.toString().c_str());
      } else if (advertisedDevice != nullptr) {
        log_i("Updated advertiser: %s", advertisedAddress.toString().c_str());
      } else {
        // Scan response from unknown device
        return 0;
      }

      advertisedDevice->setRSSI(event->disc.rssi);

      if (pScan->m_shouldParse) {
        advertisedDevice->parseAdvertisement((uint8_t *)event->disc.data, event->disc.length_data);
      } else {
        advertisedDevice->setPayload((uint8_t *)event->disc.data, event->disc.length_data, event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP);
      }

      advertisedDevice->setScan(pScan);

      if (pScan->m_pAdvertisedDeviceCallbacks) {
        // If not active scanning or scan response is not available
        // report the result to the callback now.
        if (pScan->m_scan_params.passive || !isLegacyAdv || !advertisedDevice->isScannable()) {
          advertisedDevice->m_callbackSent = true;
          pScan->m_pAdvertisedDeviceCallbacks->onResult(*advertisedDevice);

          // Otherwise, wait for the scan response so we can report the complete data.
        } else if (isLegacyAdv && event_type == BLE_HCI_ADV_RPT_EVTYPE_SCAN_RSP) {
          advertisedDevice->m_callbackSent = true;
          pScan->m_pAdvertisedDeviceCallbacks->onResult(*advertisedDevice);
        }
      }

      // If not storing results and we have invoked the callback, delete the device.
      if (pScan->m_maxResults == 0 && advertisedDevice->m_callbackSent) {
        pScan->erase(advertisedAddress);
      }

      return 0;
    }

    case BLE_GAP_EVENT_DISC_COMPLETE:
    {
      log_d("discovery complete; reason=%d", event->disc_complete.reason);

      // If a device advertised with scan response available and it was not received
      // the callback would not have been invoked, so do it here.
      if (pScan->m_pAdvertisedDeviceCallbacks) {
        for (auto &it : pScan->m_scanResults.m_vectorAdvertisedDevices) {
          if (!it.second->m_callbackSent) {
            pScan->m_pAdvertisedDeviceCallbacks->onResult(*(it.second));
          }
        }
      }

      if (pScan->m_maxResults == 0) {
        pScan->clearResults();
      }

      if (pScan->m_scanCompleteCB != nullptr) {
        pScan->m_scanCompleteCB(pScan->m_scanResults);
      }

      if (pScan->m_pTaskData != nullptr) {
        BLEUtils::taskRelease(*pScan->m_pTaskData, event->disc_complete.reason);
      }

      return 0;
    }

    default: return 0;
  }
}  // gapEventHandler

/**
 * @brief Start scanning.
 * @param [in] duration The duration in seconds for which to scan.
 * @param [in] scanCompleteCB A function to be called when scanning has completed.
 * @param [in] is_continue Set to true to save previous scan results, false to clear them.
 * @return True if scan started or false if there was an error.
 */
bool BLEScan::start(uint32_t duration, void (*scanCompleteCB)(BLEScanResults), bool is_continue) {
  log_d(">> start(duration=%d)", duration);

  if (!is_continue) {
    clearResults();
  }

  // Save the callback to be invoked when the scan completes.
  m_scanCompleteCB = scanCompleteCB;
  // Save the duration in the case that the host is reset so we can reuse it.
  m_duration = duration;

  // If 0 duration specified then we assume a continuous scan is desired.
  if (duration == 0) {
    duration = BLE_HS_FOREVER;
  } else {
    // convert duration to milliseconds
    duration = duration * 1000;
  }

  // Set the flag to ignore the results while we are deleting the vector
  if (!is_continue) {
    m_ignoreResults = true;
  }

  int rc = ble_gap_disc(BLEDevice::m_ownAddrType, duration, &m_scan_params, BLEScan::handleGAPEvent, this);

  switch (rc) {
    case 0:
    case BLE_HS_EALREADY: break;

    case BLE_HS_EBUSY: log_e("Unable to scan - connection in progress."); break;

    case BLE_HS_ETIMEOUT_HCI:
    case BLE_HS_EOS:
    case BLE_HS_ECONTROLLER:
    case BLE_HS_ENOTSYNCED:   log_e("Unable to scan - Host Reset"); break;

    default: log_e("Error initiating GAP discovery procedure; rc=%d, %s", rc, BLEUtils::returnCodeToString(rc)); break;
  }

  m_ignoreResults = false;
  log_d("<< start()");

  if (rc != 0 && rc != BLE_HS_EALREADY) {
    return false;
  }
  return true;
}  // start

/**
 * @brief Start scanning and block until scanning has been completed.
 * @param [in] duration The duration in seconds for which to scan.
 * @param [in] is_continue Set to true to save previous scan results, false to clear them.
 * @return The BLEScanResults.
 */
BLEScanResults *BLEScan::start(uint32_t duration, bool is_continue) {
  if (duration == 0) {
    log_w("Blocking scan called with duration = forever");
  }

  if (m_pTaskData != nullptr) {
    log_e("Scan already in progress");
    return &m_scanResults;
  }

  BLETaskData taskData(this);
  m_pTaskData = &taskData;

  if (start(duration, nullptr, is_continue)) {
    BLEUtils::taskWait(taskData, BLE_NPL_TIME_FOREVER);
  }

  m_pTaskData = nullptr;
  return &m_scanResults;
}  // start

/**
 * @brief Stop an in progress scan.
 * @return True if successful.
 */
bool BLEScan::stop() {
  log_d(">> stop()");

  int rc = ble_gap_disc_cancel();
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    log_e("Failed to cancel scan; rc=%d", rc);
    return false;
  }

  if (m_maxResults == 0) {
    clearResults();
  }

  if (rc != BLE_HS_EALREADY && m_scanCompleteCB != nullptr) {
    m_scanCompleteCB(m_scanResults);
  }

  if (m_pTaskData != nullptr) {
    BLEUtils::taskRelease(*m_pTaskData);
  }

  log_d("<< stop()");
  return true;
}  // stop

/**
 * @brief Called when host reset, we set a flag to stop scanning until synced.
 */
void BLEScan::onHostReset() {
  m_ignoreResults = true;
}

/**
 * @brief If the host reset and re-synced this is called.
 * If the application was scanning indefinitely with a callback, restart it.
 */
void BLEScan::onHostSync() {
  m_ignoreResults = false;

  if (m_duration == 0 && m_pAdvertisedDeviceCallbacks != nullptr) {
    start(m_duration, m_scanCompleteCB);
  }
}

#endif  // CONFIG_NIMBLE_ENABLED

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
