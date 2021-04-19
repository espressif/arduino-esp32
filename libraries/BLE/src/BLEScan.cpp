/*
 * BLEScan.cpp
 *
 *  Created on: Jul 1, 2017
 *      Author: kolban
 * 
 * 	Update: April, 2021
 * 		add BLE5 support
 */
#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)


#include <esp_err.h>

#include <map>

#include "BLEAdvertisedDevice.h"
#include "BLEScan.h"
#include "BLEUtils.h"
#include "GeneralUtils.h"
#include "esp32-hal-log.h"

/**
 * Constructor
 */
BLEScan::BLEScan() {
	memset(&m_scan_params, 0, sizeof(m_scan_params)); // Initialize all params
	m_scan_params.scan_type          = BLE_SCAN_TYPE_PASSIVE; // Default is a passive scan.
	m_scan_params.own_addr_type      = BLE_ADDR_TYPE_PUBLIC;
	m_scan_params.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
	m_scan_params.scan_duplicate     = BLE_SCAN_DUPLICATE_DISABLE;
	m_pAdvertisedDeviceCallbacks     = nullptr;
	m_stopped                        = true;
	m_wantDuplicates                 = false;
	m_shouldParse                    = true;
	setInterval(100);
	setWindow(100);
} // BLEScan


/**
 * @brief Handle GAP events related to scans.
 * @param [in] event The event type for this event.
 * @param [in] param Parameter data for this event.
 */
