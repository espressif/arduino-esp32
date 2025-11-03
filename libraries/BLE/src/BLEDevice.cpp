/*
 * BLEDevice.cpp
 *
 *  Created on: Mar 16, 2017
 *      Author: kolban
 *
 *  Modified on: Feb 18, 2025
 *      Author: lucasssvaz (based on kolban's and h2zero's work)
 *      Description: Added support for NimBLE
 */

#include "soc/soc_caps.h"
#include "sdkconfig.h"
#if defined(SOC_BLE_SUPPORTED) || defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#if defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)

/***************************************************************************
 *                           Common includes                               *
 ***************************************************************************/

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <nvs_flash.h>

#if SOC_BLE_SUPPORTED
#include <esp_bt.h>
#endif

#include <esp_err.h>
#include <map>
#include <sstream>
#include <iomanip>

#include "BLEDevice.h"
#include "BLEClient.h"
#include "BLEUtils.h"
#include "GeneralUtils.h"
#include "BLESecurity.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "esp32-hal-bt.h"
#endif

#include "esp32-hal-log.h"

/***************************************************************************
 *                           Bluedroid includes                            *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_bt_device.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <esp_gattc_api.h>
#include <esp_gatt_common_api.h>
#endif

/***************************************************************************
 *                           NimBLE includes                               *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#ifdef CONFIG_BT_NIMBLE_LEGACY_VHCI_ENABLE
#include <esp_nimble_hci.h>
#endif
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <host/ble_hs.h>
#include <host/ble_hs_pvcy.h>
#include <host/ble_gap.h>
#include "host/ble_hs_pvcy.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#endif

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
#include "esp32-hal-hosted.h"
#endif

/***************************************************************************
 *                            Common properties                            *
 ***************************************************************************/

/**
 * Singletons for the BLEDevice.
 */
BLEServer *BLEDevice::m_pServer = nullptr;
BLEScan *BLEDevice::m_pScan = nullptr;
BLEClient *BLEDevice::m_pClient = nullptr;
static bool initialized = false;
BLESecurityCallbacks *BLEDevice::m_securityCallbacks = nullptr;
uint16_t BLEDevice::m_localMTU = 23;  // not sure if this variable is useful
BLEAdvertising *BLEDevice::m_bleAdvertising = nullptr;
uint16_t BLEDevice::m_appId = 0;
std::map<uint16_t, conn_status_t> BLEDevice::m_connectedClientsMap;
gap_event_handler BLEDevice::m_customGapHandler = nullptr;

/***************************************************************************
 *                           Bluedroid properties                          *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
gattc_event_handler BLEDevice::m_customGattcHandler = nullptr;
gatts_event_handler BLEDevice::m_customGattsHandler = nullptr;
#endif

/***************************************************************************
 *                            NimBLE properties                            *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
ble_gap_event_listener BLEDevice::m_listener;
BLEDeviceCallbacks BLEDevice::defaultDeviceCallbacks{};
BLEDeviceCallbacks *BLEDevice::m_pDeviceCallbacks = &defaultDeviceCallbacks;
uint8_t BLEDevice::m_ownAddrType = BLE_OWN_ADDR_PUBLIC;
bool BLEDevice::m_synced = false;
#endif

/***************************************************************************
 *                           Common functions                             *
 ***************************************************************************/

/**
 * @brief Create a new instance of a client.
 * @return A new instance of the client.
 */
BLEClient *BLEDevice::createClient() {
  log_v(">> createClient");
#if !defined(CONFIG_GATTC_ENABLE) && !defined(CONFIG_BT_NIMBLE_ROLE_CENTRAL)
  log_e("BLE Client not enabled. Check CONFIG_GATTC_ENABLE for BlueDroid or CONFIG_BT_NIMBLE_ROLE_CENTRAL for NimBLE");
  abort();
#endif  // CONFIG_GATTC_ENABLE

#ifdef CONFIG_NIMBLE_ENABLED
  if (m_connectedClientsMap.size() >= CONFIG_BT_NIMBLE_MAX_CONNECTIONS) {
    log_e("Unable to create client. Max connections reached. Cur=%d Max=%d", m_connectedClientsMap.size(), CONFIG_BT_NIMBLE_MAX_CONNECTIONS);
    m_pClient = nullptr;
  } else
#endif
  {
    m_pClient = new BLEClient();
  }
  log_v("<< createClient");
  return m_pClient;
}  // createClient

/**
 * @brief Create a new instance of a server.
 * @return A new instance of the server.
 */
