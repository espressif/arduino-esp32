/*
 * BLEServer.cpp
 *
 *  Created on: Apr 16, 2017
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
 *                       Common includes                                   *
 ***************************************************************************/

#if SOC_BLE_SUPPORTED
#include <esp_bt.h>
#endif
#include "GeneralUtils.h"
#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEService.h"
#include "BLEUtils.h"
#include <string.h>
#include <string>
#include <unordered_set>
#include "esp32-hal-log.h"

/***************************************************************************
 *                       Bluedroid includes                                *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)
#include <esp_bt_main.h>
#endif

/***************************************************************************
 *                       NimBLE includes                                   *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#endif

/***************************************************************************
 *                        Common functions                                 *
 ***************************************************************************/

/**
 * @brief Construct a %BLE Server
 *
 * This class is not designed to be individually instantiated.  Instead one should create a server by asking
 * the BLEDevice class.
 */
BLEServer::BLEServer() {
#ifdef CONFIG_BLUEDROID_ENABLED
  m_gatts_if = ESP_GATT_IF_NONE;
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  memset(m_indWait, BLE_HS_CONN_HANDLE_NONE, sizeof(m_indWait));
  m_svcChanged = false;
#endif

#if !defined(CONFIG_BT_NIMBLE_EXT_ADV) || defined(CONFIG_BLUEDROID_ENABLED)
  m_advertiseOnDisconnect = false;
#endif

  m_appId = ESP_GATT_IF_NONE;
  m_gattsStarted = false;
  m_connectedCount = 0;
  m_connId = ESP_GATT_IF_NONE;
  m_pServerCallbacks = nullptr;
}  // BLEServer

void BLEServer::createApp(uint16_t appId) {
  m_appId = appId;
#ifdef CONFIG_BLUEDROID_ENABLED
  registerApp(appId);
#endif
}  // createApp

/**
 * @brief Create a %BLE Service.
 *
 * With a %BLE server, we can host one or more services.  Invoking this function causes the creation of a definition
 * of a new service.  Every service must have a unique UUID.
 * @param [in] uuid The UUID of the new service.
 * @return A reference to the new service object.
 */
BLEService *BLEServer::createService(const char *uuid) {
  return createService(BLEUUID(uuid));
}

/**
 * @brief Create a %BLE Service.
 *
 * With a %BLE server, we can host one or more services.  Invoking this function causes the creation of a definition
 * of a new service.  Every service must have a unique UUID.
 * @param [in] uuid The UUID of the new service.
 * @param [in] numHandles The maximum number of handles associated with this service.
 * @param [in] inst_id With multiple services with the same UUID we need to provide inst_id value different for each service.
 * @return A reference to the new service object.
 */
BLEService *BLEServer::createService(BLEUUID uuid, uint32_t numHandles, uint8_t inst_id) {
  log_v(">> createService - %s", uuid.toString().c_str());
#ifdef CONFIG_BLUEDROID_ENABLED
  m_semaphoreCreateEvt.take("createService");
#endif

  // Check that a service with the supplied UUID does not already exist.
  if (m_serviceMap.getByUUID(uuid) != nullptr) {
    log_w("<< Attempt to create a new service with uuid %s but a service with that UUID already exists.", uuid.toString().c_str());
  }

  BLEService *pService = new BLEService(uuid, numHandles);
  pService->m_instId = inst_id;
  m_serviceMap.setByUUID(uuid, pService);  // Save a reference to this service being on this server.
  pService->executeCreate(this);           // Perform the API calls to actually create the service.

#ifdef CONFIG_BLUEDROID_ENABLED
  m_semaphoreCreateEvt.wait("createService");
#endif

#ifdef CONFIG_NIMBLE_ENABLED
  m_semaphoreCreateEvt.give();
  serviceChanged();
#endif

  log_v("<< createService");
  return pService;
}  // createService

/**
 * @brief Get a %BLE Service by its UUID
 * @param [in] uuid The UUID of the new service.
 * @return A reference to the service object.
 */
BLEService *BLEServer::getServiceByUUID(const char *uuid) {
  return m_serviceMap.getByUUID(uuid);
}