void BLEScan::handleGAPEvent(
	esp_gap_ble_cb_event_t  event,
	esp_ble_gap_cb_param_t* param) {

	switch(event) {

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
		case ESP_GAP_BLE_SCAN_RESULT_EVT: {

			switch(param->scan_rst.search_evt) {
				//
				// ESP_GAP_SEARCH_INQ_CMPL_EVT
				//
				// Event that indicates that the duration allowed for the search has completed or that we have been
				// asked to stop.
				case ESP_GAP_SEARCH_INQ_CMPL_EVT: {
					log_w("ESP_GAP_SEARCH_INQ_CMPL_EVT");
					m_stopped = true;
					m_semaphoreScanEnd.give();
					if (m_scanCompleteCB != nullptr) {
						m_scanCompleteCB(m_scanResults);
					}
					break;
				} // ESP_GAP_SEARCH_INQ_CMPL_EVT

				//
				// ESP_GAP_SEARCH_INQ_RES_EVT
				//
				// Result that has arrived back from a Scan inquiry.
				case ESP_GAP_SEARCH_INQ_RES_EVT: {
					if (m_stopped) { // If we are not scanning, nothing to do with the extra results.
						break;
					}

// Examine our list of previously scanned addresses and, if we found this one already,
// ignore it.
					BLEAddress advertisedAddress(param->scan_rst.bda);
					bool found = false;
					bool shouldDelete = true;

					if (!m_wantDuplicates) {
						if (m_scanResults.m_vectorAdvertisedDevices.count(advertisedAddress.toString()) != 0) {
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
					if (m_shouldParse) {
						advertisedDevice->parseAdvertisement((uint8_t*)param->scan_rst.ble_adv, param->scan_rst.adv_data_len + param->scan_rst.scan_rsp_len);
					} else {
						advertisedDevice->setPayload((uint8_t*)param->scan_rst.ble_adv, param->scan_rst.adv_data_len + param->scan_rst.scan_rsp_len);
					}
					advertisedDevice->setScan(this);
					advertisedDevice->setAddressType(param->scan_rst.ble_addr_type);

					if (m_pAdvertisedDeviceCallbacks) { // if has callback, no need to record to vector
						m_pAdvertisedDeviceCallbacks->onResult(*advertisedDevice);
					} else if (!m_wantDuplicates && !found) {   // if no callback and not want duplicate, and not already in vector, record it
						m_scanResults.m_vectorAdvertisedDevices.insert(std::pair<std::string, BLEAdvertisedDevice*>(advertisedAddress.toString(), advertisedDevice));
						shouldDelete = false;
					}
					if (shouldDelete) {
						delete advertisedDevice;
					}

					break;
				} // ESP_GAP_SEARCH_INQ_RES_EVT

				default: {
					break;
				}
			} // switch - search_evt


			break;
		} // ESP_GAP_BLE_SCAN_RESULT_EVT
#ifdef CONFIG_BT_BLE_50_FEATURES_SUPPORTED
		case ESP_GAP_BLE_EXT_ADV_REPORT_EVT: {
			if (param->ext_adv_report.params.event_type & ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY) {
				log_v("legacy adv, adv type 0x%x data len %d", param->ext_adv_report.params.event_type, param->ext_adv_report.params.adv_data_len);
			}
			else {
				log_v("extend adv, adv type 0x%x data len %d, data status: %d", param->ext_adv_report.params.event_type, param->ext_adv_report.params.adv_data_len, param->ext_adv_report.params.data_status);
			}

			if (m_pExtendedScanCb != nullptr)
			{
				m_pExtendedScanCb->onResult(param->ext_adv_report.params);
			}
			
			break;
		}

		case ESP_GAP_BLE_SET_EXT_SCAN_PARAMS_COMPLETE_EVT: {
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
			if (m_pPeriodicScanCb != nullptr)
			{
				m_pPeriodicScanCb->onStop(param->ext_scan_stop.status);
			}
			
			if (param->ext_scan_stop.status != ESP_BT_STATUS_SUCCESS){
				log_e("extend Scan stop failed, error status = %x", param->ext_scan_stop.status);
				break;
			}
			log_v("Stop extend scan successfully");
			break;

		case ESP_GAP_BLE_PERIODIC_ADV_CREATE_SYNC_COMPLETE_EVT:
			if (m_pPeriodicScanCb != nullptr)
			{
				m_pPeriodicScanCb->onCreateSync(param->period_adv_create_sync.status);
			}

			log_v("ESP_GAP_BLE_PERIODIC_ADV_CREATE_SYNC_COMPLETE_EVT, status %d", param->period_adv_create_sync.status);
			break;
		case ESP_GAP_BLE_PERIODIC_ADV_SYNC_CANCEL_COMPLETE_EVT:
			if (m_pPeriodicScanCb != nullptr)
			{
				m_pPeriodicScanCb->onCancelSync(param->period_adv_sync_cancel.status);
			}
			log_v("ESP_GAP_BLE_PERIODIC_ADV_SYNC_CANCEL_COMPLETE_EVT, status %d", param->period_adv_sync_cancel.status);
			break;
		case ESP_GAP_BLE_PERIODIC_ADV_SYNC_TERMINATE_COMPLETE_EVT:
			if (m_pPeriodicScanCb != nullptr)
			{
				m_pPeriodicScanCb->onTerminateSync(param->period_adv_sync_term.status);
			}
			log_v("ESP_GAP_BLE_PERIODIC_ADV_SYNC_TERMINATE_COMPLETE_EVT, status %d", param->period_adv_sync_term.status);
			break;
		case ESP_GAP_BLE_PERIODIC_ADV_SYNC_LOST_EVT:
			if (m_pPeriodicScanCb != nullptr)
			{
				m_pPeriodicScanCb->onLostSync(param->periodic_adv_sync_lost.sync_handle);
			}
			log_v("ESP_GAP_BLE_PERIODIC_ADV_SYNC_LOST_EVT, sync handle %d", param->periodic_adv_sync_lost.sync_handle);
			break;
		case ESP_GAP_BLE_PERIODIC_ADV_SYNC_ESTAB_EVT:
			if (m_pPeriodicScanCb != nullptr)
			{
				m_pPeriodicScanCb->onSync(*(esp_ble_periodic_adv_sync_estab_param_t*)&param->periodic_adv_sync_estab);
			}
			log_v("ESP_GAP_BLE_PERIODIC_ADV_SYNC_ESTAB_EVT, status %d", param->periodic_adv_sync_estab.status);
			break;

		case ESP_GAP_BLE_PERIODIC_ADV_REPORT_EVT:
			if (m_pPeriodicScanCb != nullptr)
			{
				m_pPeriodicScanCb->onReport(param->period_adv_report.params);
			}
			break;

#endif // CONFIG_BT_BLE_50_FEATURES_SUPPORTED

		default: {
			break;
		} // default
	} // End switch
} // gapEventHandler


/**
 * @brief Should we perform an active or passive scan?
 * The default is a passive scan.  An active scan means that we will wish a scan response.
 * @param [in] active If true, we perform an active scan otherwise a passive scan.
 * @return N/A.
 */
void BLEScan::setActiveScan(bool active) {
	if (active) {
		m_scan_params.scan_type = BLE_SCAN_TYPE_ACTIVE;
	} else {
		m_scan_params.scan_type = BLE_SCAN_TYPE_PASSIVE;
	}
} // setActiveScan


/**
 * @brief Set the call backs to be invoked.
 * @param [in] pAdvertisedDeviceCallbacks Call backs to be invoked.
 * @param [in] wantDuplicates  True if we wish to be called back with duplicates.  Default is false.
 * @param [in] shouldParse  True if we wish to parse advertised package or raw payload.  Default is true.
 */
void BLEScan::setAdvertisedDeviceCallbacks(BLEAdvertisedDeviceCallbacks* pAdvertisedDeviceCallbacks, bool wantDuplicates, bool shouldParse) {
	m_wantDuplicates = wantDuplicates;
	m_pAdvertisedDeviceCallbacks = pAdvertisedDeviceCallbacks;
	m_shouldParse = shouldParse;
} // setAdvertisedDeviceCallbacks

#ifdef CONFIG_BT_BLE_50_FEATURES_SUPPORTED

void BLEScan::setExtendedScanCallback(BLEExtAdvertisingCallbacks* cb)
{
	m_pExtendedScanCb = cb;
}

/**
* @brief           This function is used to set the extended scan parameters to be used on the advertising channels.
*
*
* @return            - ESP_OK : success
*                    - other  : failed
*
*/
esp_err_t BLEScan::setExtScanParams()
{
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
esp_err_t BLEScan::setExtScanParams(esp_ble_ext_scan_params_t* ext_scan_params)
{
	esp_err_t rc = esp_ble_gap_set_ext_scan_params(ext_scan_params);
	if (rc) {
		log_e("set extend scan params error, error code = %x", rc);
	}
	return rc; 
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
esp_err_t BLEScan::startExtScan(uint32_t duration, uint16_t period)
{
	esp_err_t rc = esp_ble_gap_start_ext_scan(duration, period);
	if(rc) log_e("extended scan start failed: %d", rc);
	return rc;
}


esp_err_t BLEScan::stopExtScan()
{
	esp_err_t rc;
	rc = esp_ble_gap_stop_ext_scan();

	return rc;
}

void BLEScan::setPeriodicScanCallback(BLEPeriodicScanCallbacks* cb)
{
	m_pPeriodicScanCb = cb;
}

#endif // CONFIG_BT_BLE_50_FEATURES_SUPPORTED
/**
 * @brief Set the interval to scan.
 * @param [in] The interval in msecs.
 */
void BLEScan::setInterval(uint16_t intervalMSecs) {
	m_scan_params.scan_interval = intervalMSecs / 0.625;
} // setInterval


/**
 * @brief Set the window to actively scan.
 * @param [in] windowMSecs How long to actively scan.
 */
void BLEScan::setWindow(uint16_t windowMSecs) {
	m_scan_params.scan_window = windowMSecs / 0.625;
} // setWindow


/**
 * @brief Start scanning.
 * @param [in] duration The duration in seconds for which to scan.
 * @param [in] scanCompleteCB A function to be called when scanning has completed.
 * @param [in] are we continue scan (true) or we want to clear stored devices (false)
 * @return True if scan started or false if there was an error.
 */
bool BLEScan::start(uint32_t duration, void (*scanCompleteCB)(BLEScanResults), bool is_continue) {
	log_v(">> start(duration=%d)", duration);

	m_semaphoreScanEnd.take(std::string("start"));
	m_scanCompleteCB = scanCompleteCB;                  // Save the callback to be invoked when the scan completes.

	//  if we are connecting to devices that are advertising even after being connected, multiconnecting peripherals
	//  then we should not clear map or we will connect the same device few times
	if(!is_continue) {  
		for(auto _dev : m_scanResults.m_vectorAdvertisedDevices){
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
} // start


/**
 * @brief Start scanning and block until scanning has been completed.
 * @param [in] duration The duration in seconds for which to scan.
 * @return The BLEScanResults.
 */
BLEScanResults BLEScan::start(uint32_t duration, bool is_continue) {
	if(start(duration, nullptr, is_continue)) {
		m_semaphoreScanEnd.wait("start");   // Wait for the semaphore to release.
	}
	return m_scanResults;
} // start


/**
 * @brief Stop an in progress scan.
 * @return N/A.
 */
void BLEScan::stop() {
	log_v(">> stop()");

	esp_err_t errRc = ::esp_ble_gap_stop_scanning();

	m_stopped = true;
	m_semaphoreScanEnd.give();

	if (errRc != ESP_OK) {
		log_e("esp_ble_gap_stop_scanning: err: %d, text: %s", errRc, GeneralUtils::errorToString(errRc));
		return;
	}

	log_v("<< stop()");
} // stop

// delete peer device from cache after disconnecting, it is required in case we are connecting to devices with not public address
void BLEScan::erase(BLEAddress address) {
	log_i("erase device: %s", address.toString().c_str());
	BLEAdvertisedDevice *advertisedDevice = m_scanResults.m_vectorAdvertisedDevices.find(address.toString())->second;
	m_scanResults.m_vectorAdvertisedDevices.erase(address.toString());
	delete advertisedDevice;
}


/**
 * @brief Dump the scan results to the log.
 */
void BLEScanResults::dump() {
	log_v(">> Dump scan results:");
	for (int i=0; i<getCount(); i++) {
		log_d("- %s", getDevice(i).toString().c_str());
	}
} // dump


/**
 * @brief Return the count of devices found in the last scan.
 * @return The number of devices found in the last scan.
 */
int BLEScanResults::getCount() {
	return m_vectorAdvertisedDevices.size();
} // getCount


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
		if (x==i)	break;
		x++;
	}
	return dev;
}

BLEScanResults BLEScan::getResults() {
	return m_scanResults;
}

void BLEScan::clearResults() {
	for(auto _dev : m_scanResults.m_vectorAdvertisedDevices){
		delete _dev.second;
	}
	m_scanResults.m_vectorAdvertisedDevices.clear();
}

#endif /* CONFIG_BLUEDROID_ENABLED */