BLEServer *BLEDevice::createServer() {
  log_v(">> createServer");
#if !defined(CONFIG_GATTS_ENABLE) && !defined(CONFIG_BT_NIMBLE_ROLE_PERIPHERAL)
  log_e("BLE Server not enabled. Check CONFIG_GATTS_ENABLE for BlueDroid or CONFIG_BT_NIMBLE_ROLE_PERIPHERAL for NimBLE");
  abort();
#endif  // CONFIG_GATTS_ENABLE

  if (m_pServer == nullptr) {
    m_pServer = new BLEServer();
    m_pServer->createApp(m_appId++);
#if defined(CONFIG_NIMBLE_ENABLED)
    ble_gatts_reset();
    ble_svc_gap_init();
    ble_svc_gatt_init();
#endif
  }
  log_v("<< createServer");
  return m_pServer;
}  // createServer

/**
 * @brief Get the BLE device address.
 * @return The BLE device address.
 */
BLEAddress BLEDevice::getAddress() {
#if defined(CONFIG_BLUEDROID_ENABLED)
  const uint8_t *bdAddr = esp_bt_dev_get_address();
  esp_bd_addr_t addr;
  memcpy(addr, bdAddr, sizeof(addr));
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  ble_addr_t addr;
  int ret = ble_hs_id_copy_addr(m_ownAddrType, addr.val, NULL);
  if (ret != 0) {
    log_e("No BLE address found. rc=%d", ret);
    return BLEAddress();
  }
#endif
  return BLEAddress(addr);
}  // getAddress

/**
 * @brief Retrieve the Scan object that we use for scanning.
 * @return The scanning object reference.  This is a singleton object.  The caller should not
 * try and release/delete it.
 */
BLEScan *BLEDevice::getScan() {
  //log_v(">> getScan");
  if (m_pScan == nullptr) {
    m_pScan = new BLEScan();
    //log_d(" - creating a new scan object");
  }
  //log_v("<< getScan: Returning object at 0x%x", (uint32_t)m_pScan);
  return m_pScan;
}  // getScan

/**
 * @brief Retrieve the server object
 * @return The server object
 */
BLEServer *BLEDevice::getServer() {
  return m_pServer;
}  // getServer

/**
 * @brief Get the value of a characteristic of a service on a remote device.
 * @param [in] bdAddress
 * @param [in] serviceUUID
 * @param [in] characteristicUUID
 */
String BLEDevice::getValue(BLEAddress bdAddress, BLEUUID serviceUUID, BLEUUID characteristicUUID) {
  log_v(
    ">> getValue: bdAddress: %s, serviceUUID: %s, characteristicUUID: %s", bdAddress.toString().c_str(), serviceUUID.toString().c_str(),
    characteristicUUID.toString().c_str()
  );
  BLEClient *pClient = createClient();
  pClient->connect(bdAddress);
  String ret = pClient->getValue(serviceUUID, characteristicUUID);
  pClient->disconnect();
  log_v("<< getValue");
  return ret;
}  // getValue

/**
 * @brief Initialize the %BLE environment.
 * @param deviceName The device name of the device.
 */
