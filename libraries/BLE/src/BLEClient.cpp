/*
 * BLEDevice.cpp
 *
 *  Created on: Mar 22, 2017
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

#include "Arduino.h"
#if SOC_BLE_SUPPORTED
#include <esp_bt.h>
#endif
#include "BLEClient.h"
#include "BLEUtils.h"
#include "BLEService.h"
#include "GeneralUtils.h"
#include <string>
#include <sstream>
#include <unordered_set>
#include "BLEDevice.h"
#include "esp32-hal-log.h"
#include "BLESecurity.h"

/***************************************************************************
 *                           Bluedroid includes                            *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_gatt_common_api.h>
#endif

/***************************************************************************
 *                           NimBLE includes                               *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_hs.h>
#include <host/ble_gap.h>
#include <nimble/ble.h>
#include <nimble/nimble_npl_os.h>
#include <nimble/nimble_port.h>
#endif

/***************************************************************************
 *                           Common global variables                       *
 ***************************************************************************/

static BLEClientCallbacks defaultCallbacks;

/***************************************************************************
 *                           Common functions                              *
 ***************************************************************************/

/*
 * Design
 * ------
 * When we perform a searchService() requests, we are asking the BLE server to return each of the services
 * that it exposes.  For each service, we received an ESP_GATTC_SEARCH_RES_EVT event which contains details
 * of the exposed service including its UUID.
 *
 * The objects we will invent for a BLEClient will be as follows:
 * * BLERemoteService - A model of a remote service.
 * * BLERemoteCharacteristic - A model of a remote characteristic
 * * BLERemoteDescriptor - A model of a remote descriptor.
 *
 * Since there is a hierarchical relationship here, we will have the idea that from a BLERemoteService will own
 * zero or more remote characteristics and a BLERemoteCharacteristic will own zero or more remote BLEDescriptors.
 *
 * We will assume that a BLERemoteService contains a map that maps BLEUUIDs to the set of owned characteristics
 * and that a BLECharacteristic contains a map that maps BLEUUIDs to the set of owned descriptors.
 *
 *
 */

BLEClient::BLEClient() {
  m_pClientCallbacks = &defaultCallbacks;
  m_conn_id = ESP_GATT_IF_NONE;
  m_haveServices = false;
  m_isConnected = false;  // Initially, we are flagged as not connected.

#if defined(CONFIG_BLUEDROID_ENABLED)
  m_gattc_if = ESP_GATT_IF_NONE;
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  m_connectTimeout = 30000;
  m_pTaskData = nullptr;
  m_lastErr = 0;
  m_terminateFailCount = 0;

  m_pConnParams.scan_itvl = 16;                                             // Scan interval in 0.625ms units (NimBLE Default)
  m_pConnParams.scan_window = 16;                                           // Scan window in 0.625ms units (NimBLE Default)
  m_pConnParams.itvl_min = BLE_GAP_INITIAL_CONN_ITVL_MIN;                   // min_int = 0x10*1.25ms = 20ms
  m_pConnParams.itvl_max = BLE_GAP_INITIAL_CONN_ITVL_MAX;                   // max_int = 0x20*1.25ms = 40ms
  m_pConnParams.latency = BLE_GAP_INITIAL_CONN_LATENCY;                     // number of packets allowed to skip (extends max interval)
  m_pConnParams.supervision_timeout = BLE_GAP_INITIAL_SUPERVISION_TIMEOUT;  // timeout = 400*10ms = 4000ms
  m_pConnParams.min_ce_len = BLE_GAP_INITIAL_CONN_MIN_CE_LEN;               // Minimum length of connection event in 0.625ms units
  m_pConnParams.max_ce_len = BLE_GAP_INITIAL_CONN_MAX_CE_LEN;               // Maximum length of connection event in 0.625ms units
#endif
}  // BLEClient

/**
 * @brief Destructor.
 */
BLEClient::~BLEClient() {
  // We may have allocated service references associated with this client.  Before we are finished
  // with the client, we must release resources.
  for (auto &myPair : m_servicesMap) {
    delete myPair.second;
  }
  m_servicesMap.clear();
  m_servicesMapByInstID.clear();
}  // ~BLEClient

/**
 * @brief Clear any existing services.
 *
 */
void BLEClient::clearServices() {
  log_v(">> clearServices");
  // Delete all the services.
  for (auto &myPair : m_servicesMap) {
    delete myPair.second;
  }
  m_servicesMap.clear();
  m_haveServices = false;
  log_v("<< clearServices");
}  // clearServices

/**
 * Add overloaded function to ease connect to peer device with not public address
 */
bool BLEClient::connect(BLEAdvertisedDevice *device) {
  BLEAddress address = device->getAddress();
  uint8_t type = device->getAddressType();
  return connect(address, type);
}

/**
 * Add overloaded function to ease connect to peer device with not public address
 */
bool BLEClient::connectTimeout(BLEAdvertisedDevice *device, uint32_t timeoutMs) {
  BLEAddress address = device->getAddress();
  uint8_t type = device->getAddressType();
  return connect(address, type, timeoutMs);
}

esp_gatt_if_t BLEClient::getGattcIf() {
#if defined(CONFIG_BLUEDROID_ENABLED)
  return m_gattc_if;
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  log_e("NimBLE does not support getGattcIf()");
  return ESP_GATT_IF_NONE;
#endif
}  // getGattcIf

/**
 * @brief Initiate a secure connection (pair/bond) with the server.\n
 * Called automatically when a characteristic or descriptor requires encryption or authentication to access it.
 * @return True on success.
 */
