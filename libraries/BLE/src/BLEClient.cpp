/*
 * BLEDevice.cpp
 *
 *  Created on: Mar 22, 2017
 *      Author: kolban
 */
#include "soc/soc_caps.h"
#if SOC_BLE_SUPPORTED

#include "sdkconfig.h"
#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_gattc_api.h>
#include <esp_gatt_common_api.h>  // ESP32 BLE
#include "BLEClient.h"
#include "BLEUtils.h"
#include "BLEService.h"
#include "GeneralUtils.h"
#include <string>
#include <sstream>
#include <unordered_set>
#include "BLEDevice.h"
#include "esp32-hal-log.h"

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
  m_pClientCallbacks = nullptr;
  m_conn_id = ESP_GATT_IF_NONE;
  m_gattc_if = ESP_GATT_IF_NONE;
  m_haveServices = false;
  m_isConnected = false;  // Initially, we are flagged as not connected.
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
  esp_ble_addr_type_t type = device->getAddressType();
  return connect(address, type);
}

/**
 * Add overloaded function to ease connect to peer device with not public address
 */
bool BLEClient::connectTimeout(BLEAdvertisedDevice *device, uint32_t timeoutMs) {
  BLEAddress address = device->getAddress();
  esp_ble_addr_type_t type = device->getAddressType();
  return connect(address, type, timeoutMs);
}

/**
 * @brief Connect to the partner (BLE Server).
 * @param [in] address The address of the partner.
 * @param [in] type The type of the address.
 * @param [in] timeoutMs The number of milliseconds to wait for the connection to complete.
 * @return True on success.
 */
bool BLEClient::connect(BLEAddress address, esp_ble_addr_type_t type, uint32_t timeoutMs) {
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
    *getPeerAddress().getNative(),  // address
    type,                           // Note: This was added on 2018-04-03 when the latest ESP-IDF was detected to have changed the signature.
    1                               // direct connection <-- maybe needs to be changed in case of direct indirect connection???
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
 * @return N/A.
 */
void BLEClient::disconnect() {
  log_v(">> disconnect()");
  esp_err_t errRc = ::esp_ble_gattc_close(getGattcIf(), getConnId());
  if (errRc != ESP_OK) {
    log_e("esp_ble_gattc_close: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
    return;
  }
  log_v("<< disconnect()");
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
#ifdef CONFIG_BLE_SMP_ENABLE  // Check that BLE SMP (security) is configured in make menuconfig
      if (BLEDevice::m_securityLevel) {
        esp_ble_set_encryption(evtParam->connect.remote_bda, BLEDevice::m_securityLevel);
      }
#endif  // CONFIG_BLE_SMP_ENABLE
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

uint16_t BLEClient::getConnId() {
  return m_conn_id;
}  // getConnId

esp_gatt_if_t BLEClient::getGattcIf() {
  return m_gattc_if;
}  // getGattcIf

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
  // We make the API call to read the RSSI value which is an asynchronous operation.  We expect to receive
  // an ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT to indicate completion.
  //
  m_semaphoreRssiCmplEvt.take("getRssi");
  esp_err_t rc = ::esp_ble_gap_read_rssi(*getPeerAddress().getNative());
  if (rc != ESP_OK) {
    log_e("<< getRssi: esp_ble_gap_read_rssi: rc=%d %s", rc, GeneralUtils::errorToString(rc));
    return 0;
  }
  int rssiValue = m_semaphoreRssiCmplEvt.wait("getRssi");
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
  return m_mtu;
}

/**
  @brief Set the local and remote MTU size.
         Should be called once after client connects if MTU size needs to be changed.
  @return bool indicating if MTU was successfully set locally and on remote.
*/
bool BLEClient::setMTU(uint16_t mtu) {
  esp_err_t err = esp_ble_gatt_set_local_mtu(mtu);  //First must set local MTU value.
  if (err == ESP_OK) {
    err = esp_ble_gattc_send_mtu_req(m_gattc_if, m_conn_id);  //Once local is set successfully set remote size
    if (err != ESP_OK) {
      log_e("Error setting send MTU request MTU: %d err=%d", mtu, err);
      return false;
    }
  } else {
    log_e("can't set local mtu value: %d", mtu);
    return false;
  }
  log_v("<< setLocalMTU");

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

#endif /* CONFIG_BLUEDROID_ENABLED */
#endif /* SOC_BLE_SUPPORTED */
