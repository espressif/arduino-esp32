/*
 * BLEAdvertising.h
 *
 *  Created on: Jun 21, 2017
 *      Author: kolban
 */

#ifndef COMPONENTS_CPP_UTILS_BLEADVERTISING_H_
#define COMPONENTS_CPP_UTILS_BLEADVERTISING_H_
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)
#include <esp_gap_ble_api.h>
#include "BLEUUID.h"
#include <vector>
#include "FreeRTOS.h"

/**
 * @brief Advertisement data set by the programmer to be published by the %BLE server.
 */
class BLEAdvertisementData {
	// Only a subset of the possible BLE architected advertisement fields are currently exposed.  Others will
	// be exposed on demand/request or as time permits.
	//
public:
	void setAppearance(uint16_t appearance);
	void setCompleteServices(BLEUUID uuid);
	void setFlags(uint8_t);
	void setManufacturerData(std::string data);
	void setName(std::string name);
	void setPartialServices(BLEUUID uuid);
	void setServiceData(BLEUUID uuid, std::string data);
	void setShortName(std::string name);
	void addData(std::string data);         // Add data to the payload.
	std::string getPayload();               // Retrieve the current advert payload.

private:
	friend class BLEAdvertising;
	std::string m_payload;   // The payload of the advertisement.
};   // BLEAdvertisementData


/**
 * @brief Perform and manage %BLE advertising.
 *
 * A %BLE server will want to perform advertising in order to make itself known to %BLE clients.
 */
class BLEAdvertising {
public:
	BLEAdvertising();
	void addServiceUUID(BLEUUID serviceUUID);
	void addServiceUUID(const char* serviceUUID);
	void start();
	void stop();
	void setAppearance(uint16_t appearance);
	void setAdvertisementType(esp_ble_adv_type_t adv_type);
	void setMaxInterval(uint16_t maxinterval);
	void setMinInterval(uint16_t mininterval);
	void setAdvertisementData(BLEAdvertisementData& advertisementData);
	void setScanFilter(bool scanRequertWhitelistOnly, bool connectWhitelistOnly);
	void setScanResponseData(BLEAdvertisementData& advertisementData);
	void setPrivateAddress(esp_ble_addr_type_t type = BLE_ADDR_TYPE_RANDOM);
	void setDeviceAddress(esp_bd_addr_t addr, esp_ble_addr_type_t type = BLE_ADDR_TYPE_RANDOM);

	void handleGAPEvent(esp_gap_ble_cb_event_t  event, esp_ble_gap_cb_param_t* param);
	void setMinPreferred(uint16_t);
	void setMaxPreferred(uint16_t);
	void setScanResponse(bool);

private:
	esp_ble_adv_data_t   m_advData;
	esp_ble_adv_data_t   m_scanRespData; // Used for configuration of scan response data when m_scanResp is true
	esp_ble_adv_params_t m_advParams;
	std::vector<BLEUUID> m_serviceUUIDs;
	bool                 m_customAdvData = false;  // Are we using custom advertising data?
	bool                 m_customScanResponseData = false;  // Are we using custom scan response data?
	FreeRTOS::Semaphore  m_semaphoreSetAdv = FreeRTOS::Semaphore("startAdvert");
	bool                 m_scanResp = true;

};
#endif /* CONFIG_BT_ENABLED */
#endif /* COMPONENTS_CPP_UTILS_BLEADVERTISING_H_ */
