/*
 * BLE.cpp
 *
 *  Created on: Mar 16, 2017
 *      Author: kolban
 */
#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <nvs_flash.h>
#include <esp_bt.h>            // ESP32 BLE
#include <esp_bt_device.h>     // ESP32 BLE
#include <esp_bt_main.h>       // ESP32 BLE
#include <esp_gap_ble_api.h>   // ESP32 BLE
#include <esp_gatts_api.h>     // ESP32 BLE
#include <esp_gattc_api.h>     // ESP32 BLE
#include <esp_gatt_common_api.h>// ESP32 BLE
#include <esp_err.h>           // ESP32 ESP-IDF
#include <map>                 // Part of C++ Standard library
#include <sstream>             // Part of C++ Standard library
#include <iomanip>             // Part of C++ Standard library

#include "BLEDevice.h"
#include "BLEClient.h"
#include "BLEUtils.h"
#include "GeneralUtils.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "esp32-hal-bt.h"
#endif

#include "esp32-hal-log.h"


/**
 * Singletons for the BLEDevice.
 */
BLEServer* BLEDevice::m_pServer = nullptr;
BLEScan*   BLEDevice::m_pScan   = nullptr;
BLEClient* BLEDevice::m_pClient = nullptr;
bool       initialized          = false;  
esp_ble_sec_act_t BLEDevice::m_securityLevel = (esp_ble_sec_act_t)0;
BLESecurityCallbacks* BLEDevice::m_securityCallbacks = nullptr;
uint16_t   BLEDevice::m_localMTU = 23;  // not sure if this variable is useful
BLEAdvertising* BLEDevice::m_bleAdvertising = nullptr;
uint16_t BLEDevice::m_appId = 0;
std::map<uint16_t, conn_status_t> BLEDevice::m_connectedClientsMap;
gap_event_handler BLEDevice::m_customGapHandler = nullptr;
gattc_event_handler BLEDevice::m_customGattcHandler = nullptr;
gatts_event_handler BLEDevice::m_customGattsHandler = nullptr;

/**
 * @brief Create a new instance of a client.
 * @return A new instance of the client.
 */
/* STATIC */ BLEClient* BLEDevice::createClient() {
	log_v(">> createClient");
#ifndef CONFIG_GATTC_ENABLE  // Check that BLE GATTC is enabled in make menuconfig
	log_e("BLE GATTC is not enabled - CONFIG_GATTC_ENABLE not defined");
	abort();
#endif  // CONFIG_GATTC_ENABLE
	m_pClient = new BLEClient();
	log_v("<< createClient");
	return m_pClient;
} // createClient


/**
 * @brief Create a new instance of a server.
 * @return A new instance of the server.
 */
/* STATIC */ BLEServer* BLEDevice::createServer() {
	log_v(">> createServer");
#ifndef CONFIG_GATTS_ENABLE  // Check that BLE GATTS is enabled in make menuconfig
	log_e("BLE GATTS is not enabled - CONFIG_GATTS_ENABLE not defined");
	abort();
#endif // CONFIG_GATTS_ENABLE
	m_pServer = new BLEServer();
	m_pServer->createApp(m_appId++);
	log_v("<< createServer");
	return m_pServer;
} // createServer


/**
 * @brief Handle GATT server events.
 *
 * @param [in] event The event that has been newly received.
 * @param [in] gatts_if The connection to the GATT interface.
 * @param [in] param Parameters for the event.
 */
/* STATIC */ void BLEDevice::gattServerEventHandler(
   esp_gatts_cb_event_t      event,
   esp_gatt_if_t             gatts_if,
   esp_ble_gatts_cb_param_t* param
) {
	log_d("gattServerEventHandler [esp_gatt_if: %d] ... %s",
		gatts_if,
		BLEUtils::gattServerEventTypeToString(event).c_str());

	BLEUtils::dumpGattServerEvent(event, gatts_if, param);

	switch (event) {
		case ESP_GATTS_CONNECT_EVT: {
#ifdef CONFIG_BLE_SMP_ENABLE   // Check that BLE SMP (security) is configured in make menuconfig
			if(BLEDevice::m_securityLevel){
				esp_ble_set_encryption(param->connect.remote_bda, BLEDevice::m_securityLevel);
			}
#endif	// CONFIG_BLE_SMP_ENABLE
			break;
		} // ESP_GATTS_CONNECT_EVT

		default: {
			break;
		}
	} // switch


	if (BLEDevice::m_pServer != nullptr) {
		BLEDevice::m_pServer->handleGATTServerEvent(event, gatts_if, param);
	}

	if(m_customGattsHandler != nullptr) {
		m_customGattsHandler(event, gatts_if, param);
	}

} // gattServerEventHandler