/**
 * @brief Get a %BLE Service by its UUID
 * @param [in] uuid The UUID of the new service.
 * @return A reference to the service object.
 */
BLEService *BLEServer::getServiceByUUID(BLEUUID uuid) {
  return m_serviceMap.getByUUID(uuid);
}

/**
 * @brief Retrieve the advertising object that can be used to advertise the existence of the server.
 *
 * @return An advertising object.
 */
BLEAdvertising *BLEServer::getAdvertising() {
  return BLEDevice::getAdvertising();
}

uint16_t BLEServer::getConnId() {
  return m_connId;
}

/**
 * @brief Return the number of connected clients.
 * @return The number of connected clients.
 */
uint32_t BLEServer::getConnectedCount() {
  return m_connectedCount;
}  // getConnectedCount

void BLEServer::start() {
  if (m_gattsStarted) {
    return;
  }

#ifdef CONFIG_NIMBLE_ENABLED
  int rc = ble_gatts_start();
  if (rc != 0) {
    log_e("ble_gatts_start; rc=%d, %s", rc, BLEUtils::returnCodeToString(rc));
    return;
  }

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
  ble_gatts_show_local();
#endif

  BLEService *svc = m_serviceMap.getFirst();
  while (svc != nullptr) {
    if (svc->m_removed == 0) {
      rc = ble_gatts_find_svc(&svc->getUUID().getNative()->u, &svc->m_handle);
      if (rc != 0) {
        abort();
      }
    }

    BLECharacteristic *chr = svc->m_characteristicMap.getFirst();
    while (chr != nullptr) {
      if ((chr->m_properties & BLE_GATT_CHR_F_INDICATE) || (chr->m_properties & BLE_GATT_CHR_F_NOTIFY)) {
        m_notifyChrVec.push_back(chr);
      }
      chr = svc->m_characteristicMap.getNext();
    }

    svc = m_serviceMap.getNext();
  }

#endif

  m_gattsStarted = true;
}

/**
 * @brief Set the server callbacks.
 *
 * As a %BLE server operates, it will generate server level events such as a new client connecting or a previous client
 * disconnecting.  This function can be called to register a callback handler that will be invoked when these
 * events are detected.
 *
 * @param [in] pCallbacks The callbacks to be invoked.
 */
void BLEServer::setCallbacks(BLEServerCallbacks *pCallbacks) {
  m_pServerCallbacks = pCallbacks;
}  // setCallbacks

/*
 * Remove service
 */
void BLEServer::removeService(BLEService *service) {
#if defined(CONFIG_BLUEDROID_ENABLED)
  service->stop();
  service->executeDelete();
  m_serviceMap.removeService(service);
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  if (service->m_removed == 0) {
    int rc = ble_gatts_svc_set_visibility(service->getHandle(), 0);
    if (rc != 0) {
      return;
    }
    service->m_removed = NIMBLE_ATT_REMOVE_DELETE;
    serviceChanged();
    m_serviceMap.removeService(service);
    BLEDevice::getAdvertising()->removeServiceUUID(service->getUUID());
  }
#endif
}

/**
 * @brief Start advertising.
 *
 * Start the server advertising its existence.  This is a convenience function and is equivalent to
 * retrieving the advertising object and invoking start upon it.
 */
void BLEServer::startAdvertising() {
  log_v(">> startAdvertising");
  BLEDevice::startAdvertising();
  log_v("<< startAdvertising");
}  // startAdvertising

/* multi connect support */
/* TODO do some more tweaks */
void BLEServer::updatePeerMTU(uint16_t conn_id, uint16_t mtu) {
  // set mtu in conn_status_t
  const std::map<uint16_t, conn_status_t>::iterator it = m_connectedServersMap.find(conn_id);
  if (it != m_connectedServersMap.end()) {
    it->second.mtu = mtu;
    std::swap(m_connectedServersMap[conn_id], it->second);
  }
}

std::map<uint16_t, conn_status_t> BLEServer::getPeerDevices(bool _client) {
  return m_connectedServersMap;
}

uint16_t BLEServer::getPeerMTU(uint16_t conn_id) {
#if defined(CONFIG_BLUEDROID_ENABLED)
  return m_connectedServersMap.find(conn_id)->second.mtu;
#endif

#if defined(CONFIG_NIMBLE_ENABLED)
  return ble_att_mtu(conn_id);
#endif
}