void BLEDevice::init(String deviceName) {
  if (!initialized) {
    log_i("Initializing BLE stack: %s", getBLEStackString().c_str());

    esp_err_t errRc = ESP_OK;
#if defined(CONFIG_BLUEDROID_ENABLED)
#if defined(ARDUINO_ARCH_ESP32)
    if (!btStart()) {
      errRc = ESP_FAIL;
      return;
    }
#else
    btStarted();
    errRc = nvs_flash_init();
    if (errRc == ESP_ERR_NVS_NO_FREE_PAGES || errRc == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      errRc = nvs_flash_erase();
      if (errRc == ESP_OK) {
        errRc = nvs_flash_init();
      }
    }

    if (errRc != ESP_OK) {
      log_e("nvs_flash_init: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
      return;
    }

#ifndef CONFIG_BT_CLASSIC_ENABLED
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
#endif

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    errRc = esp_bt_controller_init(&bt_cfg);
    if (errRc != ESP_OK) {
      log_e("esp_bt_controller_init: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
      return;
    }

#ifndef CONFIG_BT_CLASSIC_ENABLED
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
#endif  // !ARDUINO_ARCH_ESP32

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

#ifdef CONFIG_GATTC_ENABLE  // Check that BLE client is configured in make menuconfig
    errRc = esp_ble_gattc_register_callback(BLEDevice::gattClientEventHandler);
    if (errRc != ESP_OK) {
      log_e("esp_ble_gattc_register_callback: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
      return;
    }
#endif  // CONFIG_GATTC_ENABLE

#ifdef CONFIG_GATTS_ENABLE  // Check that BLE server is configured in make menuconfig
    errRc = esp_ble_gatts_register_callback(BLEDevice::gattServerEventHandler);
    if (errRc != ESP_OK) {
      log_e("esp_ble_gatts_register_callback: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
      return;
    }
#endif  // CONFIG_GATTS_ENABLE

    errRc = ::esp_ble_gap_set_device_name(deviceName.c_str());
    if (errRc != ESP_OK) {
      log_e("esp_ble_gap_set_device_name: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
      return;
    };

#ifdef CONFIG_BLE_SMP_ENABLE  // Check that BLE SMP (security) is configured in make menuconfig
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
    errRc = ::esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    if (errRc != ESP_OK) {
      log_e("esp_ble_gap_set_security_param: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
      return;
    };
#endif  // CONFIG_BLE_SMP_ENABLE
#endif  // CONFIG_BLUEDROID_ENABLED

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
    // Initialize esp-hosted transport for BLE HCI when explicitly enabled
    if (!hostedInitBLE()) {
      log_e("Failed to initialize ESP-Hosted for BLE");
      return;
    }

    // Hosted HCI driver will be initialized automatically by NimBLE transport layer
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
    errRc = nimble_port_init();
    if (errRc != ESP_OK) {
      log_e("nimble_port_init: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
      return;
    }

    // Global struct ble_hs_cfg from nimble/host/ble_hs.h needs to be initialized
    ble_hs_cfg.reset_cb = BLEDevice::onReset;
    ble_hs_cfg.sync_cb = BLEDevice::onSync;
    ble_hs_cfg.store_status_cb = [](struct ble_store_status_event *event, void *arg) {
      return m_pDeviceCallbacks->onStoreStatus(event, arg);
    };
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
    ble_hs_cfg.sm_bonding = 0;
    ble_hs_cfg.sm_mitm = 0;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC;
#if MYNEWT_VAL(BLE_LL_CFG_FEAT_LL_PRIVACY)
    ble_hs_cfg.sm_our_key_dist |= BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist |= BLE_SM_PAIR_KEY_DIST_ID;
#endif

    errRc = ble_svc_gap_device_name_set(deviceName.c_str());
    if (errRc != ESP_OK) {
      log_e("ble_svc_gap_device_name_set: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
    }

    ble_store_config_init();
    nimble_port_freertos_init(BLEDevice::host_task);

    while (!m_synced) {
      ble_npl_time_delay(1);
    }
#endif                   // CONFIG_NIMBLE_ENABLED
    initialized = true;  // Set the initialization flag to ensure we are only initialized once.
  }
  vTaskDelay(200 / portTICK_PERIOD_MS);  // Delay for 200 msecs as a workaround to an apparent Arduino environment issue.
}  // init

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
void BLEDevice::setPower(esp_power_level_t powerLevel, esp_ble_power_type_t powerType) {
  log_v(">> setPower: %d (type: %d)", powerLevel, powerType);
#if defined(SOC_BLE_SUPPORTED)
  esp_err_t errRc = ::esp_ble_tx_power_set(powerType, powerLevel);
  if (errRc != ESP_OK) {
    log_e("esp_ble_tx_power_set: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
  };
#else
  log_w("setPower not supported with hosted HCI - power controlled by co-processor");
#endif
  log_v("<< setPower");
}  // setPower

/**
 * @brief Get the transmission power.
 * @param [in] powerType The power level to set, can be one of:
 * *   ESP_BLE_PWR_TYPE_CONN_HDL0  = 0,  For connection handle 0
 * *   ESP_BLE_PWR_TYPE_CONN_HDL1  = 1,  For connection handle 1
 * *   ESP_BLE_PWR_TYPE_CONN_HDL2  = 2,  For connection handle 2
 * *   ESP_BLE_PWR_TYPE_CONN_HDL3  = 3,  For connection handle 3
 * *   ESP_BLE_PWR_TYPE_CONN_HDL4  = 4,  For connection handle 4
 * *   ESP_BLE_PWR_TYPE_CONN_HDL5  = 5,  For connection handle 5
 * *   ESP_BLE_PWR_TYPE_CONN_HDL6  = 6,  For connection handle 6
 * *   ESP_BLE_PWR_TYPE_CONN_HDL7  = 7,  For connection handle 7
 * *   ESP_BLE_PWR_TYPE_CONN_HDL8  = 8,  For connection handle 8
 * *   ESP_BLE_PWR_TYPE_ADV        = 9,  For advertising
 * *   ESP_BLE_PWR_TYPE_SCAN       = 10, For scan
 * *   ESP_BLE_PWR_TYPE_DEFAULT    = 11, For default, if not set other, it will use default value
 * @return the power level currently used by the type specified.
 */

int BLEDevice::getPower(esp_ble_power_type_t powerType) {
#if SOC_BLE_SUPPORTED
  switch (esp_ble_tx_power_get(powerType)) {
    case ESP_PWR_LVL_N12: return -12;
    case ESP_PWR_LVL_N9:  return -9;
    case ESP_PWR_LVL_N6:  return -6;
    case ESP_PWR_LVL_N3:  return -6;
    case ESP_PWR_LVL_N0:  return 0;
    case ESP_PWR_LVL_P3:  return 3;
    case ESP_PWR_LVL_P6:  return 6;
    case ESP_PWR_LVL_P9:  return 9;
    default:              return -128;
  }
#else
  log_w("getPower not supported with hosted HCI - power controlled by co-processor");
  return 0;  // Return default power level
#endif
}  // getPower

/**
 * @brief Set the value of a characteristic of a service on a remote device.
 * @param [in] bdAddress
 * @param [in] serviceUUID
 * @param [in] characteristicUUID
 */
void BLEDevice::setValue(BLEAddress bdAddress, BLEUUID serviceUUID, BLEUUID characteristicUUID, String value) {
  log_v(
    ">> setValue: bdAddress: %s, serviceUUID: %s, characteristicUUID: %s", bdAddress.toString().c_str(), serviceUUID.toString().c_str(),
    characteristicUUID.toString().c_str()
  );
  BLEClient *pClient = createClient();
  pClient->connect(bdAddress);
  pClient->setValue(serviceUUID, characteristicUUID, value);
  pClient->disconnect();
}  // setValue

/**
 * @brief Return a string representation of the nature of this device.
 * @return A string representation of the nature of this device.
 */
String BLEDevice::toString() {
  String res = "BD Address: " + getAddress().toString();
  return res;
}  // toString

/**
 * @brief Add an entry to the BLE white list.
 * @param [in] address The address to add to the white list.
 */
void BLEDevice::whiteListAdd(BLEAddress address) {
  log_v(">> whiteListAdd: %s", address.toString().c_str());
#ifdef CONFIG_BLUEDROID_ENABLED
#ifdef ESP_IDF_VERSION_MAJOR
  esp_err_t errRc = esp_ble_gap_update_whitelist(true, address.getNative(), BLE_WL_ADDR_TYPE_PUBLIC);  // HACK!!! True to add an entry.
#else
  esp_err_t errRc = esp_ble_gap_update_whitelist(true, address.getNative());  // True to add an entry.
#endif
  if (errRc != ESP_OK) {
    log_e("esp_ble_gap_update_whitelist: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
  }
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  if (!BLEDevice::onWhiteList(address)) {
    m_whiteList.push_back(address);
    int errRc = ble_gap_wl_set(reinterpret_cast<ble_addr_t *>(&m_whiteList[0]), m_whiteList.size());
    if (errRc != 0) {
      log_e("Failed adding to whitelist rc=%d", errRc);
      m_whiteList.pop_back();
    }
  }
#endif
  log_v("<< whiteListAdd");
}  // whiteListAdd

/**
 * @brief Remove an entry from the BLE white list.
 * @param [in] address The address to remove from the white list.
 */
void BLEDevice::whiteListRemove(BLEAddress address) {
  log_v(">> whiteListRemove: %s", address.toString().c_str());
#ifdef CONFIG_BLUEDROID_ENABLED
#ifdef ESP_IDF_VERSION_MAJOR
  esp_err_t errRc = esp_ble_gap_update_whitelist(false, address.getNative(), BLE_WL_ADDR_TYPE_PUBLIC);  // HACK!!! False to remove an entry.
#else
  esp_err_t errRc = esp_ble_gap_update_whitelist(false, address.getNative());  // False to remove an entry.
#endif
  if (errRc != ESP_OK) {
    log_e("esp_ble_gap_update_whitelist: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
  }
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  for (auto it = m_whiteList.begin(); it < m_whiteList.end(); ++it) {
    if (*it == address) {
      m_whiteList.erase(it);
      int errRc = ble_gap_wl_set(reinterpret_cast<ble_addr_t *>(&m_whiteList[0]), m_whiteList.size());
      if (errRc != 0) {
        m_whiteList.push_back(address);
        log_e("Failed removing from whitelist rc=%d", errRc);
      }
      std::vector<BLEAddress>(m_whiteList).swap(m_whiteList);
    }
  }
#endif
  log_v("<< whiteListRemove");
}  // whiteListRemove

/*
 * @brief Set callbacks that will be used to handle encryption negotiation events and authentication events
 * @param [in] callbacks Pointer to BLESecurityCallbacks class callback
 */
void BLEDevice::setSecurityCallbacks(BLESecurityCallbacks *callbacks) {
  BLEDevice::m_securityCallbacks = callbacks;
}

/*
 * @brief Setup local mtu that will be used to negotiate mtu during request from client peer
 * @param [in] mtu Value to set local mtu, should be larger than 23 and lower or equal to 517
 */
esp_err_t BLEDevice::setMTU(uint16_t mtu) {
  log_v(">> setLocalMTU: %d", mtu);

#ifdef CONFIG_BLUEDROID_ENABLED
  esp_err_t err = esp_ble_gatt_set_local_mtu(mtu);
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  int err = ble_att_set_preferred_mtu(mtu);
#endif

  if (err == ESP_OK) {
    m_localMTU = mtu;
  } else {
    log_e("can't set local mtu value: %d, rc=%d", mtu, err);
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

BLEAdvertising *BLEDevice::getAdvertising() {
  if (m_bleAdvertising == nullptr) {
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
}  // startAdvertising

void BLEDevice::stopAdvertising() {
  log_v(">> stopAdvertising");
  getAdvertising()->stop();
  log_v("<< stopAdvertising");
}  // stopAdvertising

/* multi connect support */
/* requires a little more work */
std::map<uint16_t, conn_status_t> BLEDevice::getPeerDevices(bool _client) {
  return m_connectedClientsMap;
}

BLEClient *BLEDevice::getClientByID(uint16_t conn_id) {
  return BLEDevice::getClientByGattIf(conn_id);
}

BLEClient *BLEDevice::getClientByGattIf(uint16_t conn_id) {
  return (BLEClient *)m_connectedClientsMap.find(conn_id)->second.peer_device;
}

BLEClient *BLEDevice::getClientByAddress(BLEAddress address) {
  for (auto &it : m_connectedClientsMap) {
    if (((BLEClient *)it.second.peer_device)->getPeerAddress() == address) {
      return (BLEClient *)it.second.peer_device;
    }
  }
  return nullptr;
}

void BLEDevice::updatePeerDevice(void *peer, bool _client, uint16_t conn_id) {
  log_d("update conn_id: %d, GATT role: %s", conn_id, _client ? "client" : "server");
  std::map<uint16_t, conn_status_t>::iterator it = m_connectedClientsMap.find(ESP_GATT_IF_NONE);
  if (it != m_connectedClientsMap.end()) {
    std::swap(m_connectedClientsMap[conn_id], it->second);
    m_connectedClientsMap.erase(it);
  } else {
    it = m_connectedClientsMap.find(conn_id);
    if (it != m_connectedClientsMap.end()) {
      conn_status_t _st = it->second;
      _st.peer_device = peer;
      std::swap(m_connectedClientsMap[conn_id], _st);
    }
  }
}

void BLEDevice::addPeerDevice(void *peer, bool _client, uint16_t conn_id) {
  log_i("add conn_id: %d, GATT role: %s", conn_id, _client ? "client" : "server");
  conn_status_t status = {.peer_device = peer, .connected = true, .mtu = 23};

  m_connectedClientsMap.insert(std::pair<uint16_t, conn_status_t>(conn_id, status));
}

//there may have some situation that invoking this function simultaneously, that will cause CORRUPT HEAP
//let this function serializable
portMUX_TYPE BLEDevice::mux = portMUX_INITIALIZER_UNLOCKED;
void BLEDevice::removePeerDevice(uint16_t conn_id, bool _client) {
  portENTER_CRITICAL(&mux);
  log_i("remove: %d, GATT role %s", conn_id, _client ? "client" : "server");
  if (m_connectedClientsMap.find(conn_id) != m_connectedClientsMap.end()) {
    m_connectedClientsMap.erase(conn_id);
  }
  portEXIT_CRITICAL(&mux);
}

/* multi connect support */

/**
 * @brief de-Initialize the %BLE environment.
 * @param release_memory release the internal BT stack memory
 */
void BLEDevice::deinit(bool release_memory) {
  if (!initialized) {
    return;
  }

#ifdef CONFIG_BLUEDROID_ENABLED
  esp_bluedroid_disable();
  esp_bluedroid_deinit();
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  nimble_port_stop();
  nimble_port_deinit();
#endif

#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
  hostedDeinitBLE();
#endif

#if CONFIG_BT_CONTROLLER_ENABLED
  esp_bt_controller_disable();
  esp_bt_controller_deinit();
#endif

#ifdef ARDUINO_ARCH_ESP32
  if (release_memory) {
    // Require tests because we released classic BT memory and this can cause crash (most likely not, esp-idf takes care of it)
#if CONFIG_BT_CONTROLLER_ENABLED
    esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
#endif
  } else {
#ifdef CONFIG_NIMBLE_ENABLED
    m_synced = false;
#endif
    initialized = false;
  }
#endif
}

void BLEDevice::setCustomGapHandler(gap_event_handler handler) {
  m_customGapHandler = handler;
#ifdef CONFIG_NIMBLE_ENABLED
  int rc = ble_gap_event_listener_register(&m_listener, handler, NULL);
  if (rc == BLE_HS_EALREADY) {
    log_i("Already listening to GAP events.");
  } else if (rc != 0) {
    log_e("ble_gap_event_listener_register: rc=%d %s", rc, GeneralUtils::errorToString(rc));
  }
#endif
}

BLEStack BLEDevice::getBLEStack() {
#if defined(CONFIG_BLUEDROID_ENABLED)
  return BLEStack::BLUEDROID;
#elif defined(CONFIG_NIMBLE_ENABLED)
  return BLEStack::NIMBLE;
#else
  return BLEStack::UNKNOWN;
#endif
}

String BLEDevice::getBLEStackString() {
  switch (getBLEStack()) {
    case BLEStack::BLUEDROID: return "Bluedroid";
    case BLEStack::NIMBLE:    return "NimBLE";
    case BLEStack::UNKNOWN:
    default:                  return "Unknown";
  }
}

bool BLEDevice::isHostedBLE() {
#if defined(CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE)
  return true;
#else
  return false;
#endif
}

/***************************************************************************
 *                           Bluedroid functions                           *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)

/**
 * @brief Handle GATT server events.
 *
 * @param [in] event The event that has been newly received.
 * @param [in] gatts_if The connection to the GATT interface.
 * @param [in] param Parameters for the event.
 */
void BLEDevice::gattServerEventHandler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
  log_d("gattServerEventHandler [esp_gatt_if: %d] ... %s", gatts_if, BLEUtils::gattServerEventTypeToString(event).c_str());

  BLEUtils::dumpGattServerEvent(event, gatts_if, param);

  switch (event) {
    case ESP_GATTS_CONNECT_EVT:
    {
      if (BLESecurity::m_securityEnabled && BLESecurity::m_forceSecurity) {
        BLESecurity::startSecurity(param->connect.remote_bda);
      }
      break;
    }  // ESP_GATTS_CONNECT_EVT

    default:
    {
      break;
    }
  }  // switch

  if (BLEDevice::m_pServer != nullptr) {
    BLEDevice::m_pServer->handleGATTServerEvent(event, gatts_if, param);
  }

  if (m_customGattsHandler != nullptr) {
    m_customGattsHandler(event, gatts_if, param);
  }

}  // gattServerEventHandler

/**
 * @brief Handle GATT client events.
 *
 * Handler for the GATT client events.
 *
 * @param [in] event
 * @param [in] gattc_if
 * @param [in] param
 */
void BLEDevice::gattClientEventHandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {

  log_d("gattClientEventHandler [esp_gatt_if: %d] ... %s", gattc_if, BLEUtils::gattClientEventTypeToString(event).c_str());
  BLEUtils::dumpGattClientEvent(event, gattc_if, param);

  switch (event) {
    case ESP_GATTC_CONNECT_EVT:
    {
      // Set encryption on connect for BlueDroid when security is enabled
      // This ensures security is established before any secure operations
      if (BLESecurity::m_securityEnabled && BLESecurity::m_forceSecurity) {
        BLESecurity::startSecurity(param->connect.remote_bda);
      }
      break;
    }  // ESP_GATTS_CONNECT_EVT

    default: break;
  }  // switch
  for (auto &myPair : BLEDevice::getPeerDevices(true)) {
    conn_status_t conn_status = (conn_status_t)myPair.second;
    if (((BLEClient *)conn_status.peer_device)->getGattcIf() == gattc_if || ((BLEClient *)conn_status.peer_device)->getGattcIf() == ESP_GATT_IF_NONE
        || gattc_if == ESP_GATT_IF_NONE) {
      ((BLEClient *)conn_status.peer_device)->gattClientEventHandler(event, gattc_if, param);
    }
  }

  if (m_customGattcHandler != nullptr) {
    m_customGattcHandler(event, gattc_if, param);
  }

}  // gattClientEventHandler

/**
 * @brief Handle GAP events.
 */
void BLEDevice::gapEventHandler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {

  BLEUtils::dumpGapEvent(event, param);

  switch (event) {

    case ESP_GAP_BLE_OOB_REQ_EVT:  /* OOB request event */ log_i("ESP_GAP_BLE_OOB_REQ_EVT"); break;
    case ESP_GAP_BLE_LOCAL_IR_EVT: /* BLE local IR event */ log_i("ESP_GAP_BLE_LOCAL_IR_EVT"); break;
    case ESP_GAP_BLE_LOCAL_ER_EVT: /* BLE local ER event */ log_i("ESP_GAP_BLE_LOCAL_ER_EVT"); break;
    case ESP_GAP_BLE_NC_REQ_EVT: /*  NUMERIC CONFIRMATION  */
    {
      log_i("ESP_GAP_BLE_NC_REQ_EVT");
#ifdef CONFIG_BLE_SMP_ENABLE  // Check that BLE SMP (security) is configured in make menuconfig
      if (BLEDevice::m_securityCallbacks != nullptr) {
        esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, BLEDevice::m_securityCallbacks->onConfirmPIN(param->ble_security.key_notif.passkey));
      } else {
        log_e("onConfirmPIN not implemented. Rejecting connection");
        esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, false);
      }
#endif  // CONFIG_BLE_SMP_ENABLE
    } break;
    case ESP_GAP_BLE_PASSKEY_REQ_EVT: /* passkey request event */
    {
      log_i("ESP_GAP_BLE_PASSKEY_REQ_EVT: ");
      // esp_log_buffer_hex(m_remote_bda, sizeof(m_remote_bda));
#ifdef CONFIG_BLE_SMP_ENABLE  // Check that BLE SMP (security) is configured in make menuconfig
      uint32_t passkey = BLESecurity::getPassKey();

      if (!BLESecurity::m_passkeySet) {
        if (BLEDevice::m_securityCallbacks != nullptr) {
          log_i("No passkey set, getting passkey from onPassKeyRequest");
          passkey = BLEDevice::m_securityCallbacks->onPassKeyRequest();
        } else {
          log_w("*ATTENTION* onPassKeyRequest not implemented and no static passkey set.");
        }
      }

      if (BLESecurity::m_staticPasskey && passkey == BLE_SM_DEFAULT_PASSKEY) {
        log_w("*ATTENTION* Using default passkey: %06d", BLE_SM_DEFAULT_PASSKEY);
        log_w("*ATTENTION* Please use a random passkey or set a different static passkey");
      } else {
        log_i("Passkey: %d", passkey);
      }

      esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, passkey);
#endif  // CONFIG_BLE_SMP_ENABLE
    } break;
      /*
			 * TODO should we add white/black list comparison?
			 */
    case ESP_GAP_BLE_SEC_REQ_EVT:
    {
      /* send the positive(true) security response to the peer device to accept the security request.
			 If not accept the security request, should sent the security response with negative(false) accept value*/
      log_i("ESP_GAP_BLE_SEC_REQ_EVT");
#ifdef CONFIG_BLE_SMP_ENABLE  // Check that BLE SMP (security) is configured in make menuconfig
      if (BLEDevice::m_securityCallbacks != nullptr) {
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, BLEDevice::m_securityCallbacks->onSecurityRequest());
      } else {
        log_w("onSecurityRequest not implemented. Accepting security request");
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
      }
#endif  // CONFIG_BLE_SMP_ENABLE
    } break;
      /*
			  *
			  */
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:  //the app will receive this evt when the IO  has Output capability and the peer device IO has Input capability.
    {
      //display the passkey number to the user to input it in the peer device within 30 seconds
      log_i("ESP_GAP_BLE_PASSKEY_NOTIF_EVT");
#ifdef CONFIG_BLE_SMP_ENABLE  // Check that BLE SMP (security) is configured in make menuconfig
      uint32_t passkey = param->ble_security.key_notif.passkey;

      if (!BLESecurity::m_passkeySet) {
        log_w("No passkey set");
      }

      if (BLESecurity::m_staticPasskey && passkey == BLE_SM_DEFAULT_PASSKEY) {
        log_w("*ATTENTION* Using default passkey: %06d", BLE_SM_DEFAULT_PASSKEY);
        log_w("*ATTENTION* Please use a random passkey or set a different static passkey");
      } else {
        log_i("Passkey: %d", passkey);
      }

      if (BLEDevice::m_securityCallbacks != nullptr) {
        BLEDevice::m_securityCallbacks->onPassKeyNotify(passkey);
      }
#endif  // CONFIG_BLE_SMP_ENABLE
    } break;
    case ESP_GAP_BLE_KEY_EVT:
    {
      //shows the ble key type info share with peer device to the user.
      log_d("ESP_GAP_BLE_KEY_EVT");
#ifdef CONFIG_BLE_SMP_ENABLE  // Check that BLE SMP (security) is configured in make menuconfig
      log_i("key type = %s", BLESecurity::esp_key_type_to_str(param->ble_security.ble_key.key_type));
#endif  // CONFIG_BLE_SMP_ENABLE
    } break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
    {
      log_i("ESP_GAP_BLE_AUTH_CMPL_EVT");
#ifdef CONFIG_BLE_SMP_ENABLE  // Check that BLE SMP (security) is configured in make menuconfig
      if (BLEDevice::m_securityCallbacks != nullptr) {
        BLEDevice::m_securityCallbacks->onAuthenticationComplete(param->ble_security.auth_cmpl);
      }
#endif  // CONFIG_BLE_SMP_ENABLE
    } break;
    default:
    {
      break;
    }
  }  // switch

  if (BLEDevice::m_pClient != nullptr) {
    BLEDevice::m_pClient->handleGAPEvent(event, param);
  }

  if (BLEDevice::m_pScan != nullptr) {
    BLEDevice::getScan()->handleGAPEvent(event, param);
  }

  if (m_bleAdvertising != nullptr) {
    BLEDevice::getAdvertising()->handleGAPEvent(event, param);
  }

  if (m_customGapHandler != nullptr) {
    BLEDevice::m_customGapHandler(event, param);
  }

}  // gapEventHandler
void BLEDevice::setCustomGattcHandler(gattc_event_handler handler) {
  m_customGattcHandler = handler;
}

void BLEDevice::setCustomGattsHandler(gatts_event_handler handler) {
  m_customGattsHandler = handler;
}

#endif

/***************************************************************************
 *                            NimBLE functions                             *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)

/**
 * @brief Checks if a peer device is whitelisted.
 * @param [in] address The address to check for in the whitelist.
 * @returns True if the address is in the whitelist.
 */
bool BLEDevice::onWhiteList(BLEAddress &address) {
  for (auto &addr : m_whiteList) {
    if (addr == address) {
      return true;
    }
  }
  return false;
}

void BLEDevice::host_task(void *param) {
  log_i("NimBLE host task started");
  nimble_port_run();  // This function will return only when nimble_port_stop() is executed
  nimble_port_freertos_deinit();
}

void BLEDevice::onReset(int reason) {
  if (!m_synced) {
    return;
  }

  m_synced = false;

  log_i("onReset, reason=%d, %s", reason, BLEUtils::returnCodeToString(reason));

  if (initialized) {
    if (m_pScan != nullptr) {
      m_pScan->onHostReset();
    }
  }
}

void BLEDevice::onSync() {
  log_d("onSync");

  if (m_synced) {
    log_d("onSync: already synced");
    return;
  }

  int rc = ble_hs_util_ensure_addr(0);
  if (rc == 0) {
    rc = ble_hs_util_ensure_addr(1);
  }

  if (rc != 0) {
    log_e("onSync: failed to ensure BLE address. rc=%d", rc);
    return;
  }

  rc = ble_hs_id_copy_addr(BLE_OWN_ADDR_PUBLIC, NULL, NULL);
  if (rc != 0) {
    log_d("onSync: no public address available");
    m_ownAddrType = BLE_OWN_ADDR_RANDOM;
  }

  // Yield for housekeeping tasks before returning to operations.
  // Occasionally triggers exception without.
  ble_npl_time_delay(1);

  m_synced = true;

  if (initialized) {
    if (m_pScan != nullptr) {
      m_pScan->onHostSync();
    }
    if (m_bleAdvertising != nullptr) {
      m_bleAdvertising->onHostSync();
    }
  }
}

void BLEDevice::setDeviceCallbacks(BLEDeviceCallbacks *cb) {
  if (cb == nullptr) {
    m_pDeviceCallbacks = &defaultDeviceCallbacks;
  } else {
    m_pDeviceCallbacks = cb;
  }
}

/**
 * @brief Sets the address type to use.
 * @param [in] type Bluetooth Device address type.
 * The available types are defined as:
 * * 0x00: BLE_OWN_ADDR_PUBLIC - Public address; Uses the hardware static address.
 * * 0x01: BLE_OWN_ADDR_RANDOM - Random static address; Uses the hardware or generated random static address.
 * * 0x02: BLE_OWN_ADDR_RPA_PUBLIC_DEFAULT - Resolvable private address, defaults to public if no RPA available.
 * * 0x03: BLE_OWN_ADDR_RPA_RANDOM_DEFAULT - Resolvable private address, defaults to random static if no RPA available.
 */
bool BLEDevice::setOwnAddrType(uint8_t type) {
  int rc = ble_hs_id_copy_addr(type & 1, NULL, NULL);  // Odd values are random
  if (rc != 0) {
    log_e("Unable to set address type %d, rc=%d", type, rc);
    return false;
  }

  m_ownAddrType = type;

  if (type == BLE_OWN_ADDR_RPA_PUBLIC_DEFAULT || type == BLE_OWN_ADDR_RPA_RANDOM_DEFAULT) {
#ifdef CONFIG_IDF_TARGET_ESP32
    // esp32 controller does not support RPA so we must use the random static for calls to the stack
    // the host will take care of the random private address generation/setting.
    m_ownAddrType = BLE_OWN_ADDR_RANDOM;
    rc = ble_hs_pvcy_rpa_config(NIMBLE_HOST_ENABLE_RPA);
#endif
  } else {
#ifdef CONFIG_IDF_TARGET_ESP32
    rc = ble_hs_pvcy_rpa_config(NIMBLE_HOST_DISABLE_PRIVACY);
#endif
  }
  return rc == 0;
}  // setOwnAddrType

/**
 * @brief Set the device address to use.
 * @param [in] addr The address to set.
 * @return True if the address was set successfully.
 * @details To use the address generated the address type must be set to random with `setOwnAddrType`.
 */
bool BLEDevice::setOwnAddr(BLEAddress &addr) {
  return setOwnAddr(addr.getNative());
}  // setOwnAddr

/**
 * @brief Set the device address to use.
 * @param [in] addr The address to set.
 * @return True if the address was set successfully.
 * @details To use the address generated the address type must be set to random with `setOwnAddrType`.
 */
bool BLEDevice::setOwnAddr(uint8_t *addr) {
  int rc = ble_hs_id_set_rnd(addr);
  if (rc != 0) {
    log_e("Failed to set address, rc=%d", rc);
    return false;
  }
  return true;
}  // setOwnAddr

int BLEDeviceCallbacks::onStoreStatus(struct ble_store_status_event *event, void *arg) {
  log_d("onStoreStatus: default");
  return ble_store_util_status_rr(event, arg);
}

#endif

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