/**
 * @brief Handle GATT client events.
 *
 * Handler for the GATT client events.
 *
 * @param [in] event
 * @param [in] gattc_if
 * @param [in] param
 */
/* STATIC */ void BLEDevice::gattClientEventHandler(
	esp_gattc_cb_event_t      event,
	esp_gatt_if_t             gattc_if,
	esp_ble_gattc_cb_param_t* param) {

	log_d("gattClientEventHandler [esp_gatt_if: %d] ... %s",
		gattc_if, BLEUtils::gattClientEventTypeToString(event).c_str());
	BLEUtils::dumpGattClientEvent(event, gattc_if, param);

	switch(event) {
		case ESP_GATTC_CONNECT_EVT: {
#ifdef CONFIG_BLE_SMP_ENABLE   // Check that BLE SMP (security) is configured in make menuconfig
			if(BLEDevice::m_securityLevel){
				esp_ble_set_encryption(param->connect.remote_bda, BLEDevice::m_securityLevel);
			}
#endif	// CONFIG_BLE_SMP_ENABLE
			break;
		} // ESP_GATTS_CONNECT_EVT

		default:
			break;
	} // switch
	for(auto &myPair : BLEDevice::getPeerDevices(true)) {
		conn_status_t conn_status = (conn_status_t)myPair.second;
		if(((BLEClient*)conn_status.peer_device)->getGattcIf() == gattc_if || ((BLEClient*)conn_status.peer_device)->getGattcIf() == ESP_GATT_IF_NONE || gattc_if == ESP_GATT_IF_NONE){
			((BLEClient*)conn_status.peer_device)->gattClientEventHandler(event, gattc_if, param);
		}
	}

	if(m_customGattcHandler != nullptr) {
		m_customGattcHandler(event, gattc_if, param);
	}


} // gattClientEventHandler


/**
 * @brief Handle GAP events.
 */
/* STATIC */ void BLEDevice::gapEventHandler(
	esp_gap_ble_cb_event_t event,
	esp_ble_gap_cb_param_t *param) {

	BLEUtils::dumpGapEvent(event, param);

	switch(event) {

		 case ESP_GAP_BLE_OOB_REQ_EVT:                                /* OOB request event */
			 log_i("ESP_GAP_BLE_OOB_REQ_EVT");
			 break;
		 case ESP_GAP_BLE_LOCAL_IR_EVT:                               /* BLE local IR event */
			 log_i("ESP_GAP_BLE_LOCAL_IR_EVT");
			 break;
		 case ESP_GAP_BLE_LOCAL_ER_EVT:                               /* BLE local ER event */
			 log_i("ESP_GAP_BLE_LOCAL_ER_EVT");
			 break;
		 case ESP_GAP_BLE_NC_REQ_EVT:								/*  NUMERIC CONFIRMATION  */
			 log_i("ESP_GAP_BLE_NC_REQ_EVT");
#ifdef CONFIG_BLE_SMP_ENABLE   // Check that BLE SMP (security) is configured in make menuconfig
			if(BLEDevice::m_securityCallbacks != nullptr){
			 	 esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, BLEDevice::m_securityCallbacks->onConfirmPIN(param->ble_security.key_notif.passkey));
			}
#endif	// CONFIG_BLE_SMP_ENABLE
			 break;
		 case ESP_GAP_BLE_PASSKEY_REQ_EVT:                           /* passkey request event */
			log_i("ESP_GAP_BLE_PASSKEY_REQ_EVT: ");
			// esp_log_buffer_hex(m_remote_bda, sizeof(m_remote_bda));
#ifdef CONFIG_BLE_SMP_ENABLE   // Check that BLE SMP (security) is configured in make menuconfig
			if(BLEDevice::m_securityCallbacks != nullptr){
				esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, BLEDevice::m_securityCallbacks->onPassKeyRequest());
			}
