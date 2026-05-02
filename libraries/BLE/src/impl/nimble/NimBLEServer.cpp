/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2020-2025 Ryan Powell <ryan@nable-embedded.io> and
 * esp-nimble-cpp, NimBLE-Arduino contributors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file NimBLEServer.cpp
 * @brief BLE GATT server implementation for the Arduino BLE library using the
 *        NimBLE host.
 */

#include "impl/BLEGuards.h"
#if BLE_NIMBLE

#include "BLE.h"

#include "NimBLEServer.h"
#include "NimBLECharacteristic.h"
#include "NimBLEService.h"
#include "BLESecurity.h"
#include "impl/BLESecurityBackend.h"
#include "impl/BLEConnInfoData.h"
#include "impl/BLEImplHelpers.h"
#include "impl/BLEServerBackend.h"
#include "esp32-hal-log.h"

#if BLE_GATT_SERVER_SUPPORTED

#include <host/ble_hs.h>
#include <host/ble_att.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

namespace {

/**
 * @brief Notifies the application identity callback (if set) for a connection.
 * @param impl Server implementation; callback is read under the server mutex.
 * @param connInfo Connection for which identity was resolved or updated.
 */
void dispatchIdentity(BLEServer::Impl *impl, const BLEConnInfo &connInfo) {
  decltype(impl->onIdentityCb) cb;
  {
    BLELockGuard lock(impl->mtx);
    cb = impl->onIdentityCb;
  }
  BLEServer serverHandle = BLEServer::Impl::makeHandle(impl);
  if (cb) {
    cb(serverHandle, connInfo);
  }
}

}  // namespace

// --------------------------------------------------------------------------
// BLEConnInfoImpl -- Bridge from NimBLE ble_gap_conn_desc to BLEConnInfo
// --------------------------------------------------------------------------

/**
 * @brief Fills a @ref BLEConnInfo from a NimBLE connection descriptor.
 * @param desc NimBLE GAP connection descriptor.
 * @return Valid @ref BLEConnInfo for the link (MTU, PHY, RSSI when available).
 */
BLEConnInfo BLEConnInfoImpl::fromDesc(const struct ble_gap_conn_desc &desc) {
  BLEConnInfo info;
  info._valid = true;

  auto *d = info.data();
  d->handle = desc.conn_handle;
  d->mtu = ble_att_mtu(desc.conn_handle);
  d->address = BTAddress(desc.peer_ota_addr.val, static_cast<BTAddress::Type>(desc.peer_ota_addr.type));
  d->idAddress = BTAddress(desc.peer_id_addr.val, static_cast<BTAddress::Type>(desc.peer_id_addr.type));
  d->interval = desc.conn_itvl;
  d->latency = desc.conn_latency;
  d->supervisionTimeout = desc.supervision_timeout;
  d->encrypted = desc.sec_state.encrypted;
  d->authenticated = desc.sec_state.authenticated;
  d->bonded = desc.sec_state.bonded;
  d->keySize = desc.sec_state.key_size;
  d->central = (desc.role == BLE_GAP_ROLE_MASTER);
  d->txPhy = 1;
  d->rxPhy = 1;
  d->rssi = 0;

#if BLE5_SUPPORTED
  BLEPhy tx, rx;
  int rc = ble_gap_read_le_phy(desc.conn_handle, reinterpret_cast<uint8_t *>(&tx), reinterpret_cast<uint8_t *>(&rx));
  if (rc == 0) {
    d->txPhy = static_cast<uint8_t>(tx);
    d->rxPhy = static_cast<uint8_t>(rx);
  }
#endif

  int8_t rssi;
  if (ble_gap_conn_rssi(desc.conn_handle, &rssi) == 0) {
    d->rssi = rssi;
  }

  return info;
}

// --------------------------------------------------------------------------
// BLEServer::Impl -- NimBLE backend
// --------------------------------------------------------------------------