bool BLEClient::secureConnection() {
#if defined(CONFIG_BLUEDROID_ENABLED)
  log_i("secureConnection() does not need to be called for Bluedroid");
  return true;
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  int retryCount = 1;
  BLETaskData taskData(const_cast<BLEClient *>(this), BLE_HS_ENOTCONN);
  m_pTaskData = &taskData;

  do {
    if (BLESecurity::startSecurity(m_conn_id)) {
      BLEUtils::taskWait(taskData, BLE_NPL_TIME_FOREVER);
    }

  } while (taskData.m_flags == (BLE_HS_ERR_HCI_BASE + BLE_ERR_PINKEY_MISSING) && retryCount--);

  m_pTaskData = nullptr;

  if (taskData.m_flags == 0) {
    log_d("<< secureConnection: success");
    return true;
  }

  m_lastErr = taskData.m_flags;
  log_e("secureConnection: failed rc=%d", taskData.m_flags);
  return false;
#endif
}  // secureConnection

uint16_t BLEClient::getConnId() {
  return m_conn_id;
}  // getConnId

/**
 * @brief Retrieve the address of the peer.
 *
 * Returns the Bluetooth device address of the %BLE peer to which this client is connected.
 */
BLEAddress BLEClient::getPeerAddress() {
  return m_peerAddress;
}  // getAddress

/**
 * @brief Ask the BLE server for the RSSI value.
 * @return The RSSI value.
 */
int BLEClient::getRssi() {
  log_v(">> getRssi()");
  if (!isConnected()) {
    log_v("<< getRssi(): Not connected");
    return 0;
  }

#if defined(CONFIG_BLUEDROID_ENABLED)
  // We make the API call to read the RSSI value which is an asynchronous operation.  We expect to receive
  // an ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT to indicate completion.
  //
  m_semaphoreRssiCmplEvt.take("getRssi");
  esp_err_t rc = ::esp_ble_gap_read_rssi(getPeerAddress().getNative());
  if (rc != ESP_OK) {
    log_e("<< getRssi: esp_ble_gap_read_rssi: rc=%d %s", rc, GeneralUtils::errorToString(rc));
    return 0;
  }
  int rssiValue = m_semaphoreRssiCmplEvt.wait("getRssi");
#endif  // CONFIG_BLUEDROID_ENABLED

#if defined(CONFIG_NIMBLE_ENABLED)
  int8_t rssiValue = 0;
  int rc = ble_gap_conn_rssi(m_conn_id, &rssiValue);
  if (rc != 0) {
    log_e("<< getRssi: ble_gap_conn_rssi: rc=%d %s", rc, BLEUtils::returnCodeToString(rc));
    return 0;
  }
#endif  // CONFIG_BLUEDROID_ENABLED
  log_v("<< getRssi(): %d", rssiValue);
  return rssiValue;
}  // getRssi

/**
 * @brief Get the service BLE Remote Service instance corresponding to the uuid.
 * @param [in] uuid The UUID of the service being sought.
 * @return A reference to the Service or nullptr if don't know about it.
 */
BLERemoteService *BLEClient::getService(const char *uuid) {
  return getService(BLEUUID(uuid));
}  // getService

/**
 * @brief Get the service object corresponding to the uuid.
 * @param [in] uuid The UUID of the service being sought.
 * @return A reference to the Service or nullptr if don't know about it.
 * @throws BLEUuidNotFound
 */
BLERemoteService *BLEClient::getService(BLEUUID uuid) {
  log_v(">> getService: uuid: %s", uuid.toString().c_str());
  // Design
  // ------
  // We wish to retrieve the service given its UUID.  It is possible that we have not yet asked the
  // device what services it has in which case we have nothing to match against.  If we have not
  // asked the device about its services, then we do that now.  Once we get the results we can then
  // examine the services map to see if it has the service we are looking for.
  if (!m_haveServices) {
    getServices();
  }
  std::string uuidStr = uuid.toString().c_str();
  for (auto &myPair : m_servicesMap) {
    if (myPair.first == uuidStr) {
      log_v("<< getService: found the service with uuid: %s", uuid.toString().c_str());
      return myPair.second;
    }
  }  // End of each of the services.
  log_v("<< getService: not found");
  return nullptr;
}  // getService

/**
 * @brief Get the value of a specific characteristic associated with a specific service.
 * @param [in] serviceUUID The service that owns the characteristic.
 * @param [in] characteristicUUID The characteristic whose value we wish to read.
 * @throws BLEUuidNotFound
 */
String BLEClient::getValue(BLEUUID serviceUUID, BLEUUID characteristicUUID) {
  log_v(">> getValue: serviceUUID: %s, characteristicUUID: %s", serviceUUID.toString().c_str(), characteristicUUID.toString().c_str());
  String ret = getService(serviceUUID)->getCharacteristic(characteristicUUID)->readValue();
  log_v("<<getValue");
  return ret;
}  // getValue

/**
 * @brief Are we connected to a partner?
 * @return True if we are connected and false if we are not connected.
 */
bool BLEClient::isConnected() {
  return m_isConnected;
}  // isConnected

/**
 * @brief Set the callbacks that will be invoked.
 */
void BLEClient::setClientCallbacks(BLEClientCallbacks *pClientCallbacks) {
  m_pClientCallbacks = pClientCallbacks;
}  // setClientCallbacks

/**
 * @brief Set the value of a specific characteristic associated with a specific service.
 * @param [in] serviceUUID The service that owns the characteristic.
 * @param [in] characteristicUUID The characteristic whose value we wish to write.
 * @throws BLEUuidNotFound
 */
void BLEClient::setValue(BLEUUID serviceUUID, BLEUUID characteristicUUID, String value) {
  log_v(">> setValue: serviceUUID: %s, characteristicUUID: %s", serviceUUID.toString().c_str(), characteristicUUID.toString().c_str());
  getService(serviceUUID)->getCharacteristic(characteristicUUID)->writeValue(value);
  log_v("<< setValue");
}  // setValue

