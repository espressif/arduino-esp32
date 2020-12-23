/*
 * BLEAdvertisedDevice.h
 *
 *  Created on: Jul 3, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_BLEADVERTISEDDEVICE_H_
#define COMPONENTS_CPP_UTILS_BLEADVERTISEDDEVICE_H_
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)
#include <esp_gattc_api.h>

#include <map>

#include "BLEAddress.h"
#include "BLEScan.h"
#include "BLEUUID.h"


class BLEScan;
/**
 * @brief A representation of a %BLE advertised device found by a scan.
 *
 * When we perform a %BLE scan, the result will be a set of devices that are advertising.  This
 * class provides a model of a detected device.
 */
class BLEAdvertisedDevice {
public:
	BLEAdvertisedDevice();

	BLEAddress  getAddress();
	uint16_t    getAppearance();
	std::string getManufacturerData();
	std::string getName();
	int         getRSSI();
	BLEScan*    getScan();
	std::string getServiceData();
	std::string getServiceData(int i);
	BLEUUID     getServiceDataUUID();
	BLEUUID     getServiceDataUUID(int i);
	BLEUUID     getServiceUUID();
	BLEUUID     getServiceUUID(int i);
	int         getServiceDataCount();
	int8_t      getTXPower();
	uint8_t* 	getPayload();
	size_t		getPayloadLength();
	esp_ble_addr_type_t getAddressType();
	void setAddressType(esp_ble_addr_type_t type);


	bool		isAdvertisingService(BLEUUID uuid);
	bool        haveAppearance();
	bool        haveManufacturerData();
	bool        haveName();
	bool        haveRSSI();
	bool        haveServiceData();
	bool        haveServiceUUID();
	bool        haveTXPower();

	std::string toString();

private:
	friend class BLEScan;

	void parseAdvertisement(uint8_t* payload, size_t total_len=62);
	void setPayload(uint8_t* payload, size_t total_len=62);
	void setAddress(BLEAddress address);
	void setAdFlag(uint8_t adFlag);
	void setAdvertizementResult(uint8_t* payload);
	void setAppearance(uint16_t appearance);
	void setManufacturerData(std::string manufacturerData);
	void setName(std::string name);
	void setRSSI(int rssi);
	void setScan(BLEScan* pScan);
	void setServiceData(std::string data);
	void setServiceDataUUID(BLEUUID uuid);
	void setServiceUUID(const char* serviceUUID);
	void setServiceUUID(BLEUUID serviceUUID);
	void setTXPower(int8_t txPower);

	bool m_haveAppearance;
	bool m_haveManufacturerData;
	bool m_haveName;
	bool m_haveRSSI;
	bool m_haveServiceData;
	bool m_haveServiceUUID;
	bool m_haveTXPower;


	BLEAddress  m_address = BLEAddress((uint8_t*)"\0\0\0\0\0\0");
	uint8_t     m_adFlag;
	uint16_t    m_appearance;
	int         m_deviceType;
	std::string m_manufacturerData;
	std::string m_name;
	BLEScan*    m_pScan;
	int         m_rssi;
	std::vector<BLEUUID> m_serviceUUIDs;
	int8_t      m_txPower;
	std::vector<std::string> m_serviceData;
	std::vector<BLEUUID> m_serviceDataUUIDs;
	uint8_t*	m_payload;
	size_t		m_payloadLength = 0;
	esp_ble_addr_type_t m_addressType;
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
	virtual ~BLEAdvertisedDeviceCallbacks() {}
	/**
	 * @brief Called when a new scan result is detected.
	 *
	 * As we are scanning, we will find new devices.  When found, this call back is invoked with a reference to the
	 * device that was found.  During any individual scan, a device will only be detected one time.
	 */
	virtual void onResult(BLEAdvertisedDevice advertisedDevice) = 0;
};

#endif /* CONFIG_BT_ENABLED */
#endif /* COMPONENTS_CPP_UTILS_BLEADVERTISEDDEVICE_H_ */
