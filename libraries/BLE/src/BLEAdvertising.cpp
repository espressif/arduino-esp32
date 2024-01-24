/*
 * BLEAdvertising.cpp
 *
 * This class encapsulates advertising a BLE Server.
 *  Created on: Jun 21, 2017
 *      Author: kolban
 *
 * The ESP-IDF provides a framework for BLE advertising.  It has determined that there are a common set
 * of properties that are advertised and has built a data structure that can be populated by the programmer.
 * This means that the programmer doesn't have to "mess with" the low level construction of a low level
 * BLE advertising frame.  Many of the fields are determined for us while others we can set before starting
 * to advertise.
 *
 * Should we wish to construct our own payload, we can use the BLEAdvertisementData class and call the setters
 * upon it.  Once it is populated, we can then associate it with the advertising and what ever the programmer
 * set in the data will be advertised.
 *
 */
#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)
#include "BLEAdvertising.h"
#include <esp_err.h>
#include "BLEUtils.h"
#include "GeneralUtils.h"
#include "esp32-hal-log.h"

/**
 * @brief Construct a default advertising object.
 *
 */
BLEAdvertising::BLEAdvertising()
: m_scanRespData{}
{
	m_advData.set_scan_rsp        = false;
	m_advData.include_name        = true;
	m_advData.include_txpower     = true;
	m_advData.min_interval        = 0x20;
	m_advData.max_interval        = 0x40;
	m_advData.appearance          = 0x00;
	m_advData.manufacturer_len    = 0;
	m_advData.p_manufacturer_data = nullptr;
	m_advData.service_data_len    = 0;
	m_advData.p_service_data      = nullptr;
	m_advData.service_uuid_len    = 0;
	m_advData.p_service_uuid      = nullptr;
	m_advData.flag                = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);

	m_advParams.adv_int_min       = 0x20;
	m_advParams.adv_int_max       = 0x40;
	m_advParams.adv_type          = ADV_TYPE_IND;
	m_advParams.own_addr_type     = BLE_ADDR_TYPE_PUBLIC;
	m_advParams.channel_map       = ADV_CHNL_ALL;
	m_advParams.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
	m_advParams.peer_addr_type    = BLE_ADDR_TYPE_PUBLIC;

	m_customAdvData               = false;   // No custom advertising data
	m_customScanResponseData      = false;   // No custom scan response data
} // BLEAdvertising


/**
 * @brief Add a service uuid to exposed list of services.
 * @param [in] serviceUUID The UUID of the service to expose.
 */
void BLEAdvertising::addServiceUUID(BLEUUID serviceUUID) {
	m_serviceUUIDs.push_back(serviceUUID);
} // addServiceUUID


/**
 * @brief Add a service uuid to exposed list of services.
 * @param [in] serviceUUID The string representation of the service to expose.
 */
void BLEAdvertising::addServiceUUID(const char* serviceUUID) {
	addServiceUUID(BLEUUID(serviceUUID));
} // addServiceUUID


/**
 * @brief Remove a service uuid to exposed list of services.
 * @param [in] index The index of the service to stop exposing.
 */
bool BLEAdvertising::removeServiceUUID(int index) {
	
	// If index is larger than the size of the
	// advertised services, return false
	if(index > m_serviceUUIDs.size()) return false;
	
	m_serviceUUIDs.erase(m_serviceUUIDs.begin() + index);
	return true;
}
	
/**
 * @brief Remove a service uuid to exposed list of services.
 * @param [in] serviceUUID The BLEUUID of the service to stop exposing.
 */
bool BLEAdvertising::removeServiceUUID(BLEUUID serviceUUID) {
	for(int i = 0; i < m_serviceUUIDs.size(); i++) {
		if(m_serviceUUIDs.at(i).equals(serviceUUID)) {
			return removeServiceUUID(i);
		}
	}
	return false;
}
	
/**
 * @brief Remove a service uuid to exposed list of services.
 * @param [in] serviceUUID The string of the service to stop exposing.
 */