uint16_t BLEClient::getMTU() {
#ifdef CONFIG_BLUEDROID_ENABLED
  return m_mtu;
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  return ble_att_mtu(m_conn_id);
#endif
}

/**
  @brief Set the local and remote MTU size.
         Should be called once after client connects if MTU size needs to be changed.
  @return bool indicating if MTU was successfully set locally and on remote.
*/
bool BLEClient::setMTU(uint16_t mtu) {
  log_v(">> setMTU: %d", mtu);
  esp_err_t err = ESP_OK;

#ifdef CONFIG_BLUEDROID_ENABLED
  err = esp_ble_gatt_set_local_mtu(mtu);  //First must set local MTU value.
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  err = ble_att_set_preferred_mtu(mtu);
#endif

  if (err == ESP_OK) {
#ifdef CONFIG_BLUEDROID_ENABLED
    err = esp_ble_gattc_send_mtu_req(m_gattc_if, m_conn_id);  //Once local is set successfully set remote size
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
    //err = ble_gattc_exchange_mtu(m_conn_id, nullptr, nullptr);
#endif

    if (err != ESP_OK) {
      log_e("Error setting send MTU request MTU: %d err=%d", mtu, err);
      return false;
    }
  } else {
    log_e("can't set local mtu value: %d", mtu);
    return false;
  }
  log_v("<< setMTU");

  m_mtu = mtu;  //successfully changed

  return true;
}

/**
 * @brief Return a string representation of this client.
 * @return A string representation of this client.
 */
String BLEClient::toString() {
  String res = "peer address: " + m_peerAddress.toString();
  res += "\nServices:\n";
  for (auto &myPair : m_servicesMap) {
    res += myPair.second->toString() + "\n";
    // myPair.second is the value
  }
  return res;
}  // toString

void BLEClientCallbacks::onConnect(BLEClient *pClient) {
  log_d("BLEClientCallbacks", "onConnect: default");
}

void BLEClientCallbacks::onDisconnect(BLEClient *pClient) {
  log_d("BLEClientCallbacks", "onDisconnect: default");
}

/***************************************************************************
 *                           Bluedroid functions                           *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)

/**
 * @brief Connect to the partner (BLE Server).
 * @param [in] address The address of the partner.
 * @param [in] type The type of the address.
 * @param [in] timeoutMs The number of milliseconds to wait for the connection to complete.
 * @return True on success.
 */