void BLEServer::addPeerDevice(void *peer, bool _client, uint16_t conn_id) {
  conn_status_t status = {.peer_device = peer, .connected = true, .mtu = 23};

  m_connectedServersMap.insert(std::pair<uint16_t, conn_status_t>(conn_id, status));
}

bool BLEServer::removePeerDevice(uint16_t conn_id, bool _client) {
  return m_connectedServersMap.erase(conn_id) > 0;
}

#if !defined(CONFIG_BT_NIMBLE_EXT_ADV) || defined(CONFIG_BLUEDROID_ENABLED)
void BLEServer::advertiseOnDisconnect(bool enable) {
  m_advertiseOnDisconnect = enable;
}
#endif

void BLEServerCallbacks::onConnect(BLEServer *pServer) {
  log_d("BLEServerCallbacks", ">> onConnect(): Default");
  log_d("BLEServerCallbacks", "Device: %s", BLEDevice::toString().c_str());
  log_d("BLEServerCallbacks", "<< onConnect()");
}  // onConnect

void BLEServerCallbacks::onDisconnect(BLEServer *pServer) {
  log_d("BLEServerCallbacks", ">> onDisconnect(): Default");
  log_d("BLEServerCallbacks", "Device: %s", BLEDevice::toString().c_str());
  log_d("BLEServerCallbacks", "<< onDisconnect()");
}  // onDisconnect

/***************************************************************************
 *                       Bluedroid functions                               *
 ***************************************************************************/

#if defined(CONFIG_BLUEDROID_ENABLED)

/**
 * Allow to connect GATT server to peer device
 * Probably can be used in ANCS for iPhone
 */
bool BLEServer::connect(BLEAddress address) {
  esp_bd_addr_t addr;
  memcpy(&addr, address.getNative(), 6);
  // Perform the open connection request against the target BLE Server.
  m_semaphoreOpenEvt.take("connect");
  esp_err_t errRc = ::esp_ble_gatts_open(
    getGattsIf(),
    addr,  // address
    1      // direct connection
  );
  if (errRc != ESP_OK) {
    log_e("esp_ble_gattc_open: rc=%d %s", errRc, GeneralUtils::errorToString(errRc));
    return false;
  }

  uint32_t rc = m_semaphoreOpenEvt.wait("connect");  // Wait for the connection to complete.
  log_v("<< connect(), rc=%d", rc == ESP_GATT_OK);
  return rc == ESP_GATT_OK;
}  // connect

/**
 * Update connection parameters can be called only after connection has been established
 */
void BLEServer::updateConnParams(esp_bd_addr_t remote_bda, uint16_t minInterval, uint16_t maxInterval, uint16_t latency, uint16_t timeout) {
  esp_ble_conn_update_params_t conn_params;
  memcpy(conn_params.bda, remote_bda, sizeof(esp_bd_addr_t));
  conn_params.latency = latency;
  conn_params.max_int = maxInterval;  // max_int = 0x20*1.25ms = 40ms
  conn_params.min_int = minInterval;  // min_int = 0x10*1.25ms = 20ms
  conn_params.timeout = timeout;      // timeout = 400*10ms = 4000ms
  esp_ble_gap_update_conn_params(&conn_params);
}

void BLEServer::disconnect(uint16_t connId) {
  esp_ble_gatts_close(m_gatts_if, connId);
}

uint16_t BLEServer::getGattsIf() {
  return m_gatts_if;
}

/**
 * @brief Handle a GATT Server Event.
 *
 * @param [in] event
 * @param [in] gatts_if
 * @param [in] param
 *
 */
