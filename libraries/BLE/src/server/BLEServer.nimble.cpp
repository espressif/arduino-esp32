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
 * @file BLEServer.nimble.cpp
 * @brief BLE GATT server implementation for the Arduino BLE library using the
 *        NimBLE host.
 */

#include "core/BLEGuards.h"
#if BLE_NIMBLE

#include "BLE.h"

#include "BLE.nimble.h"
#include "server/BLEServer.nimble.h"
#include "gatt/BLEGattAttributes.nimble.h"
#include "types/BLEConnInfo.nimble.h"
#include "security/BLESecurity.h"
#include "types/BLEConnInfoData.h"
#include "core/BLEImplHelpers.h"
#include "core/BLEBackend.h"
#include "esp32-hal-log.h"

// API contract is documented on the declarations in the public BLE*.h headers; the definitions below carry implementation notes only.

#if BLE_GATT_SERVER_SUPPORTED

#include "gatt/BLECharacteristicValidation.h"
#include <host/ble_hs.h>
#include <host/ble_att.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include <nimble/nimble_port.h>

namespace {

// Restarting advertising after a disconnect is queued onto the NimBLE host
// event queue instead of being started inline in the GAP event callback, so it
// runs once the current event has been fully processed. This keeps the
// disconnect -> re-advertise ordering identical to the Bluedroid backend, which
// defers via esp_timer for the same reason.
struct ble_npl_event sReAdvEvent;
bool sReAdvEventInited = false;

void reAdvertiseEventCb(struct ble_npl_event *) {
  BLE.startAdvertising();
}

void scheduleReAdvertise() {
  if (!sReAdvEventInited) {
    ble_npl_event_init(&sReAdvEvent, reAdvertiseEventCb, nullptr);
    sReAdvEventInited = true;
  }
  ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &sReAdvEvent);
}

}  // namespace

// The NimBLE FreeRTOS port allocates each ble_npl_event's backing object from a
// pool that nimble_port_deinit() frees on BLE.end(). Because sReAdvEvent is a
// process-wide static, its backing pointer would dangle into that freed pool
// across an end()/begin() cycle while sReAdvEventInited stayed true, so a later
// disconnect would dispatch an event with a stale/NULL fn (crash in
// nimble_port_run). BLEClass::end() calls this to drop the reference so the next
// session re-initializes a fresh event; the freed pool block needs no explicit
// return because deinit reclaims the whole buffer.
void nimbleResetReAdvertiseEvent() {
  log_d("resetting sReAdvEvent, inited=%d", sReAdvEventInited);
  if (sReAdvEventInited) {
    // ble_npl_event_deinit() releases the backend-specific backing storage
    // (e.g. returns the block to S31's btdm_osal event pool) instead of just
    // clearing the struct in place, so the event lifecycle stays balanced
    // across an end()/begin() cycle regardless of SoC.
    ble_npl_event_deinit(&sReAdvEvent);
  }
  sReAdvEventInited = false;
}

// --------------------------------------------------------------------------
// BLEServer::Impl -- NimBLE backend
// --------------------------------------------------------------------------

// BLEService::Impl is defined in NimBLEGattAttributes.h.

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
  for (const auto &svc : impl.services) {
    for (const auto &chr : svc->characteristics) {
      if (!bleValidateCharForRegistration(*chr, /*stackIsNimble=*/true)) {
        log_e("GATT: characteristic %s rejected by validation", chr->uuid.toString().c_str());
        return BTStatus::InvalidState;
      }
    }
  }

  // ble_gatts_reset() is EBUSY while a central is connected or GAP is active
  // (adv/scan/connect). Ignoring that and calling ble_svc_gap_init() panics
  // inside ble_gatts_add_svcs (SYSINIT_PANIC_ASSERT rc==0).
  int rc = ble_gatts_reset();
  if (rc != 0) {
    log_w("ble_gatts_reset: rc=%d (GATT table immutable while connected or advertising)", rc);
    return rc == BLE_HS_EBUSY ? BTStatus::Busy : BTStatus::Fail;
  }
  ble_svc_gap_init();
  ble_svc_gatt_init();

  if (!impl.services.empty()) {
    rc = nimbleRegisterGattServices(impl.services);
    if (rc != 0) {
      log_e("nimbleRegisterGattServices: rc=%d", rc);
      return BTStatus::Fail;
    }
    for (auto &s : impl.services) {
      s->started = true;
    }
  }

  rc = ble_gatts_start();
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