bool BLEClient::connect(BLEAddress address, uint8_t type, uint32_t timeoutMs) {
  log_v(">> connect(%s)", address.toString().c_str());

  // We need the connection handle that we get from registering the application.  We register the app
  // and then block on its completion.  When the event has arrived, we will have the handle.
  m_appId = BLEDevice::m_appId++;
  BLEDevice::addPeerDevice(this, true, m_appId);
  m_semaphoreRegEvt.take("connect");

  // clearServices(); // we dont need to delete services since every client is unique?
  esp_err_t errRc = ::esp_ble_gattc_app_register(m_appId);
  if (errRc != ESP_OK) {
    log_e("esp_ble_gattc_app_register: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
    BLEDevice::removePeerDevice(m_appId, true);
    return false;
  }

  uint32_t rc = m_semaphoreRegEvt.wait("connect");

  if (rc != ESP_GATT_OK) {
    // fixes ESP_GATT_NO_RESOURCES error mostly
    log_e("esp_ble_gattc_app_register_error: rc=%d", rc);
    BLEDevice::removePeerDevice(m_appId, true);
    // not sure if this is needed here
    // esp_ble_gattc_app_unregister(m_gattc_if);
    // m_gattc_if = ESP_GATT_IF_NONE;
    return false;
  }

  m_peerAddress = address;

  // Perform the open connection request against the target BLE Server.
  m_semaphoreOpenEvt.take("connect");
  errRc = ::esp_ble_gattc_open(
    m_gattc_if,
    getPeerAddress().getNative(),  // address
    (esp_ble_addr_type_t)type,     // Note: This was added on 2018-04-03 when the latest ESP-IDF was detected to have changed the signature.
    1                              // direct connection <-- maybe needs to be changed in case of direct indirect connection???
  );
  if (errRc != ESP_OK) {
    log_e("esp_ble_gattc_open: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
    BLEDevice::removePeerDevice(m_appId, true);
    return false;
  }

  bool got_sem = m_semaphoreOpenEvt.timedWait("connect", timeoutMs);  // Wait for the connection to complete.
  rc = m_semaphoreOpenEvt.value();
  // check the status of the connection and cleanup in case of failure
  if (!got_sem || rc != ESP_GATT_OK) {
    BLEDevice::removePeerDevice(m_appId, true);
    esp_ble_gattc_app_unregister(m_gattc_if);
    m_gattc_if = ESP_GATT_IF_NONE;
  }
  log_v("<< connect(), rc=%d", rc == ESP_GATT_OK);
  return rc == ESP_GATT_OK;
}  // connect

/**
 * @brief Disconnect from the peer.
 * @return error code from bluedroid, 0 = success.
 */
int BLEClient::disconnect(uint8_t reason) {
  log_v(">> disconnect()");
  esp_err_t errRc = ::esp_ble_gattc_close(getGattcIf(), getConnId());
  if (errRc != ESP_OK) {
    log_e("esp_ble_gattc_close: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
    return errRc;
  }
  log_v("<< disconnect()");
  return ESP_OK;
}  // disconnect

/**
 * @brief Handle GATT Client events
 */
void BLEClient::gattClientEventHandler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *evtParam) {

  log_d("gattClientEventHandler [esp_gatt_if: %d] ... %s", gattc_if, BLEUtils::gattClientEventTypeToString(event).c_str());

  // it is possible to receive events from other connections while waiting for registration
  if (m_gattc_if == ESP_GATT_IF_NONE && event != ESP_GATTC_REG_EVT) {
    return;
  }

  // Execute handler code based on the type of event received.
  switch (event) {

    case ESP_GATTC_SRVC_CHG_EVT: log_i("SERVICE CHANGED"); break;

    case ESP_GATTC_CLOSE_EVT:
    {
      // esp_ble_gattc_app_unregister(m_appId);
      // BLEDevice::removePeerDevice(m_gattc_if, true);
      break;
    }

    //
    // ESP_GATTC_DISCONNECT_EVT
    //
    // disconnect:
    // - esp_gatt_status_t status
    // - uint16_t          conn_id
    // - esp_bd_addr_t     remote_bda
    case ESP_GATTC_DISCONNECT_EVT:
    {
      if (evtParam->disconnect.conn_id != getConnId()) {
        break;
      }
      // If we receive a disconnect event, set the class flag that indicates that we are
      // no longer connected.
      bool m_wasConnected = m_isConnected;
      m_isConnected = false;
      esp_ble_gattc_app_unregister(m_gattc_if);
      m_gattc_if = ESP_GATT_IF_NONE;
      m_semaphoreOpenEvt.give(ESP_GATT_IF_NONE);
      m_semaphoreRssiCmplEvt.give();
      m_semaphoreSearchCmplEvt.give(1);
      BLEDevice::removePeerDevice(m_appId, true);
      // Reset security state on disconnect
      BLESecurity::resetSecurity();
      if (m_wasConnected && m_pClientCallbacks != nullptr) {
        m_pClientCallbacks->onDisconnect(this);
      }
      break;
    }  // ESP_GATTC_DISCONNECT_EVT

    //
    // ESP_GATTC_OPEN_EVT
    //
    // open:
    // - esp_gatt_status_t status
    // - uint16_t          conn_id
    // - esp_bd_addr_t     remote_bda
    //
    case ESP_GATTC_OPEN_EVT:
    {
      m_conn_id = evtParam->open.conn_id;
      if (evtParam->open.status == ESP_GATT_OK) {
        m_isConnected = true;  // Flag us as connected.
        if (m_pClientCallbacks != nullptr) {
          m_pClientCallbacks->onConnect(this);
        }
      } else {
        log_e("Failed to connect, status=%s", GeneralUtils::errorToString(evtParam->open.status));
      }
      m_semaphoreOpenEvt.give(evtParam->open.status);
      break;
    }  // ESP_GATTC_OPEN_EVT

    //
    // ESP_GATTC_REG_EVT
    //
    // reg:
    // esp_gatt_status_t status
    // uint16_t          app_id
    //
    case ESP_GATTC_REG_EVT:
    {
      m_gattc_if = gattc_if;
      // pass on the registration status result, in case of failure
      m_semaphoreRegEvt.give(evtParam->reg.status);
      break;
    }  // ESP_GATTC_REG_EVT

    case ESP_GATTC_CFG_MTU_EVT:
      if (evtParam->cfg_mtu.conn_id != getConnId()) {
        break;
      }
      if (evtParam->cfg_mtu.status != ESP_GATT_OK) {
        log_e("Config mtu failed");
      }
      m_mtu = evtParam->cfg_mtu.mtu;
      break;

    case ESP_GATTC_CONNECT_EVT:
    {
      if (evtParam->connect.conn_id != getConnId()) {
        break;
      }
      BLEDevice::updatePeerDevice(this, true, m_appId);
      esp_err_t errRc = esp_ble_gattc_send_mtu_req(gattc_if, evtParam->connect.conn_id);
      if (errRc != ESP_OK) {
        log_e("esp_ble_gattc_send_mtu_req: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
      }
      // Set encryption on connect for BlueDroid when security is enabled
      // This ensures security is established before any secure operations
      if (BLESecurity::m_securityEnabled && BLESecurity::m_forceSecurity) {
        BLESecurity::startSecurity(evtParam->connect.remote_bda);
      }
      break;
    }  // ESP_GATTC_CONNECT_EVT

    //
    // ESP_GATTC_SEARCH_CMPL_EVT
    //
    // search_cmpl:
    // - esp_gatt_status_t status
    // - uint16_t          conn_id
    //
    case ESP_GATTC_SEARCH_CMPL_EVT:
    {
      if (evtParam->search_cmpl.conn_id != getConnId()) {
        break;
      }
      esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)evtParam;
      if (p_data->search_cmpl.status != ESP_GATT_OK) {
        log_e("search service failed, error status = %x", p_data->search_cmpl.status);
        break;
      }
#ifndef ARDUINO_ARCH_ESP32
// commented out just for now to keep backward compatibility
// if(p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_REMOTE_DEVICE) {
//   log_i("Get service information from remote device");
// } else if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_NVS_FLASH) {
//   log_i("Get service information from flash");
// } else {
//   log_i("unknown service source");
// }
#endif
      m_semaphoreSearchCmplEvt.give(0);
      break;
    }  // ESP_GATTC_SEARCH_CMPL_EVT

    //
    // ESP_GATTC_SEARCH_RES_EVT
    //
    // search_res:
    // - uint16_t      conn_id
    // - uint16_t      start_handle
    // - uint16_t      end_handle
    // - esp_gatt_id_t srvc_id
    //
    case ESP_GATTC_SEARCH_RES_EVT:
    {
      if (evtParam->search_res.conn_id != getConnId()) {
        break;
      }
      BLEUUID uuid = BLEUUID(evtParam->search_res.srvc_id);
      BLERemoteService *pRemoteService =
        new BLERemoteService(evtParam->search_res.srvc_id, this, evtParam->search_res.start_handle, evtParam->search_res.end_handle);
      m_servicesMap.insert(std::pair<std::string, BLERemoteService *>(uuid.toString().c_str(), pRemoteService));
      m_servicesMapByInstID.insert(std::pair<BLERemoteService *, uint16_t>(pRemoteService, evtParam->search_res.srvc_id.inst_id));
      break;
    }  // ESP_GATTC_SEARCH_RES_EVT

    default:
    {
      break;
    }
  }  // Switch

  // Pass the request on to all services.
  for (auto &myPair : m_servicesMap) {
    myPair.second->gattClientEventHandler(event, gattc_if, evtParam);
  }

}  // gattClientEventHandler

/**
 * @brief Ask the remote %BLE server for its services.
 * A %BLE Server exposes a set of services for its partners.  Here we ask the server for its set of
 * services and wait until we have received them all.
 * @return N/A
 */
std::map<std::string, BLERemoteService *> *BLEClient::getServices() {
  /*
 * Design
 * ------
 * We invoke esp_ble_gattc_search_service.  This will request a list of the service exposed by the
 * peer BLE partner to be returned as events.  Each event will be an an instance of ESP_GATTC_SEARCH_RES_EVT
 * and will culminate with an ESP_GATTC_SEARCH_CMPL_EVT when all have been received.
 */
  log_v(">> getServices");
  // TODO implement retrieving services from cache
  clearServices();  // Clear any services that may exist.

  esp_err_t errRc = esp_ble_gattc_search_service(
    getGattcIf(), getConnId(),
    NULL  // Filter UUID
  );

  m_semaphoreSearchCmplEvt.take("getServices");
  if (errRc != ESP_OK) {
    log_e("esp_ble_gattc_search_service: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
    return &m_servicesMap;
  }
  // If successful, remember that we now have services.
  m_haveServices = (m_semaphoreSearchCmplEvt.wait("getServices") == 0);
  log_v("<< getServices");
  return &m_servicesMap;
}  // getServices

/**
 * @brief Handle a received GAP event.
 *
 * @param [in] event
 * @param [in] param
 */
void BLEClient::handleGAPEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  log_d("BLEClient ... handling GAP event!");
  switch (event) {
    //
    // ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT
    //
    // read_rssi_cmpl
    // - esp_bt_status_t status
    // - int8_t rssi
    // - esp_bd_addr_t remote_addr
    //
    case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT:
    {
      m_semaphoreRssiCmplEvt.give((uint32_t)param->read_rssi_cmpl.rssi);
      break;
    }  // ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT

    default: break;
  }
}  // handleGAPEvent

#endif  // CONFIG_BLUEDROID_ENABLED

/***************************************************************************
 *                           NimBLE functions                              *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)

/**
 * @brief If we have asked to disconnect and the event does not
 * occur within the supervision timeout + added delay, this will
 * be called to reset the host in the case of a stalled controller.
 */
void BLEClient::dcTimerCb(ble_npl_event *event) {
  ble_hs_sched_reset(BLE_HS_ECONTROLLER);
}

/**
 * @brief Connect to the partner (BLE Server).
 * @param [in] address The address of the partner.
 * @param [in] type The type of the address.
 * @param [in] timeoutMs The number of milliseconds to wait for the connection to complete.
 * @return True on success.
 */
bool BLEClient::connect(BLEAddress address, uint8_t type, uint32_t timeoutMs) {
  log_v(">> connect(%s)", address.toString().c_str());

  if (!BLEDevice::m_synced) {
    log_d("BLEClient", "Host reset, wait for sync.");
    return false;
  }

  if (m_conn_id != BLE_HS_CONN_HANDLE_NONE || m_isConnected || m_pTaskData != nullptr) {
    log_e("Client busy, connected to %s, id=%d", m_peerAddress.toString().c_str(), getConnId());
    return false;
  }

  ble_addr_t peerAddr_t;
  memcpy(&peerAddr_t.val, address.getNative(), 6);
  peerAddr_t.type = address.getType();
  if (ble_gap_conn_find_by_addr(&peerAddr_t, NULL) == 0) {
    log_e("A connection to %s already exists", address.toString().c_str());
    return false;
  }

  if (address == BLEAddress("")) {
    log_e("Invalid peer address (NULL)");
    return false;
  }

  m_appId = BLEDevice::m_appId++;
  m_peerAddress = address;

  int rc = 0;

  /* Try to connect the the advertiser.  Allow 30 seconds (30000 ms) for
   *  timeout (default value of m_connectTimeout).
   *  Loop on BLE_HS_EBUSY if the scan hasn't stopped yet.
   */
  do {
    rc = ble_gap_connect(BLEDevice::m_ownAddrType, &peerAddr_t, m_connectTimeout, &m_pConnParams, BLEClient::handleGAPEvent, this);
    switch (rc) {
      case 0: break;

      case BLE_HS_EBUSY:
        // Scan was still running, stop it and try again
        if (!BLEDevice::getScan()->stop()) {
          rc = BLE_HS_EUNKNOWN;
        }
        break;

      case BLE_HS_EDONE:
        // A connection to this device already exists, do not connect twice.
        log_e("Already connected to device; addr=%s", m_peerAddress.toString().c_str());
        break;

      case BLE_HS_EALREADY:
        // Already attempting to connect to this device, cancel the previous
        // attempt and report failure here so we don't get 2 connections.
        log_e("Already attempting to connect to %s - canceling", m_peerAddress.toString().c_str());
        ble_gap_conn_cancel();
        break;

      default: log_e("Failed to connect to %s, rc=%d; %s", m_peerAddress.toString().c_str(), rc, BLEUtils::returnCodeToString(rc)); break;
    }

  } while (rc == BLE_HS_EBUSY);

  if (rc != 0) {
    m_lastErr = rc;
    return false;
  }

  BLETaskData taskData(this);
  m_pTaskData = &taskData;

  // Wait for the connect timeout time +1 second for the connection to complete
  if (!BLEUtils::taskWait(taskData, m_connectTimeout + 1000)) {
    // If a connection was made but no response from MTU exchange proceed anyway
    if (isConnected()) {
      taskData.m_flags = 0;
    } else {
      // workaround; if the controller doesn't cancel the connection at the timeout, cancel it here.
      log_e("Connect timeout - canceling");
      ble_gap_conn_cancel();
      taskData.m_flags = BLE_HS_ETIMEOUT;
    }
  }

  m_pTaskData = nullptr;
  rc = taskData.m_flags;

  if (rc != 0) {
    log_e("Connection failed; status=%d %s", rc, BLEUtils::returnCodeToString(rc));
    m_lastErr = rc;
    return false;
  }

  m_isConnected = true;
  m_pClientCallbacks->onConnect(this);

  log_i("<< connect()");

  BLEDevice::addPeerDevice(this, true, m_appId);
  // Check if still connected before returning
  return isConnected();
}

/**
 * @brief STATIC Callback for the service discovery API function.\n
 * When a service is found or there is none left or there was an error
 * the API will call this and report findings.
 */
int BLEClient::serviceDiscoveredCB(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service, void *arg) {
  log_d("Service Discovered >> status: %d handle: %d", error->status, (error->status == 0) ? service->start_handle : -1);

  BLETaskData *pTaskData = (BLETaskData *)arg;
  BLEClient *client = (BLEClient *)pTaskData->m_pInstance;

  if (error->status == BLE_HS_ENOTCONN) {
    log_e("<< Service Discovered; Disconnected");
    BLEUtils::taskRelease(*pTaskData, error->status);
    return error->status;
  }

  // Make sure the service discovery is for this device
  if (client->getConnId() != conn_handle) {
    return 0;
  }

  if (error->status == 0) {
    // Found a service - add it to the vector
    BLERemoteService *pRemoteService = new BLERemoteService(client, service);
    client->m_servicesMap.insert(std::pair<std::string, BLERemoteService *>(pRemoteService->getUUID().toString().c_str(), pRemoteService));
    client->m_servicesMapByInstID.insert(std::pair<BLERemoteService *, uint16_t>(pRemoteService, service->start_handle));
    return 0;
  }

  BLEUtils::taskRelease(*pTaskData, error->status);
  log_d("<< Service Discovered");
  return error->status;
}

std::map<std::string, BLERemoteService *> *BLEClient::getServices() {
  log_v(">> getServices");

  clearServices();  // Clear any services that may exist.

  if (!isConnected()) {
    log_e("Disconnected, could not retrieve services -aborting");
    return &m_servicesMap;
  }

  int errRc = 0;
  BLETaskData taskData(this);

  errRc = ble_gattc_disc_all_svcs(m_conn_id, BLEClient::serviceDiscoveredCB, &taskData);

  if (errRc != 0) {
    log_e("ble_gattc_disc_all_svcs: rc=%d %s", errRc, BLEUtils::returnCodeToString(errRc));
    m_lastErr = errRc;
    return &m_servicesMap;
  }

  BLEUtils::taskWait(taskData, BLE_NPL_TIME_FOREVER);
  errRc = taskData.m_flags;
  if (errRc == 0 || errRc == BLE_HS_EDONE) {
    // If successful, remember that we now have services.
    m_haveServices = m_servicesMap.size() > 0;
    log_v("<< getServices");
    return &m_servicesMap;
  }

  m_lastErr = errRc;
  log_e("Could not retrieve services, rc=%d %s", errRc, BLEUtils::returnCodeToString(errRc));
  return &m_servicesMap;
}  // getServices

int BLEClient::handleGAPEvent(struct ble_gap_event *event, void *arg) {
  BLEClient *client = (BLEClient *)arg;
  int rc = 0;
  BLETaskData *pTaskData = client->m_pTaskData;

  log_d("BLEClient", "Got Client event %s", BLEUtils::gapEventToString(event->type));

  switch (event->type) {
    case BLE_GAP_EVENT_DISCONNECT:
    {
      rc = event->disconnect.reason;
      // If Host reset tell the device now before returning to prevent
      // any errors caused by calling host functions before resyncing.
      switch (rc) {
        case BLE_HS_ECONTROLLER:
        case BLE_HS_ETIMEOUT_HCI:
        case BLE_HS_ENOTSYNCED:
        case BLE_HS_EOS:
          log_e("BLEClient", "Disconnect - host reset, rc=%d", rc);
          BLEDevice::onReset(rc);
          break;
        default:
          // Check that the event is for this client.
          if (client->m_conn_id != event->disconnect.conn.conn_handle) {
            return 0;
          }
          break;
      }

      // Remove the device from ignore list so we will scan it again
      // BLEDevice::removeIgnored(client->m_peerAddress);

      // No longer connected, clear the connection ID.
      client->m_conn_id = BLE_HS_CONN_HANDLE_NONE;
      client->m_terminateFailCount = 0;

      // If we received a connected event but did not get established (no PDU)
      // then a disconnect event will be sent but we should not send it to the
      // app for processing. Instead we will ensure the task is released
      // and report the error.
      if (!client->m_isConnected) {
        break;
      }

      log_i("BLEClient", "disconnect; reason=%d, %s", rc, BLEUtils::returnCodeToString(rc));

      BLEDevice::removePeerDevice(client->m_appId, true);
      client->m_isConnected = false;
      // Reset security state on disconnect
      BLESecurity::resetSecurity();
      if (client->m_pClientCallbacks != nullptr) {
        client->m_pClientCallbacks->onDisconnect(client);
      }
      break;
    }  // BLE_GAP_EVENT_DISCONNECT

    case BLE_GAP_EVENT_CONNECT:
    {
      // If we aren't waiting for this connection response
      // we should drop the connection immediately.
      if (client->isConnected() || client->m_pTaskData == nullptr) {
        ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        return 0;
      }

      rc = event->connect.status;
      if (rc == 0) {
        log_i("BLEClient: Connected event. Handle: %d", event->connect.conn_handle);

        client->m_conn_id = event->connect.conn_handle;

        rc = ble_gattc_exchange_mtu(client->m_conn_id, nullptr, nullptr);
        if (rc != 0) {
          log_e("BLEClient", "MTU exchange error; rc=%d %s", rc, BLEUtils::returnCodeToString(rc));
          break;
        }

        if (BLESecurity::m_securityEnabled) {
          BLESecurity::startSecurity(client->m_conn_id);
        }

        // In the case of a multiconnecting device we ignore this device when
        // scanning since we are already connected to it
        // BLEDevice::addIgnored(client->m_peerAddress);
      } else {
        client->m_conn_id = BLE_HS_CONN_HANDLE_NONE;
        break;
      }

      return 0;
    }  // BLE_GAP_EVENT_CONNECT

    case BLE_GAP_EVENT_TERM_FAILURE:
    {
      if (client->m_conn_id != event->term_failure.conn_handle) {
        return 0;
      }

      log_e("Connection termination failure; rc=%d - retrying", event->term_failure.status);
      if (++client->m_terminateFailCount > 2) {
        ble_hs_sched_reset(BLE_HS_ECONTROLLER);
      } else {
        ble_gap_terminate(event->term_failure.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
      }
      return 0;
    }  // BLE_GAP_EVENT_TERM_FAILURE

    case BLE_GAP_EVENT_NOTIFY_RX:
    {
      if (client->m_conn_id != event->notify_rx.conn_handle) {
        return 0;
      }

      // If a notification comes before this flag is set we might
      // access a vector while it is being cleared in connect()
      if (!client->m_isConnected) {
        return 0;
      }

      log_d("BLEClient", "Notify received for handle: %d", event->notify_rx.attr_handle);

      for (auto &myPair : client->m_servicesMap) {
        // Dont waste cycles searching services without this handle in its range
        if (myPair.second->getEndHandle() < event->notify_rx.attr_handle) {
          continue;
        }

        auto cMap = &myPair.second->m_characteristicMap;
        log_d("BLEClient", "checking service %s for handle: %d", myPair.second->getUUID().toString().c_str(), event->notify_rx.attr_handle);

        auto characteristic = cMap->cbegin();
        for (; characteristic != cMap->cend(); ++characteristic) {
          if (characteristic->second->m_handle == event->notify_rx.attr_handle) {
            break;
          }
        }

        if (characteristic != cMap->cend()) {
          log_d("BLEClient", "Got Notification for characteristic %s", characteristic->second->toString().c_str());

          characteristic->second->m_semaphoreReadCharEvt.take();
          characteristic->second->m_value = String((char *)event->notify_rx.om->om_data, event->notify_rx.om->om_len);
          characteristic->second->m_semaphoreReadCharEvt.give();

          if (characteristic->second->m_notifyCallback != nullptr) {
            log_d("Invoking callback for notification on characteristic %s", characteristic->second->toString().c_str());
            characteristic->second->m_notifyCallback(
              characteristic->second, event->notify_rx.om->om_data, event->notify_rx.om->om_len, !event->notify_rx.indication
            );
          }
          break;
        }
      }

      return 0;
    }  // BLE_GAP_EVENT_NOTIFY_RX

    case BLE_GAP_EVENT_CONN_UPDATE_REQ:
    case BLE_GAP_EVENT_L2CAP_UPDATE_REQ:
    {
      if (client->m_conn_id != event->conn_update_req.conn_handle) {
        return 0;
      }
      log_d("Peer requesting to update connection parameters");
      log_d(
        "MinInterval: %d, MaxInterval: %d, Latency: %d, Timeout: %d", event->conn_update_req.peer_params->itvl_min,
        event->conn_update_req.peer_params->itvl_max, event->conn_update_req.peer_params->latency, event->conn_update_req.peer_params->supervision_timeout
      );

      rc = client->m_pClientCallbacks->onConnParamsUpdateRequest(client, event->conn_update_req.peer_params) ? 0 : BLE_ERR_CONN_PARMS;

      if (!rc && event->type == BLE_GAP_EVENT_CONN_UPDATE_REQ) {
        event->conn_update_req.self_params->itvl_min = client->m_pConnParams.itvl_min;
        event->conn_update_req.self_params->itvl_max = client->m_pConnParams.itvl_max;
        event->conn_update_req.self_params->latency = client->m_pConnParams.latency;
        event->conn_update_req.self_params->supervision_timeout = client->m_pConnParams.supervision_timeout;
      }

      log_d("%s peer params", (rc == 0) ? "Accepted" : "Rejected");
      return rc;
    }  // BLE_GAP_EVENT_CONN_UPDATE_REQ, BLE_GAP_EVENT_L2CAP_UPDATE_REQ

    case BLE_GAP_EVENT_CONN_UPDATE:
    {
      if (client->m_conn_id != event->conn_update.conn_handle) {
        return 0;
      }
      if (event->conn_update.status == 0) {
        log_i("Connection parameters updated.");
      } else {
        log_e("Update connection parameters failed.");
      }
      return 0;
    }  // BLE_GAP_EVENT_CONN_UPDATE

    case BLE_GAP_EVENT_ENC_CHANGE:
    {
      if (client->m_conn_id != event->enc_change.conn_handle) {
        return 0;
      }

      if (event->enc_change.status == 0 || event->enc_change.status == (BLE_HS_ERR_HCI_BASE + BLE_ERR_PINKEY_MISSING)) {
        struct ble_gap_conn_desc desc;
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);

        if (event->enc_change.status == (BLE_HS_ERR_HCI_BASE + BLE_ERR_PINKEY_MISSING)) {
          // Key is missing, try deleting.
          ble_store_util_delete_peer(&desc.peer_id_addr);
        } else if (BLEDevice::m_securityCallbacks != nullptr) {
          BLEDevice::m_securityCallbacks->onAuthenticationComplete(&desc);
        }
      }

      rc = event->enc_change.status;
      break;
    }  //BLE_GAP_EVENT_ENC_CHANGE

    case BLE_GAP_EVENT_MTU:
    {
      if (client->m_conn_id != event->mtu.conn_handle) {
        return 0;
      }
      log_i("mtu update event; conn_handle=%d mtu=%d", event->mtu.conn_handle, event->mtu.value);
      rc = 0;
      break;
    }  // BLE_GAP_EVENT_MTU

    case BLE_GAP_EVENT_PASSKEY_ACTION:
    {
      struct ble_sm_io pkey = {0, 0};

      if (client->m_conn_id != event->passkey.conn_handle) {
        return 0;
      }

      if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
        // Display the passkey on this device
        log_d("BLE_SM_IOACT_DISP");

        pkey.action = event->passkey.params.action;
        pkey.passkey = BLESecurity::getPassKey();  // This is the passkey to be entered on peer

        if (!BLESecurity::m_passkeySet) {
          log_w("No passkey set");
        }

        if (BLESecurity::m_staticPasskey && pkey.passkey == BLE_SM_DEFAULT_PASSKEY) {
          log_w("*ATTENTION* Using default passkey: %06d", BLE_SM_DEFAULT_PASSKEY);
          log_w("*ATTENTION* Please use a random passkey or set a different static passkey");
        } else {
          log_i("Passkey: %d", pkey.passkey);
        }

        if (BLEDevice::m_securityCallbacks != nullptr) {
          BLEDevice::m_securityCallbacks->onPassKeyNotify(pkey.passkey);
        }

        rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
        log_d("ble_sm_inject_io result: %d", rc);

      } else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
        // Check if the passkey on the peer device is correct
        log_d("BLE_SM_IOACT_NUMCMP");

        log_d("Passkey on device's display: %d", event->passkey.params.numcmp);
        pkey.action = event->passkey.params.action;

        if (BLEDevice::m_securityCallbacks != nullptr) {
          pkey.numcmp_accept = BLEDevice::m_securityCallbacks->onConfirmPIN(event->passkey.params.numcmp);
        } else {
          log_e("onConfirmPIN not implemented. Rejecting connection");
          pkey.numcmp_accept = 0;
        }

        rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
        log_d("ble_sm_inject_io result: %d", rc);

      } else if (event->passkey.params.action == BLE_SM_IOACT_OOB) {
        // Out of band pairing
        // TODO: Handle out of band pairing
        log_w("BLE_SM_IOACT_OOB: Not implemented");

        static uint8_t tem_oob[16] = {0};
        pkey.action = event->passkey.params.action;
        for (int i = 0; i < 16; i++) {
          pkey.oob[i] = tem_oob[i];
        }
        rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
        log_d("ble_sm_inject_io result: %d", rc);
      } else if (event->passkey.params.action == BLE_SM_IOACT_INPUT) {
        // Input passkey from peer device
        log_d("BLE_SM_IOACT_INPUT");

        pkey.action = event->passkey.params.action;
        pkey.passkey = BLESecurity::getPassKey();

        if (!BLESecurity::m_passkeySet) {
          if (BLEDevice::m_securityCallbacks != nullptr) {
            log_i("No passkey set, getting passkey from onPassKeyRequest");
            pkey.passkey = BLEDevice::m_securityCallbacks->onPassKeyRequest();
          } else {
            log_w("*ATTENTION* onPassKeyRequest not implemented and no static passkey set.");
          }
        }

        if (BLESecurity::m_staticPasskey && pkey.passkey == BLE_SM_DEFAULT_PASSKEY) {
          log_w("*ATTENTION* Using default passkey: %06d", BLE_SM_DEFAULT_PASSKEY);
          log_w("*ATTENTION* Please use a random passkey or set a different static passkey");
        } else {
          log_i("Passkey: %d", pkey.passkey);
        }

        rc = ble_sm_inject_io(event->passkey.conn_handle, &pkey);
        log_d("ble_sm_inject_io result: %d", rc);

      } else if (event->passkey.params.action == BLE_SM_IOACT_NONE) {
        log_d("BLE_SM_IOACT_NONE");
        log_i("No passkey action required");
      }

      return 0;
    }  // BLE_GAP_EVENT_PASSKEY_ACTION

    default:
    {
      return 0;
    }
  }  // Switch

  if (pTaskData != nullptr) {
    BLEUtils::taskRelease(*pTaskData, rc);
  }

  return 0;
}  // handleGAPEvent

/**
 * @brief Disconnect from the peer.
 * @return Error code from NimBLE stack, 0 = success.
 */
int BLEClient::disconnect(uint8_t reason) {
  log_d(">> disconnect()");
  int rc = 0;

  if (isConnected()) {
    rc = ble_gap_terminate(m_conn_id, reason);
    if (rc != 0 && rc != BLE_HS_ENOTCONN && rc != BLE_HS_EALREADY) {
      m_lastErr = rc;
      log_e("ble_gap_terminate failed: rc=%d %s", rc, BLEUtils::returnCodeToString(rc));
    }
  } else {
    log_d("Not connected to any peers");
  }

  log_d("<< disconnect()");
  return rc;
}  // disconnect

bool BLEClientCallbacks::onConnParamsUpdateRequest(BLEClient *pClient, const ble_gap_upd_params *params) {
  log_d("BLEClientCallbacks", "onConnParamsUpdateRequest: default");
  return true;
}

#endif  // CONFIG_NIMBLE_ENABLED

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