// BLEService::Impl is defined in NimBLEService.h (shared with NimBLECharacteristic.cpp)

// --------------------------------------------------------------------------
// BLEServer start / connection management
// --------------------------------------------------------------------------

/**
 * @brief Rebuilds the entire NimBLE GATT database: reset, register all services, start GATT.
 * @param impl Server state whose @c services are registered in order.
 * @return @c BTStatus::OK on success, or @c BTStatus::Fail on registration or start error.
 * @note Unlike Bluedroid, registration always uses a full @c ble_gatts_reset and re-register
 *       of all services; per-service create/delete is not used.
 */
static BTStatus nimbleRebuildGattDatabase(BLEServer::Impl &impl) {
  ble_gatts_reset();
  ble_svc_gap_init();
  ble_svc_gatt_init();

  if (!impl.services.empty()) {
    int rc = nimbleRegisterGattServices(impl.services);
    if (rc != 0) {
      log_e("nimbleRegisterGattServices: rc=%d", rc);
      return BTStatus::Fail;
    }
    for (auto &s : impl.services) {
      s->started = true;
    }
  }

  int rc = ble_gatts_start();
  if (rc != 0) {
    log_e("ble_gatts_start: rc=%d", rc);
    return BTStatus::Fail;
  }

  String name = BLE.getDeviceName();
  if (name.length() > 0) {
    ble_svc_gap_device_name_set(name.c_str());
  }

  impl.started = true;
  return BTStatus::OK;
}

/**
 * @brief Starts the GATT server or re-registers the database when new attributes appear.
 * @return @c BTStatus::OK if already up to date or after successful (re)start, or an
 *         error status on failure.
 * @note Re-registration is driven by new characteristics (handle 0); entire GATT is rebuilt
 *       via @c nimbleRebuildGattDatabase, not incremental Bluedroid-style create per service.
 */
BTStatus BLEServer::start() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (impl.started) {
    // Check if new services were added after the initial start
    // (characteristics with handle == 0 have not been registered).
    bool hasNew = false;
    for (auto &s : impl.services) {
      for (auto &c : s->characteristics) {
        if (c->handle == 0) {
          hasNew = true;
          break;
        }
      }
      if (hasNew) {
        break;
      }
    }
    if (!hasNew) {
      return BTStatus::OK;
    }
    log_d("Server: re-registering GATT services (new characteristics detected)");
  } else {
    log_d("Server: starting with %u service(s)", (unsigned)impl.services.size());
  }

  BTStatus st = nimbleRebuildGattDatabase(impl);
  if (st == BTStatus::OK) {
    log_i("Server: started, %u service(s) registered", (unsigned)impl.services.size());
  }
  return st;
}

/**
 * @brief Terminates a connection as central stack request.
 * @param connHandle Connection handle to close.
 * @param reason GAP termination reason code passed to the stack.
 * @return @c BTStatus::OK on success, @c InvalidState if no impl, or @c Fail on API error.
 */