void BLEServer::handleGATTServerEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
  log_v(">> handleGATTServerEvent: %s", BLEUtils::gattServerEventTypeToString(event).c_str());

  switch (event) {
    // ESP_GATTS_ADD_CHAR_EVT - Indicate that a characteristic was added to the service.
    // add_char:
    // - esp_gatt_status_t status
    // - uint16_t          attr_handle
    // - uint16_t          service_handle
    // - esp_bt_uuid_t     char_uuid
    //
    case ESP_GATTS_ADD_CHAR_EVT:
    {
      break;
    }  // ESP_GATTS_ADD_CHAR_EVT

    case ESP_GATTS_MTU_EVT:
      updatePeerMTU(param->mtu.conn_id, param->mtu.mtu);
      if (m_pServerCallbacks != nullptr) {
        m_pServerCallbacks->onMtuChanged(this, param);
      }
      break;

    // ESP_GATTS_CONNECT_EVT
    // connect:
    // - uint16_t      conn_id
    // - esp_bd_addr_t remote_bda
    //
    case ESP_GATTS_CONNECT_EVT:
    {
      m_connId = param->connect.conn_id;
      addPeerDevice((void *)this, false, m_connId);
      if (m_pServerCallbacks != nullptr) {
        m_pServerCallbacks->onConnect(this);
        m_pServerCallbacks->onConnect(this, param);
      }
      m_connectedCount++;  // Increment the number of connected devices count.
      break;
    }  // ESP_GATTS_CONNECT_EVT

    // ESP_GATTS_CREATE_EVT
    // Called when a new service is registered as having been created.
    //
    // create:
    // * esp_gatt_status_t  status
    // * uint16_t           service_handle
    // * esp_gatt_srvc_id_t service_id
    //
    case ESP_GATTS_CREATE_EVT:
    {
      BLEService *pService = m_serviceMap.getByUUID(
        param->create.service_id.id.uuid, param->create.service_id.id.inst_id
      );  // <--- very big bug for multi services with the same uuid
      m_serviceMap.setByHandle(param->create.service_handle, pService);
      m_semaphoreCreateEvt.give();
      break;
    }  // ESP_GATTS_CREATE_EVT

    // ESP_GATTS_DISCONNECT_EVT
    //
    // disconnect
    // - uint16_t      					conn_id
    // - esp_bd_addr_t 					remote_bda
    // - esp_gatt_conn_reason_t         reason
    //
    // If we receive a disconnect event then invoke the callback for disconnects (if one is present).
    // we also want to start advertising again.
    case ESP_GATTS_DISCONNECT_EVT:
    {
      if (m_pServerCallbacks != nullptr) {  // If we have callbacks, call now.
        m_pServerCallbacks->onDisconnect(this);
        m_pServerCallbacks->onDisconnect(this, param);
      }
      if (m_connId == ESP_GATT_IF_NONE) {
        return;
      }

      // only decrement if connection is found in map and removed
      // sometimes this event triggers w/o a valid connection
      if (removePeerDevice(param->disconnect.conn_id, false)) {
        m_connectedCount--;  // Decrement the number of connected devices count.
      }

      // Reset security state on disconnect
      BLESecurity::resetSecurity();

      // Start advertising again if enabled
      if (m_advertiseOnDisconnect) {
        log_i("Start advertising again after disconnect");
        startAdvertising();
      }

      break;
    }  // ESP_GATTS_DISCONNECT_EVT

    // ESP_GATTS_READ_EVT - A request to read the value of a characteristic has arrived.
    //
    // read:
    // - uint16_t      conn_id
    // - uint32_t      trans_id
    // - esp_bd_addr_t bda
    // - uint16_t      handle
    // - uint16_t      offset
    // - bool          is_long
    // - bool          need_rsp
    //
    case ESP_GATTS_READ_EVT:
    {
      break;
    }  // ESP_GATTS_READ_EVT

    // ESP_GATTS_REG_EVT
    // reg:
    // - esp_gatt_status_t status
    // - uint16_t app_id
    //
    case ESP_GATTS_REG_EVT:
    {
      m_gatts_if = gatts_if;
      m_semaphoreRegisterAppEvt.give();  // Unlock the mutex waiting for the registration of the app.
      break;
    }  // ESP_GATTS_REG_EVT

    // ESP_GATTS_WRITE_EVT - A request to write the value of a characteristic has arrived.
    //
    // write:
    // - uint16_t      conn_id
    // - uint16_t      trans_id
    // - esp_bd_addr_t bda
    // - uint16_t      handle
    // - uint16_t      offset
    // - bool          need_rsp
    // - bool          is_prep
    // - uint16_t      len
    // - uint8_t*      value
    //
    case ESP_GATTS_WRITE_EVT:
    {
      break;
    }

    case ESP_GATTS_OPEN_EVT: m_semaphoreOpenEvt.give(param->open.status); break;

    default: break;
  }

  // Invoke the handler for every Service we have.
  m_serviceMap.handleGATTServerEvent(event, gatts_if, param);

  log_v("<< handleGATTServerEvent");
}  // handleGATTServerEvent