#endif	// CONFIG_BLE_SMP_ENABLE
			break;
			/*
			 * TODO should we add white/black list comparison?
			 */
		 case ESP_GAP_BLE_SEC_REQ_EVT:
			 /* send the positive(true) security response to the peer device to accept the security request.
			 If not accept the security request, should sent the security response with negative(false) accept value*/
			 log_i("ESP_GAP_BLE_SEC_REQ_EVT");
#ifdef CONFIG_BLE_SMP_ENABLE   // Check that BLE SMP (security) is configured in make menuconfig
			if(BLEDevice::m_securityCallbacks!=nullptr){
				esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, BLEDevice::m_securityCallbacks->onSecurityRequest());
			}
			else{
				esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
			}
#endif	// CONFIG_BLE_SMP_ENABLE
			break;
			 /*
			  *
			  */
		 case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:  //the app will receive this evt when the IO  has Output capability and the peer device IO has Input capability.
			 //display the passkey number to the user to input it in the peer deivce within 30 seconds
			 log_i("ESP_GAP_BLE_PASSKEY_NOTIF_EVT");
#ifdef CONFIG_BLE_SMP_ENABLE   // Check that BLE SMP (security) is configured in make menuconfig
			log_i("passKey = %d", param->ble_security.key_notif.passkey);
			if(BLEDevice::m_securityCallbacks!=nullptr){
				BLEDevice::m_securityCallbacks->onPassKeyNotify(param->ble_security.key_notif.passkey);
			}
#endif	// CONFIG_BLE_SMP_ENABLE
			 break;
		 case ESP_GAP_BLE_KEY_EVT:
			 //shows the ble key type info share with peer device to the user.
			 log_d("ESP_GAP_BLE_KEY_EVT");
#ifdef CONFIG_BLE_SMP_ENABLE   // Check that BLE SMP (security) is configured in make menuconfig
			 log_i("key type = %s", BLESecurity::esp_key_type_to_str(param->ble_security.ble_key.key_type));
#endif	// CONFIG_BLE_SMP_ENABLE
			 break;
		 case ESP_GAP_BLE_AUTH_CMPL_EVT:
			 log_i("ESP_GAP_BLE_AUTH_CMPL_EVT");
#ifdef CONFIG_BLE_SMP_ENABLE   // Check that BLE SMP (security) is configured in make menuconfig
			 if(BLEDevice::m_securityCallbacks != nullptr){
				 BLEDevice::m_securityCallbacks->onAuthenticationComplete(param->ble_security.auth_cmpl);
			 }
#endif	// CONFIG_BLE_SMP_ENABLE
			 break;
		default: {
			break;
		}
	} // switch

	if (BLEDevice::m_pClient != nullptr) {
		BLEDevice::m_pClient->handleGAPEvent(event, param);
	}

	if (BLEDevice::m_pScan != nullptr) {
		BLEDevice::getScan()->handleGAPEvent(event, param);
	}

	if(m_bleAdvertising != nullptr) {
		BLEDevice::getAdvertising()->handleGAPEvent(event, param);
	}

	if(m_customGapHandler != nullptr) {
		BLEDevice::m_customGapHandler(event, param);
	}

} // gapEventHandler


/**
 * @brief Get the BLE device address.
 * @return The BLE device address.
 */
/* STATIC*/ BLEAddress BLEDevice::getAddress() {
	const uint8_t* bdAddr = esp_bt_dev_get_address();
	esp_bd_addr_t addr;
	memcpy(addr, bdAddr, sizeof(addr));
	return BLEAddress(addr);
} // getAddress


/**
 * @brief Retrieve the Scan object that we use for scanning.
 * @return The scanning object reference.  This is a singleton object.  The caller should not
 * try and release/delete it.
 */
/* STATIC */ BLEScan* BLEDevice::getScan() {
	//log_v(">> getScan");
	if (m_pScan == nullptr) {
		m_pScan = new BLEScan();
		//log_d(" - creating a new scan object");
	}
	//log_v("<< getScan: Returning object at 0x%x", (uint32_t)m_pScan);
	return m_pScan;
} // getScan


/**
 * @brief Get the value of a characteristic of a service on a remote device.
 * @param [in] bdAddress
 * @param [in] serviceUUID
 * @param [in] characteristicUUID
 */
