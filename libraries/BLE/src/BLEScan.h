/*
 * BLEScan.h
 *
 *  Created on: Jul 1, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_BLESCAN_H_
#define COMPONENTS_CPP_UTILS_BLESCAN_H_
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)
#include <esp_gap_ble_api.h>

// #include <vector>
#include <string>
#include "BLEAdvertisedDevice.h"
#include "BLEClient.h"
#include "FreeRTOS.h"

class BLEAdvertisedDevice;
class BLEAdvertisedDeviceCallbacks;
class BLEClient;
class BLEScan;


/**
 * @brief The result of having performed a scan.
 * When a scan completes, we have a set of found devices.  Each device is described
 * by a BLEAdvertisedDevice object.  The number of items in the set is given by
 * getCount().  We can retrieve a device by calling getDevice() passing in the
 * index (starting at 0) of the desired device.
 */
class BLEScanResults {
public:
	void                dump();
	int                 getCount();
	BLEAdvertisedDevice getDevice(uint32_t i);

private:
	friend BLEScan;
	std::map<std::string, BLEAdvertisedDevice*> m_vectorAdvertisedDevices;
};

/**
 * @brief Perform and manage %BLE scans.
 *
 * Scanning is associated with a %BLE client that is attempting to locate BLE servers.
 */
class BLEScan {
public:
	void           setActiveScan(bool active);
	void           setAdvertisedDeviceCallbacks(
			              BLEAdvertisedDeviceCallbacks* pAdvertisedDeviceCallbacks,
										bool wantDuplicates = false,
										bool shouldParse = true);
	void           setInterval(uint16_t intervalMSecs);
	void           setWindow(uint16_t windowMSecs);
	bool           start(uint32_t duration, void (*scanCompleteCB)(BLEScanResults), bool is_continue = false);
	BLEScanResults start(uint32_t duration, bool is_continue = false);
	void           stop();
	void 		   erase(BLEAddress address);
	BLEScanResults getResults();
	void			clearResults();

private:
	BLEScan();   // One doesn't create a new instance instead one asks the BLEDevice for the singleton.
	friend class BLEDevice;
	void         handleGAPEvent(
		esp_gap_ble_cb_event_t  event,
		esp_ble_gap_cb_param_t* param);
	void parseAdvertisement(BLEClient* pRemoteDevice, uint8_t *payload);


	esp_ble_scan_params_t         m_scan_params;
	BLEAdvertisedDeviceCallbacks* m_pAdvertisedDeviceCallbacks = nullptr;
	bool                          m_stopped = true;
	bool                          m_shouldParse = true;
	FreeRTOS::Semaphore           m_semaphoreScanEnd = FreeRTOS::Semaphore("ScanEnd");
	BLEScanResults                m_scanResults;
	bool                          m_wantDuplicates;
	void                        (*m_scanCompleteCB)(BLEScanResults scanResults);
}; // BLEScan

#endif /* CONFIG_BT_ENABLED */
#endif /* COMPONENTS_CPP_UTILS_BLESCAN_H_ */