/**
 * @brief Register the app.
 *
 * @return N/A
 */
void BLEServer::registerApp(uint16_t m_appId) {
  log_v(">> registerApp - %d", m_appId);
  m_semaphoreRegisterAppEvt.take("registerApp");  // Take the mutex, will be released by ESP_GATTS_REG_EVT event.
  ::esp_ble_gatts_app_register(m_appId);
  m_semaphoreRegisterAppEvt.wait("registerApp");
  log_v("<< registerApp");
}  // registerApp

// Bluedroid callbacks

void BLEServerCallbacks::onConnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param) {
  log_d("BLEServerCallbacks", ">> onConnect(): Default");
  log_d("BLEServerCallbacks", "Device: %s", BLEDevice::toString().c_str());
  log_d("BLEServerCallbacks", "<< onConnect()");
}  // onConnect

void BLEServerCallbacks::onDisconnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param) {
  log_d("BLEServerCallbacks", ">> onDisconnect(): Default");
  log_d("BLEServerCallbacks", "Device: %s", BLEDevice::toString().c_str());
  log_d("BLEServerCallbacks", "<< onDisconnect()");
}  // onDisconnect

void BLEServerCallbacks::onMtuChanged(BLEServer *pServer, esp_ble_gatts_cb_param_t *param) {
  [[maybe_unused]]
  uint16_t mtu = param->mtu.mtu;
  log_d("BLEServerCallbacks", ">> onMtuChanged(): Default");
  log_d("BLEServerCallbacks", "Device: %s MTU: %d", BLEDevice::toString().c_str(), mtu);
  log_d("BLEServerCallbacks", "<< onMtuChanged()");
}  // onMtuChanged

#endif

/***************************************************************************
 *                       NimBLE functions                                  *
 ***************************************************************************/

#if defined(CONFIG_NIMBLE_ENABLED)

uint16_t BLEServer::getHandle() {
  return getConnId();
}

/**
 * @brief Resets the GATT server, used when services are added/removed after initialization.
 */
void BLEServer::resetGATT() {
  if (getConnectedCount() > 0) {
    return;
  }

  BLEDevice::stopAdvertising();
  ble_gatts_reset();
  ble_svc_gap_init();
  ble_svc_gatt_init();

  BLEService *svc = m_serviceMap.getFirst();
  while (svc != nullptr) {
    if (svc->m_removed > 0) {
      if (svc->m_removed == NIMBLE_ATT_REMOVE_DELETE) {
        m_serviceMap.removeService(svc);
        delete svc;
      }
    } else {
      svc->start();
    }

    svc = m_serviceMap.getNext();
  }

  m_svcChanged = false;
  m_gattsStarted = false;
}

/**
 * @brief Handle a GATT Server Event.
 *
 * @param [in] event
 * @param [in] gatts_if
 * @param [in] param
 *
 */