/* STATIC */ std::string BLEDevice::getValue(BLEAddress bdAddress, BLEUUID serviceUUID, BLEUUID characteristicUUID) {
	log_v(">> getValue: bdAddress: %s, serviceUUID: %s, characteristicUUID: %s", bdAddress.toString().c_str(), serviceUUID.toString().c_str(), characteristicUUID.toString().c_str());
	BLEClient* pClient = createClient();
	pClient->connect(bdAddress);
	std::string ret = pClient->getValue(serviceUUID, characteristicUUID);
	pClient->disconnect();
	log_v("<< getValue");
	return ret;
} // getValue


/**
 * @brief Initialize the %BLE environment.
 * @param deviceName The device name of the device.
 */
/* STATIC */ void BLEDevice::init(std::string deviceName) {
	if(!initialized){
		initialized = true; // Set the initialization flag to ensure we are only initialized once.

		esp_err_t errRc = ESP_OK;
#ifdef ARDUINO_ARCH_ESP32
		if (!btStart()) {
			errRc = ESP_FAIL;
			return;
		}
#else
		errRc = ::nvs_flash_init();
		if (errRc != ESP_OK) {
			log_e("nvs_flash_init: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
			return;
		}

#ifndef CLASSIC_BT_ENABLED
		esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);  
#endif
		esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
		errRc = esp_bt_controller_init(&bt_cfg);
		if (errRc != ESP_OK) {
			log_e("esp_bt_controller_init: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
			return;
		}

#ifndef CLASSIC_BT_ENABLED
		errRc = esp_bt_controller_enable(ESP_BT_MODE_BLE);
		if (errRc != ESP_OK) {
			log_e("esp_bt_controller_enable: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
			return;
		}
#else
		errRc = esp_bt_controller_enable(ESP_BT_MODE_BTDM);
		if (errRc != ESP_OK) {
			log_e("esp_bt_controller_enable: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
			return;
		}
#endif
#endif

		esp_bluedroid_status_t bt_state = esp_bluedroid_get_status();
		if (bt_state == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
			errRc = esp_bluedroid_init();
			if (errRc != ESP_OK) {
				log_e("esp_bluedroid_init: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
				return;
			}
		}

		if (bt_state != ESP_BLUEDROID_STATUS_ENABLED) {
			errRc = esp_bluedroid_enable();
			if (errRc != ESP_OK) {
				log_e("esp_bluedroid_enable: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
				return;
			}
		}

		errRc = esp_ble_gap_register_callback(BLEDevice::gapEventHandler);
		if (errRc != ESP_OK) {
			log_e("esp_ble_gap_register_callback: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
			return;
		}

#ifdef CONFIG_GATTC_ENABLE   // Check that BLE client is configured in make menuconfig
		errRc = esp_ble_gattc_register_callback(BLEDevice::gattClientEventHandler);
		if (errRc != ESP_OK) {
			log_e("esp_ble_gattc_register_callback: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
			return;
		}
#endif   // CONFIG_GATTC_ENABLE

#ifdef CONFIG_GATTS_ENABLE  // Check that BLE server is configured in make menuconfig
		errRc = esp_ble_gatts_register_callback(BLEDevice::gattServerEventHandler);
		if (errRc != ESP_OK) {
			log_e("esp_ble_gatts_register_callback: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
			return;
		}
#endif   // CONFIG_GATTS_ENABLE

		errRc = ::esp_ble_gap_set_device_name(deviceName.c_str());
		if (errRc != ESP_OK) {
			log_e("esp_ble_gap_set_device_name: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
			return;
		};

#ifdef CONFIG_BLE_SMP_ENABLE   // Check that BLE SMP (security) is configured in make menuconfig
		esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
		errRc = ::esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
		if (errRc != ESP_OK) {
			log_e("esp_ble_gap_set_security_param: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
			return;
		};
#endif // CONFIG_BLE_SMP_ENABLE
	}
	vTaskDelay(200 / portTICK_PERIOD_MS); // Delay for 200 msecs as a workaround to an apparent Arduino environment issue.
} // init


/**
 * @brief Set the transmission power.
 * The power level can be one of:
 * * ESP_PWR_LVL_N14
 * * ESP_PWR_LVL_N11
 * * ESP_PWR_LVL_N8
 * * ESP_PWR_LVL_N5
 * * ESP_PWR_LVL_N2
 * * ESP_PWR_LVL_P1
 * * ESP_PWR_LVL_P4
 * * ESP_PWR_LVL_P7
 *
 * The power types can be one of:
 * * ESP_BLE_PWR_TYPE_CONN_HDL0
 * * ESP_BLE_PWR_TYPE_CONN_HDL1
 * * ESP_BLE_PWR_TYPE_CONN_HDL2
 * * ESP_BLE_PWR_TYPE_CONN_HDL3
 * * ESP_BLE_PWR_TYPE_CONN_HDL4
 * * ESP_BLE_PWR_TYPE_CONN_HDL5
 * * ESP_BLE_PWR_TYPE_CONN_HDL6
 * * ESP_BLE_PWR_TYPE_CONN_HDL7
 * * ESP_BLE_PWR_TYPE_CONN_HDL8
 * * ESP_BLE_PWR_TYPE_ADV
 * * ESP_BLE_PWR_TYPE_SCAN
 * * ESP_BLE_PWR_TYPE_DEFAULT
 * @param [in] powerType.
 * @param [in] powerLevel.
 */
/* STATIC */ void BLEDevice::setPower(esp_power_level_t powerLevel, esp_ble_power_type_t powerType) {
	log_v(">> setPower: %d (type: %d)", powerLevel, powerType);
	esp_err_t errRc = ::esp_ble_tx_power_set(powerType, powerLevel);
	if (errRc != ESP_OK) {
		log_e("esp_ble_tx_power_set: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
	};
	log_v("<< setPower");
} // setPower


/**
 * @brief Set the value of a characteristic of a service on a remote device.
 * @param [in] bdAddress
 * @param [in] serviceUUID
 * @param [in] characteristicUUID
 */
/* STATIC */ void BLEDevice::setValue(BLEAddress bdAddress, BLEUUID serviceUUID, BLEUUID characteristicUUID, std::string value) {
	log_v(">> setValue: bdAddress: %s, serviceUUID: %s, characteristicUUID: %s", bdAddress.toString().c_str(), serviceUUID.toString().c_str(), characteristicUUID.toString().c_str());
	BLEClient* pClient = createClient();
	pClient->connect(bdAddress);
	pClient->setValue(serviceUUID, characteristicUUID, value);
	pClient->disconnect();
} // setValue


/**
 * @brief Return a string representation of the nature of this device.
 * @return A string representation of the nature of this device.
 */
/* STATIC */ std::string BLEDevice::toString() {
	std::string res = "BD Address: " + getAddress().toString();
	return res;
} // toString


/**
 * @brief Add an entry to the BLE white list.
 * @param [in] address The address to add to the white list.
 */
void BLEDevice::whiteListAdd(BLEAddress address) {
	log_v(">> whiteListAdd: %s", address.toString().c_str());
#ifdef ESP_IDF_VERSION_MAJOR
    esp_err_t errRc = esp_ble_gap_update_whitelist(true, *address.getNative(), BLE_WL_ADDR_TYPE_PUBLIC);  // HACK!!! True to add an entry.
#else
    esp_err_t errRc = esp_ble_gap_update_whitelist(true, *address.getNative());  // True to add an entry.
#endif
	if (errRc != ESP_OK) {
		log_e("esp_ble_gap_update_whitelist: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
	}
	log_v("<< whiteListAdd");
} // whiteListAdd


/**
 * @brief Remove an entry from the BLE white list.
 * @param [in] address The address to remove from the white list.
 */
void BLEDevice::whiteListRemove(BLEAddress address) {
	log_v(">> whiteListRemove: %s", address.toString().c_str());
#ifdef ESP_IDF_VERSION_MAJOR
    esp_err_t errRc = esp_ble_gap_update_whitelist(false, *address.getNative(), BLE_WL_ADDR_TYPE_PUBLIC);  // HACK!!! False to remove an entry.
#else
    esp_err_t errRc = esp_ble_gap_update_whitelist(false, *address.getNative());  // False to remove an entry.
#endif
	if (errRc != ESP_OK) {
		log_e("esp_ble_gap_update_whitelist: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
	}
	log_v("<< whiteListRemove");
} // whiteListRemove

/*
 * @brief Set encryption level that will be negotiated with peer device durng connection
 * @param [in] level Requested encryption level
 */
void BLEDevice::setEncryptionLevel(esp_ble_sec_act_t level) {
	BLEDevice::m_securityLevel = level;
}

/*
 * @brief Set callbacks that will be used to handle encryption negotiation events and authentication events
 * @param [in] cllbacks Pointer to BLESecurityCallbacks class callback
 */
void BLEDevice::setSecurityCallbacks(BLESecurityCallbacks* callbacks) {
	BLEDevice::m_securityCallbacks = callbacks;
}

/*
 * @brief Setup local mtu that will be used to negotiate mtu during request from client peer
 * @param [in] mtu Value to set local mtu, should be larger than 23 and lower or equal to 517
 */
esp_err_t BLEDevice::setMTU(uint16_t mtu) {
	log_v(">> setLocalMTU: %d", mtu);
	esp_err_t err = esp_ble_gatt_set_local_mtu(mtu);
	if (err == ESP_OK) {
		m_localMTU = mtu;
	} else {
		log_e("can't set local mtu value: %d", mtu);
	}
	log_v("<< setLocalMTU");
	return err;
}

/*
 * @brief Get local MTU value set during mtu request or default value
 */
uint16_t BLEDevice::getMTU() {
	return m_localMTU;
}

bool BLEDevice::getInitialized() {
	return initialized;
}

BLEAdvertising* BLEDevice::getAdvertising() {
	if(m_bleAdvertising == nullptr) {
		m_bleAdvertising = new BLEAdvertising();
		log_i("create advertising");
	}
	log_d("get advertising");
	return m_bleAdvertising; 
}

void BLEDevice::startAdvertising() {
	log_v(">> startAdvertising");
	getAdvertising()->start();
	log_v("<< startAdvertising");
} // startAdvertising

void BLEDevice::stopAdvertising() {
    log_v(">> stopAdvertising");
    getAdvertising()->stop();
    log_v("<< stopAdvertising");
} // stopAdvertising

/* multi connect support */
/* requires a little more work */
std::map<uint16_t, conn_status_t> BLEDevice::getPeerDevices(bool _client) {
	return m_connectedClientsMap;
}

BLEClient* BLEDevice::getClientByGattIf(uint16_t conn_id) {
	return (BLEClient*)m_connectedClientsMap.find(conn_id)->second.peer_device;
}

void BLEDevice::updatePeerDevice(void* peer, bool _client, uint16_t conn_id) {
	log_d("update conn_id: %d, GATT role: %s", conn_id, _client? "client":"server");
	std::map<uint16_t, conn_status_t>::iterator it = m_connectedClientsMap.find(ESP_GATT_IF_NONE);
	if (it != m_connectedClientsMap.end()) {
		std::swap(m_connectedClientsMap[conn_id], it->second);
		m_connectedClientsMap.erase(it);
	}else{
		it = m_connectedClientsMap.find(conn_id);
		if (it != m_connectedClientsMap.end()) {
			conn_status_t _st = it->second;
			_st.peer_device = peer;
			std::swap(m_connectedClientsMap[conn_id], _st);
		}
	}
}

void BLEDevice::addPeerDevice(void* peer, bool _client, uint16_t conn_id) {
	log_i("add conn_id: %d, GATT role: %s", conn_id, _client? "client":"server");
	conn_status_t status = {
		.peer_device = peer,
		.connected = true,
		.mtu = 23
	};

	m_connectedClientsMap.insert(std::pair<uint16_t, conn_status_t>(conn_id, status));
}

void BLEDevice::removePeerDevice(uint16_t conn_id, bool _client) {
	log_i("remove: %d, GATT role %s", conn_id, _client?"client":"server");
	if(m_connectedClientsMap.find(conn_id) != m_connectedClientsMap.end())
		m_connectedClientsMap.erase(conn_id);
}

/* multi connect support */

/**
 * @brief de-Initialize the %BLE environment.
 * @param release_memory release the internal BT stack memory
 */
/* STATIC */ void BLEDevice::deinit(bool release_memory) {
    if (!initialized) return;

    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
#ifdef ARDUINO_ARCH_ESP32
    if (release_memory) {
        esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);  // <-- require tests because we released classic BT memory and this can cause crash (most likely not, esp-idf takes care of it)
    } else {
        initialized = false;   
    } 
#endif
}

void BLEDevice::setCustomGapHandler(gap_event_handler handler) {
	m_customGapHandler = handler;
}

void BLEDevice::setCustomGattcHandler(gattc_event_handler handler) {
	m_customGattcHandler = handler;
}

void BLEDevice::setCustomGattsHandler(gatts_event_handler handler) {
	m_customGattsHandler = handler;
}

#endif // CONFIG_BT_ENABLED