bool BLEAdvertising::removeServiceUUID(const char* serviceUUID) {
	return removeServiceUUID(BLEUUID(serviceUUID));
}

/**
 * @brief Set the device appearance in the advertising data.
 * The appearance attribute is of type 0x19.  The codes for distinct appearances can be found here:
 * https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.gap.appearance.xml.
 * @param [in] appearance The appearance of the device in the advertising data.
 * @return N/A.
 */
void BLEAdvertising::setAppearance(uint16_t appearance) {
	m_advData.appearance = appearance;
} // setAppearance

void BLEAdvertising::setAdvertisementType(esp_ble_adv_type_t adv_type){
	m_advParams.adv_type = adv_type;
} // setAdvertisementType

void BLEAdvertising::setAdvertisementChannelMap(esp_ble_adv_channel_t channel_map) {
	m_advParams.channel_map = channel_map;
} // setAdvertisementChannelMap

void BLEAdvertising::setMinInterval(uint16_t mininterval) {
	m_advParams.adv_int_min = mininterval;
} // setMinInterval

void BLEAdvertising::setMaxInterval(uint16_t maxinterval) {
	m_advParams.adv_int_max = maxinterval;
} // setMaxInterval

void BLEAdvertising::setMinPreferred(uint16_t mininterval) {
	m_advData.min_interval = mininterval;
} // 

void BLEAdvertising::setMaxPreferred(uint16_t maxinterval) {
	m_advData.max_interval = maxinterval;
} // 

void BLEAdvertising::setScanResponse(bool set) {
	m_scanResp = set;
}

/**
 * @brief Set the filtering for the scan filter.
 * @param [in] scanRequestWhitelistOnly If true, only allow scan requests from those on the white list.
 * @param [in] connectWhitelistOnly If true, only allow connections from those on the white list.
 */
void BLEAdvertising::setScanFilter(bool scanRequestWhitelistOnly, bool connectWhitelistOnly) {
	log_v(">> setScanFilter: scanRequestWhitelistOnly: %d, connectWhitelistOnly: %d", scanRequestWhitelistOnly, connectWhitelistOnly);
	if (!scanRequestWhitelistOnly && !connectWhitelistOnly) {
		m_advParams.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
		log_v("<< setScanFilter");
		return;
	}
	if (scanRequestWhitelistOnly && !connectWhitelistOnly) {
		m_advParams.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_WLST_CON_ANY;
		log_v("<< setScanFilter");
		return;
	}
	if (!scanRequestWhitelistOnly && connectWhitelistOnly) {
		m_advParams.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_WLST;
		log_v("<< setScanFilter");
		return;
	}
	if (scanRequestWhitelistOnly && connectWhitelistOnly) {
		m_advParams.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST;
		log_v("<< setScanFilter");
		return;
	}
} // setScanFilter


/**
 * @brief Set the advertisement data that is to be published in a regular advertisement.
 * @param [in] advertisementData The data to be advertised.
 */
void BLEAdvertising::setAdvertisementData(BLEAdvertisementData& advertisementData) {
	log_v(">> setAdvertisementData");
	esp_err_t errRc = ::esp_ble_gap_config_adv_data_raw(
		(uint8_t*)advertisementData.getPayload().c_str(),
		advertisementData.getPayload().length());
	if (errRc != ESP_OK) {
		log_e("esp_ble_gap_config_adv_data_raw: %d %s", errRc, GeneralUtils::errorToString(errRc));
	}
	m_customAdvData = true;   // Set the flag that indicates we are using custom advertising data.
	log_v("<< setAdvertisementData");
} // setAdvertisementData


/**
 * @brief Set the advertisement data that is to be published in a scan response.
 * @param [in] advertisementData The data to be advertised.
 */