int BLEServer::handleGATTServerEvent(struct ble_gap_event *event, void *arg) {
  BLEServer *server = (BLEServer *)arg;
  log_v(">> handleGAPEvent: %s", BLEUtils::gapEventToString(event->type));
  int rc = 0;
  struct ble_gap_conn_desc desc;

  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
    {
      if (event->connect.status != 0) {
        /* Connection failed; resume advertising */
        log_e("Connection failed");
        BLEDevice::startAdvertising();
      } else {
        server->m_connId = event->connect.conn_handle;
        server->addPeerDevice((void *)server, false, event->connect.conn_handle);
        rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
        if (rc != 0) {
          return 0;
        }

        if (server->m_pServerCallbacks != nullptr) {
          server->m_pServerCallbacks->onConnect(server);
          server->m_pServerCallbacks->onConnect(server, &desc);
        }

        if (BLESecurity::m_securityEnabled && BLESecurity::m_forceSecurity) {
          BLESecurity::startSecurity(event->connect.conn_handle);
        }

        server->m_connectedCount++;
      }

      return 0;
    }  // BLE_GAP_EVENT_CONNECT

    case BLE_GAP_EVENT_DISCONNECT:
    {
      // If Host reset tell the device now before returning to prevent
      // any errors caused by calling host functions before resyncing.
      switch (event->disconnect.reason) {
        case BLE_HS_ETIMEOUT_HCI:
        case BLE_HS_EOS:
        case BLE_HS_ECONTROLLER:
        case BLE_HS_ENOTSYNCED:
          log_d("Disconnect - host reset, rc=%d", event->disconnect.reason);
          BLEDevice::onReset(event->disconnect.reason);
          break;
        default: break;
      }

      if (server->removePeerDevice(event->disconnect.conn.conn_handle, false)) {
        server->m_connectedCount--;
      }

      if (server->m_svcChanged) {
        server->resetGATT();
      }

      if (server->m_pServerCallbacks != nullptr) {
        server->m_pServerCallbacks->onDisconnect(server);
        server->m_pServerCallbacks->onDisconnect(server, &event->disconnect.conn);
      }

      // Reset security state on disconnect
      BLESecurity::resetSecurity();

#if !defined(CONFIG_BT_NIMBLE_EXT_ADV)
      if (server->m_advertiseOnDisconnect) {
        log_i("Start advertising again after disconnect");
        server->startAdvertising();
      }
#endif

      return 0;
    }  // BLE_GAP_EVENT_DISCONNECT

    case BLE_GAP_EVENT_SUBSCRIBE:
    {
      log_i("subscribe event; attr_handle=%d, subscribed: %s", event->subscribe.attr_handle, (event->subscribe.cur_notify ? "true" : "false"));

      for (auto &it : server->m_notifyChrVec) {
        if (it->getHandle() == event->subscribe.attr_handle) {
          uint16_t properties = it->getProperties();
          if ((properties & BLE_GATT_CHR_F_READ_AUTHEN) || (properties & BLE_GATT_CHR_F_READ_AUTHOR) || (properties & BLE_GATT_CHR_F_READ_ENC)) {
            rc = ble_gap_conn_find(event->subscribe.conn_handle, &desc);
            if (rc != 0) {
              break;
            }

            if (!desc.sec_state.encrypted) {
              BLESecurity::startSecurity(event->subscribe.conn_handle);
            }
          }

          it->setSubscribe(event);
          break;
        }
      }

      return 0;
    }  // BLE_GAP_EVENT_SUBSCRIBE

    case BLE_GAP_EVENT_MTU:
    {
      log_i("mtu update event; conn_handle=%d mtu=%d", event->mtu.conn_handle, event->mtu.value);
      rc = ble_gap_conn_find(event->mtu.conn_handle, &desc);
      if (rc != 0) {
        return 0;
      }

      if (server->m_pServerCallbacks != nullptr) {
        server->m_pServerCallbacks->onMtuChanged(server, &desc, event->mtu.value);
      }
      return 0;
    }  // BLE_GAP_EVENT_MTU

    case BLE_GAP_EVENT_NOTIFY_TX:
    {
      BLECharacteristic *pChar = nullptr;

      for (auto &it : server->m_notifyChrVec) {
        if (it->getHandle() == event->notify_tx.attr_handle) {
          pChar = it;
        }
      }

      if (pChar == nullptr) {
        return 0;
      }

      BLECharacteristicCallbacks::Status statusRC;

      if (event->notify_tx.indication) {
        if (event->notify_tx.status != 0) {
          if (event->notify_tx.status == BLE_HS_EDONE) {
            statusRC = BLECharacteristicCallbacks::Status::SUCCESS_INDICATE;
          } else if (rc == BLE_HS_ETIMEOUT) {
            statusRC = BLECharacteristicCallbacks::Status::ERROR_INDICATE_TIMEOUT;
          } else {
            statusRC = BLECharacteristicCallbacks::Status::ERROR_INDICATE_FAILURE;
          }
        } else {
          return 0;
        }

        server->clearIndicateWait(event->notify_tx.conn_handle);
      } else {
        if (event->notify_tx.status == 0) {
          statusRC = BLECharacteristicCallbacks::Status::SUCCESS_NOTIFY;
        } else {
          statusRC = BLECharacteristicCallbacks::Status::ERROR_GATT;
        }
      }

      pChar->m_pCallbacks->onStatus(pChar, statusRC, event->notify_tx.status);

      return 0;
    }  // BLE_GAP_EVENT_NOTIFY_TX

    case BLE_GAP_EVENT_ADV_COMPLETE:
    {
      log_d("Advertising Complete");
      BLEDevice::getAdvertising()->advCompleteCB();
      return 0;
    }

    case BLE_GAP_EVENT_CONN_UPDATE:
    {
      log_d("Connection parameters updated.");
      return 0;
    }  // BLE_GAP_EVENT_CONN_UPDATE

    case BLE_GAP_EVENT_REPEAT_PAIRING:
    {
      /* We already have a bond with the peer, but it is attempting to
       * establish a new secure link.  This app sacrifices security for
       * convenience: just throw away the old bond and accept the new link.
       */

      /* Delete the old bond. */
      rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
      if (rc != 0) {
        return BLE_GAP_REPEAT_PAIRING_IGNORE;
      }

      ble_store_util_delete_peer(&desc.peer_id_addr);

      /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
       * continue with the pairing operation.
       */
      return BLE_GAP_REPEAT_PAIRING_RETRY;
    }  // BLE_GAP_EVENT_REPEAT_PAIRING

    case BLE_GAP_EVENT_ENC_CHANGE:
    {
      rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
      if (rc != 0) {
        return BLE_ATT_ERR_INVALID_HANDLE;
      }

      if (BLEDevice::m_securityCallbacks != nullptr) {
        BLEDevice::m_securityCallbacks->onAuthenticationComplete(&desc);
      }

      return 0;
    }  // BLE_GAP_EVENT_ENC_CHANGE

    case BLE_GAP_EVENT_PASSKEY_ACTION:
    {
      struct ble_sm_io pkey = {0, 0};

      if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
        // Display the passkey on this device
        log_d("BLE_SM_IOACT_DISP");

        pkey.action = event->passkey.params.action;
        pkey.passkey = BLESecurity::getPassKey();

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
        log_d("BLE_SM_IOACT_DISP; ble_sm_inject_io result: %d", rc);

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
        log_d("BLE_SM_IOACT_NUMCMP; ble_sm_inject_io result: %d", rc);

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
        log_d("BLE_SM_IOACT_OOB; ble_sm_inject_io result: %d", rc);
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
        log_d("BLE_SM_IOACT_INPUT; ble_sm_inject_io result: %d", rc);

      } else if (event->passkey.params.action == BLE_SM_IOACT_NONE) {
        log_d("BLE_SM_IOACT_NONE");
        log_i("No passkey action required");
      }

      log_d("<< handleGATTServerEvent");
      return 0;
    }  // BLE_GAP_EVENT_PASSKEY_ACTION

    case BLE_GAP_EVENT_AUTHORIZE:
    {
      log_d("BLE_GAP_EVENT_AUTHORIZE");

      log_i(
        "Authorization request: conn_handle=%d attr_handle=%d is_read=%d", event->authorize.conn_handle, event->authorize.attr_handle, event->authorize.is_read
      );

      bool authorized = false;

      if (BLEDevice::m_securityCallbacks != nullptr) {
        log_i("Asking for authorization from onAuthorizationRequest");
        authorized =
          BLEDevice::m_securityCallbacks->onAuthorizationRequest(event->authorize.conn_handle, event->authorize.attr_handle, event->authorize.is_read);
      } else {
        log_w("onAuthorizationRequest not implemented. Rejecting authorization request");
      }

      if (authorized) {
        log_i("Authorization granted");
        event->authorize.out_response = BLE_GAP_AUTHORIZE_ACCEPT;
      } else {
        log_i("Authorization rejected");
        event->authorize.out_response = BLE_GAP_AUTHORIZE_REJECT;
      }

      return 0;
    }  // BLE_GAP_EVENT_AUTHORIZE

    default: break;
  }

  log_d("<< handleGATTServerEvent");
  return 0;
}