// Re-registration is driven by new characteristics (handle 0); the entire GATT is rebuilt via
// nimbleRebuildGattDatabase, not incremental Bluedroid-style create per service.
BTStatus BLEServer::start() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  bool active;
  bool wasStarted;
  {
    BLELockGuard lock(impl.mtx);
    active = impl.active;
    wasStarted = impl.started;
  }
  if (!active) {
    return BTStatus::InvalidState;
  }
  if (wasStarted) {
    // Check if new services were added after the initial start
    // (characteristics with handle == 0 have not been registered).
    bool hasNew = false;
    for (auto &s : impl.services) {
      if (!s->started) {
        hasNew = true;
        break;
      }
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

// Not used on NimBLE (server role only); always returns BTStatus::NotSupported.
BTStatus BLEServer::connect(const BTAddress &address) {
  log_w("Server: connect not supported on NimBLE (server role only)");
  return BTStatus::NotSupported;
}

uint16_t BLEServer::getPeerMTU(uint16_t connHandle) const {
  return ble_att_mtu(connHandle);
}

BLEIndicationSend bleServerSendIndication(
  BLEServer::Impl *impl, uint16_t connHandle, uint16_t attrHandle, const uint8_t *data, size_t length, BLECharacteristic::NotifyStatus &status, uint32_t &code
) {
  {
    BLELockGuard lock(impl->mtx);
    if (!impl->active || !impl->started || !impl->connFind(connHandle)) {
      status = BLECharacteristic::ERROR_NO_CLIENT;
      code = BLE_HS_ENOTCONN;
      return BLEIndicationSend::Failed;
    }
  }
  struct os_mbuf *om = ble_hs_mbuf_from_flat(data, length);
  if (!om) {
    log_e("Server: indicate - no memory for mbuf (conn=%u)", connHandle);
    status = BLECharacteristic::ERROR_INDICATE_FAILURE;
    code = BLE_HS_ENOMEM;
    return BLEIndicationSend::Failed;
  }
  // EALREADY: the connection's indication slot is held by one NimBLE sent
  // itself (e.g. Service Changed); the queue retries when that one completes.
  int rc = ble_gatts_indicate_custom(connHandle, attrHandle, om);
  if (rc == 0) {
    return BLEIndicationSend::Sent;
  }
  if (rc == BLE_HS_EALREADY) {
    status = BLECharacteristic::ERROR_INDICATE_FAILURE;
    code = BLE_HS_EALREADY;
    return BLEIndicationSend::Busy;
  }
  log_w("Server: indicate failed for conn=%u handle=%u rc=%d", connHandle, attrHandle, rc);
  status = (rc == BLE_HS_ENOTCONN) ? BLECharacteristic::ERROR_NO_CLIENT : BLECharacteristic::ERROR_INDICATE_FAILURE;
  code = rc;
  return BLEIndicationSend::Failed;
}

// Parameters are validated locally against the BT Core Spec ranges (v5.x, Vol 6, Part B,
// §4.5.1) and rejected before being sent to the controller.
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
  txPhy = BLEPhy::PHY_1M;
  rxPhy = BLEPhy::PHY_1M;
  log_w("Server: getPhy not supported (BLE 5.0 unavailable)");
  return BTStatus::NotSupported;
#endif
}

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
      impl->dispatchConnect(connInfo);
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
      if (impl->dropIndicationsHook) {
        impl->dropIndicationsHook(impl, connHandle);
      }
      impl->dispatchDisconnect(connInfo, reason);

      if (shouldAdvertise) {
        scheduleReAdvertise();
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
      impl->dispatchMtuChanged(connInfo, mtu);
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
      impl->dispatchConnParamsUpdate(connInfo);
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
      impl->dispatchIdentityResolved(connInfo);

#if BLE_SMP_SUPPORTED
      if (status == (BLE_HS_ERR_HCI_BASE + BLE_ERR_PINKEY_MISSING)) {
        ble_store_util_delete_peer(&desc.peer_id_addr);
      } else if (status != 0) {
        log_w("Server: pairing/encryption failed on conn %u, host status=0x%04x", connHandle, status);
      }
      nimbleForwardSecurityGapEvent(event);
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
      impl->dispatchIdentityResolved(connInfo);
      return 0;
    }

#if BLE_SMP_SUPPORTED
    case BLE_GAP_EVENT_PASSKEY_ACTION:
    case BLE_GAP_EVENT_PARING_COMPLETE:  // Name is misspelled in NimBLE
    case BLE_GAP_EVENT_REPEAT_PAIRING:
      return nimbleForwardSecurityGapEvent(event);
#endif /* BLE_SMP_SUPPORTED */

    case BLE_GAP_EVENT_SUBSCRIBE:
    {
      uint16_t connHandle = event->subscribe.conn_handle;
      uint16_t attrHandle = event->subscribe.attr_handle;
      uint8_t curNotify = event->subscribe.cur_notify;
      uint8_t curIndicate = event->subscribe.cur_indicate;
      uint16_t subVal = (curIndicate ? BLE_CCCD_INDICATE : 0) | (curNotify ? BLE_CCCD_NOTIFY : 0);
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
              chrImpl = chr;
              break;
            }
          }
          if (chrImpl) {
            break;
          }
        }
      }
      if (chrImpl) {
        BLELockGuard lock(chrImpl->mtx);
        subscribeCb = chrImpl->onSubscribeCb;
      }
      if (subscribeCb) {
        BLECharacteristic characteristic = BLECharacteristicImplCommon::makeHandle(chrImpl);
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
      int status = event->notify_tx.status;
      uint16_t connHandle = event->notify_tx.conn_handle;
      uint16_t attrHandle = event->notify_tx.attr_handle;
      if (status != 0 && status != BLE_HS_EDONE) {
        log_w("Server: %s on handle %u conn=%u failed, status=%d", event->notify_tx.indication ? "indication" : "notification", attrHandle, connHandle, status);
      }

      if (event->notify_tx.indication) {
        // status 0 only means the indication went out; the confirmation
        // (BLE_HS_EDONE) or failure follows in a later event.
        if (status == 0) {
          return 0;
        }
        BLECharacteristic::NotifyStatus result;
        if (status == BLE_HS_EDONE) {
          result = BLECharacteristic::SUCCESS_INDICATE;
        } else if (status == BLE_HS_ENOTCONN) {
          result = BLECharacteristic::ERROR_NO_CLIENT;
        } else if (status == BLE_HS_ETIMEOUT) {
          result = BLECharacteristic::ERROR_INDICATE_TIMEOUT;
        } else {
          result = BLECharacteristic::ERROR_INDICATE_FAILURE;
        }
        bool completed = impl->indicationDoneHook && impl->indicationDoneHook(impl, connHandle, attrHandle, result, (status == BLE_HS_EDONE) ? 0 : status);
        if (completed && status == BLE_HS_ETIMEOUT) {
          ble_gap_terminate(connHandle, BLE_ERR_REM_USER_CONN_TERM);
        }
        return 0;
      }

      std::shared_ptr<BLECharacteristic::Impl> chrImpl;
      {
        BLELockGuard lock(impl->mtx);
        for (auto &svc : impl->services) {
          for (auto &chr : svc->characteristics) {
            if (chr->handle == attrHandle) {
              chrImpl = chr;
              break;
            }
          }
          if (chrImpl) {
            break;
          }
        }
      }
      if (chrImpl) {
        BLECharacteristic::NotifyStatus result = BLECharacteristic::SUCCESS_NOTIFY;
        if (status == BLE_HS_ENOTCONN) {
          result = BLECharacteristic::ERROR_NO_CLIENT;
        } else if (status != 0) {
          result = BLECharacteristic::ERROR_GATT;
        }
        chrImpl->dispatchStatus(result, status);
      }
      return 0;
    }