void BLEAdvertising::setScanResponseData(BLEAdvertisementData& advertisementData) {
	log_v(">> setScanResponseData");
	esp_err_t errRc = ::esp_ble_gap_config_scan_rsp_data_raw(
		(uint8_t*)advertisementData.getPayload().c_str(),
		advertisementData.getPayload().length());
	if (errRc != ESP_OK) {
		log_e("esp_ble_gap_config_scan_rsp_data_raw: %d %s", errRc, GeneralUtils::errorToString(errRc));
	}
	m_customScanResponseData = true;   // Set the flag that indicates we are using custom scan response data.
	log_v("<< setScanResponseData");
} // setScanResponseData

/**
 * @brief Start advertising.
 * Start advertising.
 * @return N/A.
 */
void BLEAdvertising::start() {
	log_v(">> start: customAdvData: %d, customScanResponseData: %d", m_customAdvData, m_customScanResponseData);

	// We have a vector of service UUIDs that we wish to advertise.  In order to use the
	// ESP-IDF framework, these must be supplied in a contiguous array of their 128bit (16 byte)
	// representations.  If we have 1 or more services to advertise then we allocate enough
	// storage to host them and then copy them in one at a time into the contiguous storage.
	int numServices = m_serviceUUIDs.size();
	if (numServices > 0) {
		m_advData.service_uuid_len = 16 * numServices;
		m_advData.p_service_uuid = (uint8_t *)malloc(m_advData.service_uuid_len);
		if(!m_advData.p_service_uuid) {
			log_e(">> start failed: out of memory");
			return;
		}

		uint8_t* p = m_advData.p_service_uuid;
		for (int i = 0; i < numServices; i++) {
			log_d("- advertising service: %s", m_serviceUUIDs[i].toString().c_str());
			BLEUUID serviceUUID128 = m_serviceUUIDs[i].to128();
			memcpy(p, serviceUUID128.getNative()->uuid.uuid128, 16);
			p += 16;
		}
	} else {
		m_advData.service_uuid_len = 0;
		log_d("- no services advertised");
	}

	esp_err_t errRc;

	if (!m_customAdvData) {
		// Set the configuration for advertising.
		m_advData.set_scan_rsp = false;
		m_advData.include_name = !m_scanResp;
		m_advData.include_txpower = !m_scanResp;
		errRc = ::esp_ble_gap_config_adv_data(&m_advData);
		if (errRc != ESP_OK) {
			log_e("<< esp_ble_gap_config_adv_data: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
			return;
		}
	}

	if (!m_customScanResponseData && m_scanResp) {
		// Set the configuration for scan response.
		memcpy(&m_scanRespData, &m_advData, sizeof(esp_ble_adv_data_t)); // Copy the content of m_advData.
		m_scanRespData.set_scan_rsp = true; // Define this struct as scan response data
		m_scanRespData.include_name = true; // Caution: This may lead to a crash if the device name has more than 29 characters
		m_scanRespData.include_txpower = true;
		m_scanRespData.appearance = 0; // If defined the 'Appearance' attribute is already included in the advertising data
		m_scanRespData.flag = 0; // 'Flags' attribute should no be included in the scan response

		errRc = ::esp_ble_gap_config_adv_data(&m_scanRespData);
		if (errRc != ESP_OK) {
			log_e("<< esp_ble_gap_config_adv_data (Scan response): rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
			return;
		}
	}

	// If we had services to advertise then we previously allocated some storage for them.
	// Here we release that storage.
	free(m_advData.p_service_uuid); //TODO change this variable to local scope?
	m_advData.p_service_uuid = nullptr;

	// Start advertising.
	errRc = ::esp_ble_gap_start_advertising(&m_advParams);
	if (errRc != ESP_OK) {
		log_e("<< esp_ble_gap_start_advertising: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
		return;
	}
	log_v("<< start");
} // start


/**
 * @brief Stop advertising.
 * Stop advertising.
 * @return N/A.
 */
void BLEAdvertising::stop() {
	log_v(">> stop");
	esp_err_t errRc = ::esp_ble_gap_stop_advertising();
	if (errRc != ESP_OK) {
		log_e("esp_ble_gap_stop_advertising: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
		return;
	}
	log_v("<< stop");
} // stop

/**
 * @brief Set BLE address.
 * @param [in] Bluetooth address.
 * @param [in] Bluetooth address type.
 * Set BLE address.
 */

void BLEAdvertising::setDeviceAddress(esp_bd_addr_t addr, esp_ble_addr_type_t type)
{
	log_v(">> setPrivateAddress");

	m_advParams.own_addr_type = type;
	esp_err_t errRc = esp_ble_gap_set_rand_addr((uint8_t*)addr);
	if (errRc != ESP_OK)
	{
		log_e("esp_ble_gap_set_rand_addr: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
		return;
	}
	log_v("<< setPrivateAddress");
} // setPrivateAddress

/**
 * @brief Add data to the payload to be advertised.
 * @param [in] data The data to be added to the payload.
 */
void BLEAdvertisementData::addData(String data) {
	if ((m_payload.length() + data.length()) > ESP_BLE_ADV_DATA_LEN_MAX) {
		return;
	}
	m_payload.concat(data);
} // addData


/**
 * @brief Set the appearance.
 * @param [in] appearance The appearance code value.
 *
 * See also:
 * https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.characteristic.gap.appearance.xml
 */
void BLEAdvertisementData::setAppearance(uint16_t appearance) {
	char cdata[2];
	cdata[0] = 3;
	cdata[1] = ESP_BLE_AD_TYPE_APPEARANCE; // 0x19
	addData(String(cdata, 2) + String((char*) &appearance, 2));
} // setAppearance


/**
 * @brief Set the complete services.
 * @param [in] uuid The single service to advertise.
 */
void BLEAdvertisementData::setCompleteServices(BLEUUID uuid) {
	char cdata[2];
	switch (uuid.bitSize()) {
		case 16: {
			// [Len] [0x02] [LL] [HH]
			cdata[0] = 3;
			cdata[1] = ESP_BLE_AD_TYPE_16SRV_CMPL;  // 0x03
			addData(String(cdata, 2) + String((char*) &uuid.getNative()->uuid.uuid16, 2));
			break;
		}

		case 32: {
			// [Len] [0x04] [LL] [LL] [HH] [HH]
			cdata[0] = 5;
			cdata[1] = ESP_BLE_AD_TYPE_32SRV_CMPL;  // 0x05
			addData(String(cdata, 2) + String((char*) &uuid.getNative()->uuid.uuid32, 4));
			break;
		}

		case 128: {
			// [Len] [0x04] [0] [1] ... [15]
			cdata[0] = 17;
			cdata[1] = ESP_BLE_AD_TYPE_128SRV_CMPL;  // 0x07
			addData(String(cdata, 2) + String((char*) uuid.getNative()->uuid.uuid128, 16));
			break;
		}

		default:
			return;
	}
} // setCompleteServices


/**
 * @brief Set the advertisement flags.
 * @param [in] The flags to be set in the advertisement.
 *
 * * ESP_BLE_ADV_FLAG_LIMIT_DISC
 * * ESP_BLE_ADV_FLAG_GEN_DISC
 * * ESP_BLE_ADV_FLAG_BREDR_NOT_SPT
 * * ESP_BLE_ADV_FLAG_DMT_CONTROLLER_SPT
 * * ESP_BLE_ADV_FLAG_DMT_HOST_SPT
 * * ESP_BLE_ADV_FLAG_NON_LIMIT_DISC
 */
void BLEAdvertisementData::setFlags(uint8_t flag) {
	char cdata[3];
	cdata[0] = 2;
	cdata[1] = ESP_BLE_AD_TYPE_FLAG;  // 0x01
	cdata[2] = flag;
	addData(String(cdata, 3));
} // setFlag



/**
 * @brief Set manufacturer specific data.
 * @param [in] data Manufacturer data.
 */
void BLEAdvertisementData::setManufacturerData(String data) {
	log_d("BLEAdvertisementData", ">> setManufacturerData");
	char cdata[2];
	cdata[0] = data.length() + 1;
	cdata[1] = ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE;  // 0xff
	addData(String(cdata, 2) + data);
	log_d("BLEAdvertisementData", "<< setManufacturerData");
} // setManufacturerData


/**
 * @brief Set the name.
 * @param [in] The complete name of the device.
 */
void BLEAdvertisementData::setName(String name) {
	log_d("BLEAdvertisementData", ">> setName: %s", name.c_str());
	char cdata[2];
	cdata[0] = name.length() + 1;
	cdata[1] = ESP_BLE_AD_TYPE_NAME_CMPL;  // 0x09
	addData(String(cdata, 2) + name);
	log_d("BLEAdvertisementData", "<< setName");
} // setName


/**
 * @brief Set the partial services.
 * @param [in] uuid The single service to advertise.
 */
void BLEAdvertisementData::setPartialServices(BLEUUID uuid) {
	char cdata[2];
	switch (uuid.bitSize()) {
		case 16: {
			// [Len] [0x02] [LL] [HH]
			cdata[0] = 3;
			cdata[1] = ESP_BLE_AD_TYPE_16SRV_PART;  // 0x02
			addData(String(cdata, 2) + String((char *) &uuid.getNative()->uuid.uuid16, 2));
			break;
		}

		case 32: {
			// [Len] [0x04] [LL] [LL] [HH] [HH]
			cdata[0] = 5;
			cdata[1] = ESP_BLE_AD_TYPE_32SRV_PART; // 0x04
			addData(String(cdata, 2) + String((char *) &uuid.getNative()->uuid.uuid32, 4));
			break;
		}

		case 128: {
			// [Len] [0x04] [0] [1] ... [15]
			cdata[0] = 17;
			cdata[1] = ESP_BLE_AD_TYPE_128SRV_PART;  // 0x06
			addData(String(cdata, 2) + String((char *) &uuid.getNative()->uuid.uuid128, 16));
			break;
		}

		default:
			return;
	}
} // setPartialServices


/**
 * @brief Set the service data (UUID + data)
 * @param [in] uuid The UUID to set with the service data.  Size of UUID will be used.
 * @param [in] data The data to be associated with the service data advert.
 */
void BLEAdvertisementData::setServiceData(BLEUUID uuid, String data) {
	char cdata[2];
	switch (uuid.bitSize()) {
		case 16: {
			// [Len] [0x16] [UUID16] data
			cdata[0] = data.length() + 3;
			cdata[1] = ESP_BLE_AD_TYPE_SERVICE_DATA;  // 0x16
			addData(String(cdata, 2) + String((char*) &uuid.getNative()->uuid.uuid16, 2) + data);
			break;
		}

		case 32: {
			// [Len] [0x20] [UUID32] data
			cdata[0] = data.length() + 5;
			cdata[1] = ESP_BLE_AD_TYPE_32SERVICE_DATA; // 0x20
			addData(String(cdata, 2) + String((char*) &uuid.getNative()->uuid.uuid32, 4) + data);
			break;
		}

		case 128: {
			// [Len] [0x21] [UUID128] data
			cdata[0] = data.length() + 17;
			cdata[1] = ESP_BLE_AD_TYPE_128SERVICE_DATA;  // 0x21
			addData(String(cdata, 2) + String((char*) &uuid.getNative()->uuid.uuid128, 16) + data);
			break;
		}

		default:
			return;
	}
} // setServiceData


/**
 * @brief Set the short name.
 * @param [in] The short name of the device.
 */
void BLEAdvertisementData::setShortName(String name) {
	log_d("BLEAdvertisementData", ">> setShortName: %s", name.c_str());
	char cdata[2];
	cdata[0] = name.length() + 1;
	cdata[1] = ESP_BLE_AD_TYPE_NAME_SHORT;  // 0x08
	addData(String(cdata, 2) + name);
	log_d("BLEAdvertisementData", "<< setShortName");
} // setShortName


/**
 * @brief Retrieve the payload that is to be advertised.
 * @return The payload that is to be advertised.
 */
String BLEAdvertisementData::getPayload() {
	return m_payload;
} // getPayload

void BLEAdvertising::handleGAPEvent(
		esp_gap_ble_cb_event_t  event,
		esp_ble_gap_cb_param_t* param)  {

	log_d("handleGAPEvent [event no: %d]", (int)event);

	switch(event) {
		case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: {
			// m_semaphoreSetAdv.give();
			break;
		}
		case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT: {
			// m_semaphoreSetAdv.give();
			break;
		}
		case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: {
			// m_semaphoreSetAdv.give();
			break;
		}
		case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: {
			log_i("STOP advertising");
			//start();
			break;
		}
		default:
			break;
	}
}

#ifdef SOC_BLE_50_SUPPORTED

/**
* @brief           Creator
*
* @param[in]       instance : number of multi advertising instances
*
*
*/
BLEMultiAdvertising::BLEMultiAdvertising(uint8_t num)
{
	params_arrays = (esp_ble_gap_ext_adv_params_t*)calloc(num, sizeof(esp_ble_gap_ext_adv_params_t));
	ext_adv = (esp_ble_gap_ext_adv_t*)calloc(num, sizeof(esp_ble_gap_ext_adv_t));
	count = num;
}

/**
* @brief           This function is used by the Host to set the advertising parameters.
*
* @param[in]       instance : identifies the advertising set whose parameters are being configured.
* @param[in]       params   : advertising parameters
*
* @return            - true : success
*                    - false  : failed
*
*/
bool BLEMultiAdvertising::setAdvertisingParams(uint8_t instance, const esp_ble_gap_ext_adv_params_t* params)
{
	if (params->type == ESP_BLE_GAP_SET_EXT_ADV_PROP_LEGACY_IND && params->primary_phy == ESP_BLE_GAP_PHY_2M) return false;
	esp_err_t rc;
	rc = esp_ble_gap_ext_adv_set_params(instance, params);

	return ESP_OK == rc;
}

/**
* @brief           This function is used to set the data used in advertising PDUs that have a data field
*
* @param[in]       instance : identifies the advertising set whose data are being configured
* @param[in]       length   : data length
* @param[in]       data     : data information
*
* @return            - true : success
*                    - false  : failed
*
*/
bool BLEMultiAdvertising::setAdvertisingData(uint8_t instance, uint16_t length, const uint8_t* data)
{
	esp_err_t rc;
	rc = esp_ble_gap_config_ext_adv_data_raw(instance, length, data);
	if (rc) log_e("set advertising data err: %d", rc);

	return ESP_OK == rc;
}

bool BLEMultiAdvertising::setScanRspData(uint8_t instance, uint16_t length, const uint8_t* data)
{
	esp_err_t rc;
	rc = esp_ble_gap_config_ext_scan_rsp_data_raw(instance, length, data);
	if (rc) log_e("set scan resp data err: %d", rc);

	return ESP_OK == rc;
}

/**
* @brief           This function is used to request the Controller to enable one or more
*                  advertising sets using the advertising sets identified by the instance parameter.
*
* @return            - true : success
*                    - false  : failed
*
*/
bool BLEMultiAdvertising::start()
{
	return start(count, 0);
}

/**
* @brief           This function is used to request the Controller to enable one or more
*                  advertising sets using the advertising sets identified by the instance parameter.
*
* @param[in]       num : Number of advertising sets to enable or disable
* @param[in]       from : first sxt adv set to use
*
* @return            - true : success
*                    - false  : failed
*
*/
bool BLEMultiAdvertising::start(uint8_t num, uint8_t from)
{
	if (num > count || from >= count) return false;

	esp_err_t rc;
	rc = esp_ble_gap_ext_adv_start(num, &ext_adv[from]);
	if (rc) log_e("start extended advertising err: %d", rc);

	return ESP_OK == rc;
}

/**
* @brief           This function is used to request the Controller to disable one or more
*                  advertising sets using the advertising sets identified by the instance parameter.
*
* @param[in]       num_adv : Number of advertising sets to enable or disable
* @param[in]       ext_adv_inst : ext adv instance
*
* @return            - ESP_OK : success
*                    - other  : failed
*
*/
bool BLEMultiAdvertising::stop(uint8_t num_adv, const uint8_t* ext_adv_inst)
{
	esp_err_t rc;
	rc = esp_ble_gap_ext_adv_stop(num_adv, ext_adv_inst);
	if (rc) log_e("stop extended advertising err: %d", rc);

	return ESP_OK == rc;
}

/**
* @brief           This function is used to remove an advertising set from the Controller.
*
* @param[in]       instance : Used to identify an advertising set
*
* @return            - ESP_OK : success
*                    - other  : failed
*
*/
bool BLEMultiAdvertising::remove(uint8_t instance)
{
	esp_err_t rc;
	rc = esp_ble_gap_ext_adv_set_remove(instance);
	if (rc) log_e("remove extended advertising err: %d", rc);

	return ESP_OK == rc;
}

/**
* @brief           This function is used to remove all existing advertising sets from the Controller.
*
*
* @return            - ESP_OK : success
*                    - other  : failed
*
*/
bool BLEMultiAdvertising::clear()
{
	esp_err_t rc;
	rc = esp_ble_gap_ext_adv_set_clear();
	if (rc) log_e("clear extended advertising err: %d", rc);

	return ESP_OK == rc;
}

/**
* @brief           This function is used by the Host to set the random device address specified by the Random_Address parameter.
*
* @param[in]       instance  : Used to identify an advertising set
* @param[in]       addr_legacy : Random Device Address
*
* @return            - true : success
*                    - false  : failed
*
*/
bool BLEMultiAdvertising::setInstanceAddress(uint8_t instance, uint8_t* addr_legacy)
{
	esp_err_t rc;
	rc = esp_ble_gap_ext_adv_set_rand_addr(instance, addr_legacy);
	if (rc) log_e("set random address err: %d", rc);

	return ESP_OK == rc;
}

/**
* @brief           This function is used by the Host to set the parameters for periodic advertising.
*
* @param[in]       instance : identifies the advertising set whose periodic advertising parameters are being configured.
* @param[in]       params : periodic adv parameters
*
* @return            - true : success
*                    - false  : failed
*
*/
bool BLEMultiAdvertising::setPeriodicAdvertisingParams(uint8_t instance, const esp_ble_gap_periodic_adv_params_t* params)
{
	esp_err_t rc;
	rc = esp_ble_gap_periodic_adv_set_params(instance, params);
	if (rc) log_e("set periodic advertising params err: %d", rc);

	return ESP_OK == rc;
}

/**
* @brief           This function is used to set the data used in periodic advertising PDUs.
*
* @param[in]       instance : identifies the advertising set whose periodic advertising parameters are being configured.
* @param[in]       length : the length of periodic data
* @param[in]       data : periodic data information
*
* @return            - true : success
*                    - false  : failed
*
*/
bool BLEMultiAdvertising::setPeriodicAdvertisingData(uint8_t instance, uint16_t length, const uint8_t* data)
{
	esp_err_t rc;
	rc = esp_ble_gap_config_periodic_adv_data_raw(instance, length, data);
	if (rc) log_e("set periodic advertising raw data err: %d", rc);

	return ESP_OK == rc;
}

/**
* @brief           This function is used to request the Controller to enable the periodic advertising for the advertising set specified
*
* @param[in]       instance : Used to identify an advertising set
*
* @return            - true : success
*                    - false  : failed
*
*/
bool BLEMultiAdvertising::startPeriodicAdvertising(uint8_t instance)
{
	esp_err_t rc;
	rc = esp_ble_gap_periodic_adv_start(instance);
	if (rc) log_e("start periodic advertising err: %d", rc);

	return ESP_OK == rc;
}

void BLEMultiAdvertising::setDuration(uint8_t instance, int duration, int max_events)
{
	ext_adv[instance] = { instance, duration, max_events };
}

#endif // SOC_BLE_50_SUPPORTED

#endif /* CONFIG_BLUEDROID_ENABLED */
#endif /* SOC_BLE_SUPPORTED */