/**
 * @brief Request an Update the connection parameters:
 * * Can only be used after a connection has been established.
 * @param [in] conn_handle The connection handle of the peer to send the request to.
 * @param [in] minInterval The minimum connection interval in 1.25ms units.
 * @param [in] maxInterval The maximum connection interval in 1.25ms units.
 * @param [in] latency The number of packets allowed to skip (extends max interval).
 * @param [in] timeout The timeout time in 10ms units before disconnecting.
 */
void BLEServer::updateConnParams(uint16_t conn_handle, uint16_t minInterval, uint16_t maxInterval, uint16_t latency, uint16_t timeout) {
  ble_gap_upd_params params;

  params.latency = latency;
  params.itvl_max = maxInterval;                        // max_int = 0x20*1.25ms = 40ms
  params.itvl_min = minInterval;                        // min_int = 0x10*1.25ms = 20ms
  params.supervision_timeout = timeout;                 // timeout = 400*10ms = 4000ms
  params.min_ce_len = BLE_GAP_INITIAL_CONN_MIN_CE_LEN;  // Minimum length of connection event in 0.625ms units
  params.max_ce_len = BLE_GAP_INITIAL_CONN_MAX_CE_LEN;  // Maximum length of connection event in 0.625ms units

  int rc = ble_gap_update_params(conn_handle, &params);
  if (rc != 0) {
    log_e("Update params error: %d, %s", rc, BLEUtils::returnCodeToString(rc));
  }
}  // updateConnParams