#if defined(BLE_GAP_EVENT_AUTHORIZE)
    case BLE_GAP_EVENT_AUTHORIZE:
      return nimbleForwardSecurityGapEvent(event);
#endif

    default: return 0;
  }
}

int BLEServer::handleGapEvent(void *rawEvent) {
  if (!_impl || !rawEvent) {
    return 0;
  }
  return Impl::gapEventHandler(static_cast<struct ble_gap_event *>(rawEvent), _impl.get());
}

// BLEService public API methods are in NimBLECharacteristic.cpp

// --------------------------------------------------------------------------
// Dynamic service removal (NimBLE has no single-service delete; rebuild GATT)
// --------------------------------------------------------------------------

// NimBLE has no single-service delete; a started server rebuilds the entire GATT database
// (like BLEServer::start), unlike Bluedroid's esp_ble_gatts_delete_service.
BTStatus bleServerRemoveService(BLEServer::Impl *impl, std::shared_ptr<BLEService::Impl> svc) {
  if (!impl || !svc) {
    return BTStatus::InvalidState;
  }

  size_t originalIndex = 0;
  bool inList = false;
  {
    BLELockGuard lock(impl->mtx);
    for (size_t i = 0; i < impl->services.size(); ++i) {
      if (impl->services[i].get() == svc.get()) {
        originalIndex = i;
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

  if (!svc->started || !impl->started) {
    bleDetachLocalService(svc);
    return BTStatus::OK;
  }

  BTStatus st = nimbleRebuildGattDatabase(*impl);
  if (st == BTStatus::OK) {
    bleDetachLocalService(svc);
    log_i("Server: service removed, %u service(s) registered", (unsigned)impl->services.size());
    return BTStatus::OK;
  }

  // reset() has already replaced the native database. Restore the local service
  // at its original position and rebuild again so a failed removal is atomic
  // from the public API's perspective.
  {
    BLELockGuard lock(impl->mtx);
    size_t insertIndex = originalIndex < impl->services.size() ? originalIndex : impl->services.size();
    impl->services.insert(impl->services.begin() + insertIndex, svc);
  }
  BTStatus rollback = nimbleRebuildGattDatabase(*impl);
  if (rollback != BTStatus::OK) {
    log_e("Server: failed to restore GATT database after service-removal failure");
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

#endif /* BLE_GATT_SERVER_SUPPORTED */

#endif /* BLE_NIMBLE */