BTStatus BLEServer::disconnect(uint16_t connHandle, uint8_t reason) {
  if (!_impl) {
    log_w("Server: disconnect called with no impl");
    return BTStatus::InvalidState;
  }
  int rc = ble_gap_terminate(connHandle, reason);
  if (rc != 0) {
    log_e("Server: ble_gap_terminate handle=%u rc=%d", connHandle, rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

/**
 * @brief Outgoing connection from the server object (not used on NimBLE).
 * @param address Target address (ignored).
 * @return Always @c BTStatus::NotSupported in server-only NimBLE role.
 */
BTStatus BLEServer::connect(const BTAddress &address) {
  log_w("Server: connect not supported on NimBLE (server role only)");
  return BTStatus::NotSupported;
}

/**
 * @brief Returns the negotiated ATT MTU for a connection.
 * @param connHandle GAP connection handle.
 * @return MTU in octets from the NimBLE ATT layer.
 */
uint16_t BLEServer::getPeerMTU(uint16_t connHandle) const {
  return ble_att_mtu(connHandle);
}

/**
 * @brief Requests a connection parameter update for an established link.
 * @param connHandle GAP connection handle.
 * @param params Desired min/max interval, latency, and supervision timeout.
 * @return @c BTStatus::OK on success, @c InvalidState if no impl, @c InvalidParam if out of spec, or @c Fail on API error.
 * @note Connection parameter constraints per BT Core Spec v5.x, Vol 6, Part B, §4.5.1:
 *       - Interval: 6–3200 (1.25 ms units); min ≤ max.
 *       - Latency: 0–499.
 *       - Supervision timeout: 10–3200 (10 ms units),
 *         must satisfy: timeout > (1 + latency) × maxInterval × 2.
 *       Invalid parameters are rejected locally before being sent to the controller.
 */
BTStatus BLEServer::updateConnParams(uint16_t connHandle, const BLEConnParams &params) {
  if (!_impl) {
    log_w("Server: updateConnParams called with no impl");
    return BTStatus::InvalidState;
  }

  if (!params.isValid()) {
    log_e("Server: updateConnParams — parameters out of spec");
    return BTStatus::InvalidParam;
  }

  struct ble_gap_upd_params nimParams = {};
  nimParams.itvl_min = params.minInterval;
  nimParams.itvl_max = params.maxInterval;
  nimParams.latency = params.latency;
  nimParams.supervision_timeout = params.timeout;
  int rc = ble_gap_update_params(connHandle, &nimParams);
  if (rc != 0) {
    log_e("Server: ble_gap_update_params handle=%u rc=%d", connHandle, rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

/**
 * @brief Sets preferred LE PHYs for a connection (BLE 5.0+ when supported).
 * @param connHandle GAP connection handle.
 * @param txPhy Preferred TX PHY.
 * @param rxPhy Preferred RX PHY.
 * @return @c BTStatus::OK on success, @c NotSupported if BLE5 is off, or @c Fail on error.
 */
BTStatus BLEServer::setPhy(uint16_t connHandle, BLEPhy txPhy, BLEPhy rxPhy) {
#if BLE5_SUPPORTED
  int rc = ble_gap_set_prefered_le_phy(connHandle, static_cast<uint8_t>(txPhy), static_cast<uint8_t>(rxPhy), 0);
  if (rc != 0) {
    log_e("Server: ble_gap_set_prefered_le_phy handle=%u rc=%d", connHandle, rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
#else
  log_w("Server: setPhy not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

/**
 * @brief Reads the current TX/RX LE PHYs for a connection.
 * @param connHandle GAP connection handle.
 * @param txPhy Filled with current TX PHY on success.
 * @param rxPhy Filled with current RX PHY on success.
 * @return @c BTStatus::OK on success, @c NotSupported if BLE5 is off, or @c Fail on error.
 */
BTStatus BLEServer::getPhy(uint16_t connHandle, BLEPhy &txPhy, BLEPhy &rxPhy) const {
#if BLE5_SUPPORTED
  uint8_t tx, rx;
  int rc = ble_gap_read_le_phy(connHandle, &tx, &rx);
  if (rc == 0) {
    txPhy = static_cast<BLEPhy>(tx);
    rxPhy = static_cast<BLEPhy>(rx);
    return BTStatus::OK;
  }
  log_e("Server: ble_gap_read_le_phy handle=%u rc=%d", connHandle, rc);
  return BTStatus::Fail;
#else
  log_w("Server: getPhy not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

/**
 * @brief Requests a data length update (DLE) on the link.
 * @param connHandle GAP connection handle.
 * @param txOctets Max TX payload octets to request.
 * @param txTime Max TX time parameter (microseconds per controller usage).
 * @return @c BTStatus::OK on success or @c Fail on API error.
 */
BTStatus BLEServer::setDataLen(uint16_t connHandle, uint16_t txOctets, uint16_t txTime) {
  int rc = ble_gap_set_data_len(connHandle, txOctets, txTime);
  if (rc != 0) {
    log_e("Server: ble_gap_set_data_len handle=%u rc=%d", connHandle, rc);
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

// --------------------------------------------------------------------------
// GAP event handler
// --------------------------------------------------------------------------

/**
 * @brief NimBLE GAP event callback: connections, MTU, pairing, and subscription.
 * @param event NimBLE GAP event; switch handles connect, disconnect, security, etc.
 * @param arg Pointer to @ref BLEServer::Impl (server instance).
 * @return 0 to consume the event, or a repeat-pairing code when applicable to SMP.
 */
int BLEServer::Impl::gapEventHandler(struct ble_gap_event *event, void *arg) {
  auto *impl = static_cast<BLEServer::Impl *>(arg);
  if (!impl) {
    return 0;
  }

  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
    {
      if (event->connect.status != 0) {
        log_e("Server: connection failed, status=%d", event->connect.status);
        return 0;
      }

      struct ble_gap_conn_desc desc;
      int rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
      if (rc != 0) {
        return 0;
      }

      BLEConnInfo connInfo = BLEConnInfoImpl::fromDesc(desc);

      {
        BLELockGuard lock(impl->mtx);
        impl->connSet(event->connect.conn_handle, connInfo);
      }
      ble_server_dispatch::dispatchConnect(impl, connInfo);
      return 0;
    }

    case BLE_GAP_EVENT_DISCONNECT:
    {
      uint16_t connHandle = event->disconnect.conn.conn_handle;
      uint8_t reason = event->disconnect.reason;
      log_d("Server: disconnect event, handle=%u reason=0x%02x", connHandle, reason);

      BLEConnInfo connInfo = BLEConnInfoImpl::fromDesc(event->disconnect.conn);

      bool shouldAdvertise = false;
      {
        BLELockGuard lock(impl->mtx);
        impl->connErase(connHandle);
        for (auto &svc : impl->services) {
          for (auto &chr : svc->characteristics) {
            auto &subs = chr->subscribers;
            for (auto it = subs.begin(); it != subs.end(); ++it) {
              if (it->first == connHandle) {
                subs.erase(it);
                break;
              }
            }
          }
        }
        shouldAdvertise = impl->advertiseOnDisconnect;
      }
      ble_server_dispatch::dispatchDisconnect(impl, connInfo, reason);

      if (shouldAdvertise) {
        BLE.startAdvertising();
      }
      return 0;
    }

    case BLE_GAP_EVENT_MTU:
    {
      uint16_t connHandle = event->mtu.conn_handle;
      uint16_t mtu = event->mtu.value;

      struct ble_gap_conn_desc desc;
      int rc = ble_gap_conn_find(connHandle, &desc);
      if (rc != 0) {
        log_w("MTU event: conn_find failed for handle %u", connHandle);
        return 0;
      }
      BLEConnInfo connInfo = BLEConnInfoImpl::fromDesc(desc);

      {
        BLELockGuard lock(impl->mtx);
        if (impl->connFind(connHandle)) {
          impl->connSet(connHandle, connInfo);
        }
      }
      ble_server_dispatch::dispatchMtuChanged(impl, connInfo, mtu);
      return 0;
    }

    case BLE_GAP_EVENT_CONN_UPDATE:
    {
      if (event->conn_update.status != 0) {
        return 0;
      }
      uint16_t connHandle = event->conn_update.conn_handle;
      struct ble_gap_conn_desc desc;
      int rc = ble_gap_conn_find(connHandle, &desc);
      if (rc != 0) {
        return 0;
      }

      BLEConnInfo connInfo = BLEConnInfoImpl::fromDesc(desc);

      {
        BLELockGuard lock(impl->mtx);
        if (impl->connFind(connHandle)) {
          impl->connSet(connHandle, connInfo);
        }
      }
      ble_server_dispatch::dispatchConnParamsUpdate(impl, connInfo);
      return 0;
    }

    case BLE_GAP_EVENT_ENC_CHANGE:
    {
      uint16_t connHandle = event->enc_change.conn_handle;
      int status = event->enc_change.status;

      struct ble_gap_conn_desc desc;
      int rc = ble_gap_conn_find(connHandle, &desc);
      if (rc != 0) {
        return 0;
      }

      BLEConnInfo connInfo = BLEConnInfoImpl::fromDesc(desc);

      {
        BLELockGuard lock(impl->mtx);
        if (impl->connFind(connHandle)) {
          impl->connSet(connHandle, connInfo);
        }
      }
      dispatchIdentity(impl, connInfo);

#if BLE_SMP_SUPPORTED
      BLESecurity sec = BLE.getSecurity();
      if (sec) {
        BLESecurityBackend::notifyAuthComplete(sec, connInfo, status == 0);
      }
#endif /* BLE_SMP_SUPPORTED */
      return 0;
    }

    case BLE_GAP_EVENT_IDENTITY_RESOLVED:
    {
      uint16_t connHandle = event->identity_resolved.conn_handle;

      struct ble_gap_conn_desc desc;
      int rc = ble_gap_conn_find(connHandle, &desc);
      if (rc != 0) {
        return 0;
      }

      BLEConnInfo connInfo = BLEConnInfoImpl::fromDesc(desc);

      {
        BLELockGuard lock(impl->mtx);
        if (impl->connFind(connHandle)) {
          impl->connSet(connHandle, connInfo);
        }
      }
      dispatchIdentity(impl, connInfo);
      return 0;
    }

#if BLE_SMP_SUPPORTED
    case BLE_GAP_EVENT_PASSKEY_ACTION:
    {
      uint16_t connHandle = event->passkey.conn_handle;

      struct ble_gap_conn_desc desc;
      int rc = ble_gap_conn_find(connHandle, &desc);
      if (rc != 0) {
        return 0;
      }

      BLEConnInfo connInfo = BLEConnInfoImpl::fromDesc(desc);
      BLESecurity sec = BLE.getSecurity();
      if (!sec) {
        return 0;
      }

      if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
        struct ble_sm_io pkey = {};
        pkey.action = BLE_SM_IOACT_DISP;
        pkey.passkey = BLESecurityBackend::resolvePasskeyForDisplay(sec, connInfo);
        ble_sm_inject_io(connHandle, &pkey);
      } else if (event->passkey.params.action == BLE_SM_IOACT_INPUT) {
        struct ble_sm_io pkey = {};
        pkey.action = BLE_SM_IOACT_INPUT;
        pkey.passkey = BLESecurityBackend::resolvePasskeyForInput(sec, connInfo);
        ble_sm_inject_io(connHandle, &pkey);
      } else if (event->passkey.params.action == BLE_SM_IOACT_NUMCMP) {
        struct ble_sm_io pkey = {};
        pkey.action = BLE_SM_IOACT_NUMCMP;
        pkey.numcmp_accept = BLESecurityBackend::resolveNumericComparison(sec, connInfo, event->passkey.params.numcmp) ? 1 : 0;
        ble_sm_inject_io(connHandle, &pkey);
      }
      return 0;
    }

    case BLE_GAP_EVENT_REPEAT_PAIRING:
    {
      struct ble_gap_conn_desc desc;
      int rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
      if (rc != 0) {
        return BLE_GAP_REPEAT_PAIRING_IGNORE;
      }

      ble_store_util_delete_peer(&desc.peer_id_addr);
      return BLE_GAP_REPEAT_PAIRING_RETRY;
    }
#endif /* BLE_SMP_SUPPORTED */

    case BLE_GAP_EVENT_SUBSCRIBE:
    {
      uint16_t connHandle = event->subscribe.conn_handle;
      uint16_t attrHandle = event->subscribe.attr_handle;
      uint8_t curNotify = event->subscribe.cur_notify;
      uint8_t curIndicate = event->subscribe.cur_indicate;
      uint16_t subVal = (curIndicate ? 0x0002 : 0) | (curNotify ? 0x0001 : 0);
      log_d("Server: subscribe event, conn=%u attr=%u notify=%d indicate=%d subVal=0x%04x", connHandle, attrHandle, curNotify, curIndicate, subVal);

      BLECharacteristic::SubscribeHandler subscribeCb;
      std::shared_ptr<BLECharacteristic::Impl> chrImpl;
      {
        BLELockGuard lock(impl->mtx);
        for (auto &svc : impl->services) {
          for (auto &chr : svc->characteristics) {
            if (chr->handle == attrHandle) {
              if (subVal > 0) {
                bool found = false;
                for (auto &kv : chr->subscribers) {
                  if (kv.first == connHandle) {
                    kv.second = subVal;
                    found = true;
                    break;
                  }
                }
                if (!found) {
                  chr->subscribers.emplace_back(connHandle, subVal);
                }
              } else {
                auto &subs = chr->subscribers;
                for (auto it = subs.begin(); it != subs.end(); ++it) {
                  if (it->first == connHandle) {
                    subs.erase(it);
                    break;
                  }
                }
              }
              subscribeCb = chr->onSubscribeCb;
              chrImpl = chr;
              break;
            }
          }
          if (chrImpl) {
            break;
          }
        }
      }
      if (subscribeCb) {
        BLECharacteristic characteristic(chrImpl);
        struct ble_gap_conn_desc desc;
        if (ble_gap_conn_find(connHandle, &desc) == 0) {
          BLEConnInfo info = BLEConnInfoImpl::fromDesc(desc);
          subscribeCb(characteristic, info, subVal);
        }
      }
      return 0;
    }

    case BLE_GAP_EVENT_NOTIFY_TX:
    {
      return 0;
    }

#if defined(BLE_GAP_EVENT_AUTHORIZE)
    // Per-access authorization callback from the NimBLE stack. Triggered
    // when an attribute was registered with BLE_GATT_CHR_F_{READ,WRITE}_AUTHOR
    // and the connection is not yet marked as authorized via
    // ble_gap_dev_authorization(). We route the decision into the
    // application-supplied BLESecurity::onAuthorization() handler so the
    // sketch can accept/reject every ATT read or write individually.
    case BLE_GAP_EVENT_AUTHORIZE:
    {
      uint16_t connHandle = event->authorize.conn_handle;
      uint16_t attrHandle = event->authorize.attr_handle;
      bool isRead = (event->authorize.is_read != 0);

      BLEConnInfo conn;
      struct ble_gap_conn_desc desc {};
      if (ble_gap_conn_find(connHandle, &desc) == 0) {
        conn = BLEConnInfoImpl::fromDesc(desc);
      }

      BLESecurity sec = BLE.getSecurity();
      bool allowed = sec ? BLESecurityBackend::notifyAuthorization(sec, conn, attrHandle, isRead) : false;
      event->authorize.out_response = allowed ? BLE_GAP_AUTHORIZE_ACCEPT : BLE_GAP_AUTHORIZE_REJECT;
      log_d("Server: BLE_GAP_EVENT_AUTHORIZE handle=%u attr=%u read=%d -> %s", connHandle, attrHandle, (int)isRead, allowed ? "ACCEPT" : "REJECT");
      return 0;
    }
#endif

    default: return 0;
  }
}

/**
 * @brief Entry that forwards a GAP event into @ref gapEventHandler.
 * @param rawEvent Pointer to @c struct ble_gap_event, or null.
 * @return Stack result from @c BLEServer::Impl::gapEventHandler, or 0 if impl or event is null.
 */
int BLEServer::handleGapEvent(void *rawEvent) {
  if (!_impl || !rawEvent) {
    return 0;
  }
  return Impl::gapEventHandler(static_cast<struct ble_gap_event *>(rawEvent), _impl.get());
}

// BLEService public API methods are in NimBLECharacteristic.cpp

// --------------------------------------------------------------------------
// BLEClass::createServer() -- NimBLE factory method
// --------------------------------------------------------------------------

/**
 * @brief Returns the singleton @ref BLEServer for NimBLE, creating the impl on first use.
 * @return A @ref BLEServer handle, or an invalid one if @ref BLEClass::isInitialized is false.
 */
BLEServer BLEClass::createServer() {
  if (!isInitialized()) {
    log_e("createServer: BLE not initialized");
    return BLEServer();
  }

  // Singleton server: use a static shared_ptr
  static std::shared_ptr<BLEServer::Impl> server;
  if (!server) {
    log_d("createServer: creating new server instance");
    server = std::make_shared<BLEServer::Impl>();
  }

  return BLEServer(server);
}

// --------------------------------------------------------------------------
// Dynamic service removal (NimBLE has no single-service delete; rebuild GATT)
// --------------------------------------------------------------------------

/**
 * @brief Removes a service from the in-memory list and, if the server was started, rebuilds GATT.
 * @param impl Server that owns the service list.
 * @param svc Service implementation to remove; must be present in @a impl.
 * @return @c OK after removal, @c InvalidState, or @c Fail if rebuild fails.
 * @note There is no single-service @c gatts delete on NimBLE; a running server triggers a
 *       full GATT database rebuild like @ref BLEServer::start, unlike Bluedroid
 *       @c esp_ble_gatts_delete_service.
 */
BTStatus bleServerRemoveService(BLEServer::Impl *impl, std::shared_ptr<BLEService::Impl> svc) {
  if (!impl || !svc) {
    return BTStatus::InvalidState;
  }

  bool inList = false;
  {
    BLELockGuard lock(impl->mtx);
    for (auto &s : impl->services) {
      if (s.get() == svc.get()) {
        inList = true;
        break;
      }
    }
  }
  if (!inList) {
    return BTStatus::InvalidState;
  }

  {
    BLELockGuard lock(impl->mtx);
    for (auto it = impl->services.begin(); it != impl->services.end(); ++it) {
      if (it->get() == svc.get()) {
        impl->services.erase(it);
        break;
      }
    }
  }

  if (!impl->started) {
    return BTStatus::OK;
  }

  BTStatus st = nimbleRebuildGattDatabase(*impl);
  if (st == BTStatus::OK) {
    log_i("Server: service removed, %u service(s) registered", (unsigned)impl->services.size());
  }
  return st;
}

#else /* !BLE_GATT_SERVER_SUPPORTED -- stubs */

// Stubs for BLE_GATT_SERVER_SUPPORTED == 0: log; return NotSupported, 0, or empty. getPhy() may not
// set out-parameters; check return or initialize.

BTStatus BLEServer::start() {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEServer::disconnect(uint16_t, uint8_t) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEServer::connect(const BTAddress &) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

uint16_t BLEServer::getPeerMTU(uint16_t) const {
  return 0;
}

BTStatus BLEServer::updateConnParams(uint16_t, const BLEConnParams &) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEServer::setPhy(uint16_t, BLEPhy, BLEPhy) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEServer::getPhy(uint16_t, BLEPhy &, BLEPhy &) const {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLEServer::setDataLen(uint16_t, uint16_t, uint16_t) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

int BLEServer::handleGapEvent(void *) {
  return 0;
}

BLEServer BLEClass::createServer() {
  log_w("GATT server not supported");
  return BLEServer();
}

#endif /* BLE_GATT_SERVER_SUPPORTED */

#endif /* BLE_NIMBLE */