bool BLEServer::setIndicateWait(uint16_t conn_handle) {
  for (auto i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
    if (m_indWait[i] == conn_handle) {
      return false;
    }
  }

  return true;
}

void BLEServer::clearIndicateWait(uint16_t conn_handle) {
  for (auto i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
    if (m_indWait[i] == conn_handle) {
      m_indWait[i] = BLE_HS_CONN_HANDLE_NONE;
      return;
    }
  }
}

/**
 * @brief Disconnect the specified client with optional reason.
 * @param [in] connId Connection Id of the client to disconnect.
 * @param [in] reason code for disconnecting.
 * @return NimBLE host return code.
 */
int BLEServer::disconnect(uint16_t connId, uint8_t reason) {
  log_d(">> disconnect()");

  int rc = ble_gap_terminate(connId, reason);
  if (rc != 0) {
    log_e("ble_gap_terminate failed: rc=%d %s", rc, BLEUtils::returnCodeToString(rc));
  }

  log_d("<< disconnect()");
  return rc;
}  // disconnect

/**
 * @brief Set the service changed flag
 */
void BLEServer::serviceChanged() {
  if (m_gattsStarted) {
    m_svcChanged = true;
  }
}  // serviceChanged

// NimBLE callbacks

void BLEServerCallbacks::onConnect(BLEServer *pServer, struct ble_gap_conn_desc *desc) {
  log_d("BLEServerCallbacks", ">> onConnect(): Default");
  log_d("BLEServerCallbacks", "Device: %s", BLEDevice::toString().c_str());
  log_d("BLEServerCallbacks", "<< onConnect()");
}  // onConnect

void BLEServerCallbacks::onDisconnect(BLEServer *pServer, struct ble_gap_conn_desc *desc) {
  log_d("BLEServerCallbacks", ">> onDisconnect(): Default");
  log_d("BLEServerCallbacks", "Device: %s", BLEDevice::toString().c_str());
  log_d("BLEServerCallbacks", "<< onDisconnect()");
}  // onDisconnect

void BLEServerCallbacks::onMtuChanged(BLEServer *pServer, ble_gap_conn_desc *desc, uint16_t mtu) {
  log_d("BLEServerCallbacks", ">> onMtuChanged(): Default");
  log_d("BLEServerCallbacks", "Device: %s MTU: %d", BLEDevice::toString().c_str(), mtu);
  log_d("BLEServerCallbacks", "<< onMtuChanged()");
}  // onMtuChanged

#endif  // CONFIG_NIMBLE_ENABLED

#endif /* CONFIG_BLUEDROID_ENABLED || CONFIG_NIMBLE_ENABLED */
#endif /* SOC_BLE_SUPPORTED || CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE */
