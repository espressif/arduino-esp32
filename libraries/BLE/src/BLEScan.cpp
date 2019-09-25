/*
 * BLEScan.cpp
 *
 *  Created on: Jul 1, 2017
 *      Author: kolban
 */
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)


#include <esp_err.h>

#include <map>

#include "BLEAdvertisedDevice.h"
#include "BLEScan.h"
#include "BLEUtils.h"
#include "GeneralUtils.h"
#if defined(ARDUINO_ARCH_ESP32) && defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG ""
#else
#include "esp_log.h"
static const char* LOG_TAG = "BLEScan";
#endif




/**
 * Constructor
 */
BLEScan::BLEScan() {
	m_scan_params.scan_type          = BLE_SCAN_TYPE_PASSIVE; // Default is a passive scan.
	m_scan_params.own_addr_type      = BLE_ADDR_TYPE_PUBLIC;
	m_scan_params.scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL;
	m_pAdvertisedDeviceCallbacks     = nullptr;
	m_stopped                        = true;
	m_wantDuplicates                 = false;
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
					ESP_LOGW(LOG_TAG, "ESP_GAP_SEARCH_INQ_CMPL_EVT");
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

					if (m_scanResults.m_vectorAdvertisedDevices.count(advertisedAddress.toString()) != 0) {
						found = true;
					}

					if (found && !m_wantDuplicates) {  // If we found a previous entry AND we don't want duplicates, then we are done.
						ESP_LOGD(LOG_TAG, "Ignoring %s, already seen it.", advertisedAddress.toString().c_str());
						vTaskDelay(1);  // <--- allow to switch task in case we scan infinity and dont have new devices to report, or we are blocked here
						break;
					}

					// We now construct a model of the advertised device that we have just found for the first
					// time.
					// ESP_LOG_BUFFER_HEXDUMP(LOG_TAG, (uint8_t*)param->scan_rst.ble_adv, param->scan_rst.adv_data_len + param->scan_rst.scan_rsp_len, ESP_LOG_DEBUG);
					// ESP_LOGW(LOG_TAG, "bytes length: %d + %d, addr type: %d", param->scan_rst.adv_data_len, param->scan_rst.scan_rsp_len, param->scan_rst.ble_addr_type);
					BLEAdvertisedDevice *advertisedDevice = new BLEAdvertisedDevice();
					advertisedDevice->setAddress(advertisedAddress);
					advertisedDevice->setRSSI(param->scan_rst.rssi);
					advertisedDevice->setAdFlag(param->scan_rst.flag);
					advertisedDevice->parseAdvertisement((uint8_t*)param->scan_rst.ble_adv, param->scan_rst.adv_data_len + param->scan_rst.scan_rsp_len);
					advertisedDevice->setScan(this);
					advertisedDevice->setAddressType(param->scan_rst.ble_addr_type);

					if (!found) {   // If we have previously seen this device, don't record it again.
						m_scanResults.m_vectorAdvertisedDevices.insert(std::pair<std::string, BLEAdvertisedDevice*>(advertisedAddress.toString(), advertisedDevice));
					}

					if (m_pAdvertisedDeviceCallbacks) {
						m_pAdvertisedDeviceCallbacks->onResult(*advertisedDevice);
					}
					if(found)
						delete advertisedDevice;

					break;
				} // ESP_GAP_SEARCH_INQ_RES_EVT

				default: {
					break;
				}
			} // switch - search_evt


			break;
		} // ESP_GAP_BLE_SCAN_RESULT_EVT

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
 */
void BLEScan::setAdvertisedDeviceCallbacks(BLEAdvertisedDeviceCallbacks* pAdvertisedDeviceCallbacks, bool wantDuplicates) {
	m_wantDuplicates = wantDuplicates;
	m_pAdvertisedDeviceCallbacks = pAdvertisedDeviceCallbacks;
} // setAdvertisedDeviceCallbacks


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
	ESP_LOGD(LOG_TAG, ">> start(duration=%d)", duration);

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
		ESP_LOGE(LOG_TAG, "esp_ble_gap_set_scan_params: err: %d, text: %s", errRc, GeneralUtils::errorToString(errRc));
		m_semaphoreScanEnd.give();
		return false;
	}

	errRc = ::esp_ble_gap_start_scanning(duration);

	if (errRc != ESP_OK) {
		ESP_LOGE(LOG_TAG, "esp_ble_gap_start_scanning: err: %d, text: %s", errRc, GeneralUtils::errorToString(errRc));
		m_semaphoreScanEnd.give();
		return false;
	}

	m_stopped = false;

	ESP_LOGD(LOG_TAG, "<< start()");
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
	ESP_LOGD(LOG_TAG, ">> stop()");

	esp_err_t errRc = ::esp_ble_gap_stop_scanning();

	m_stopped = true;
	m_semaphoreScanEnd.give();

	if (errRc != ESP_OK) {
		ESP_LOGE(LOG_TAG, "esp_ble_gap_stop_scanning: err: %d, text: %s", errRc, GeneralUtils::errorToString(errRc));
		return;
	}

	ESP_LOGD(LOG_TAG, "<< stop()");
} // stop

// delete peer device from cache after disconnecting, it is required in case we are connecting to devices with not public address
void BLEScan::erase(BLEAddress address) {
	ESP_LOGI(LOG_TAG, "erase device: %s", address.toString().c_str());
	BLEAdvertisedDevice *advertisedDevice = m_scanResults.m_vectorAdvertisedDevices.find(address.toString())->second;
	m_scanResults.m_vectorAdvertisedDevices.erase(address.toString());
	delete advertisedDevice;
}


/**
 * @brief Dump the scan results to the log.
 */
void BLEScanResults::dump() {
	ESP_LOGD(LOG_TAG, ">> Dump scan results:");
	for (int i=0; i<getCount(); i++) {
		ESP_LOGD(LOG_TAG, "- %s", getDevice(i).toString().c_str());
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

#endif /* CONFIG_BT_ENABLED */
