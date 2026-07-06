/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
 * Copyright 2017 Neil Kolban
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
 * @file BluedroidServer.cpp
 * @brief BLE GATT server implementation for the Arduino BLE library using the
 *        ESP-IDF Bluedroid GATTS stack.
 */

#include "impl/common/BLEGuards.h"
#if BLE_BLUEDROID

#if BLE_GATT_SERVER_SUPPORTED

#include "BLE.h"

#include "BluedroidServer.h"
#include "BluedroidCore.h"
#include "BluedroidGattAttributes.h"
#include "BluedroidUUID.h"
#include "impl/common/BLESync.h"
#include "impl/common/BLEImplHelpers.h"
#include "impl/common/BLECharacteristicValidation.h"
#include "impl/common/BLEConnInfoData.h"
#include "BluedroidConnInfo.h"
#include "impl/common/BLEMutex.h"
#include "impl/BLEBackend.h"
#include "esp32-hal-log.h"

#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <esp_bt_device.h>
#include <string.h>

// API contract is documented on the declarations in the public BLE*.h headers; the definitions below carry implementation notes only.

// --------------------------------------------------------------------------
// Prepared (long) write buffers -- per ATT bearer, keyed by conn_id
// --------------------------------------------------------------------------

std::vector<BLEServer::Impl::PrepWrite> &BLEServer::Impl::prepWritesFor(uint16_t connId) {
  for (auto &entry : prepWrites) {
    if (entry.first == connId) {
      return entry.second;
    }
  }
  prepWrites.emplace_back(connId, std::vector<PrepWrite>{});
  return prepWrites.back().second;
}

std::vector<BLEServer::Impl::PrepWrite> *BLEServer::Impl::findPrepWrites(uint16_t connId) {
  for (auto &entry : prepWrites) {
    if (entry.first == connId) {
      return &entry.second;
    }
  }
  return nullptr;
}

void BLEServer::Impl::erasePrepWrites(uint16_t connId) {
  for (auto it = prepWrites.begin(); it != prepWrites.end(); ++it) {
    if (it->first == connId) {
      prepWrites.erase(it);
      return;
    }
  }
}

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------

namespace {

// Pure mapping from user-declared permissions to Bluedroid's esp_gatt_perm_t;
// read @ref toEspGattPermissions documentation for the authorize-bit behavior.

/**
 * @brief Maps @ref BLEPermission to Bluedroid @c esp_gatt_perm_t for GATT registration.
 * @param perm Declared attribute permissions.
 * @return Bluedroid permission bitfield for @c esp_ble_gatts_add_char and descriptors.
 * @note Unlike NimBLE, there are no separate read/write authorize perm bits; authorized
 *       access is implemented in software in @ref BLEServer::Impl::handleGATTS.
 */
esp_gatt_perm_t toEspGattPermissions(BLEPermission perm) {
  esp_gatt_perm_t result = 0;
  if (perm & BLEPermission::Read) {
    result |= ESP_GATT_PERM_READ;
  }
  if (perm & BLEPermission::ReadEncrypted) {
    result |= ESP_GATT_PERM_READ_ENCRYPTED;
  }
  if (perm & BLEPermission::ReadAuthenticated) {
    result |= ESP_GATT_PERM_READ_ENC_MITM;
  }
  if (perm & BLEPermission::ReadAuthorized) {
    result |= ESP_GATT_PERM_READ;
  }
  if (perm & BLEPermission::Write) {
    result |= ESP_GATT_PERM_WRITE;
  }
  if (perm & BLEPermission::WriteEncrypted) {
    result |= ESP_GATT_PERM_WRITE_ENCRYPTED;
  }
  if (perm & BLEPermission::WriteAuthenticated) {
    result |= ESP_GATT_PERM_WRITE_ENC_MITM;
  }
  if (perm & BLEPermission::WriteAuthorized) {
    result |= ESP_GATT_PERM_WRITE;
  }
  return result;
}

// Fail-closed property mapping: see @ref toEspGattProperties.

/**
 * @brief Maps @ref BLEProperty and @ref BLEPermission to Bluedroid characteristic properties.
 * @param props Requested GATT property bits to advertise.
 * @param perms Attribute permissions; read/write GATT property bits are gated on these.
 * @return @c esp_gatt_char_prop_t for @c esp_ble_gatts_add_char.
 */
esp_gatt_char_prop_t toEspGattProperties(BLEProperty props, BLEPermission perms) {
  const bool anyReadPerm = (perms & BLEPermission::Read) || (perms & BLEPermission::ReadEncrypted) || (perms & BLEPermission::ReadAuthenticated)
                           || (perms & BLEPermission::ReadAuthorized);
  const bool anyWritePerm = (perms & BLEPermission::Write) || (perms & BLEPermission::WriteEncrypted) || (perms & BLEPermission::WriteAuthenticated)
                            || (perms & BLEPermission::WriteAuthorized);

  esp_gatt_char_prop_t r = 0;
  if (props & BLEProperty::Broadcast) {
    r |= ESP_GATT_CHAR_PROP_BIT_BROADCAST;
  }
  if ((props & BLEProperty::Read) && anyReadPerm) {
    r |= ESP_GATT_CHAR_PROP_BIT_READ;
  }
  if ((props & BLEProperty::WriteNR) && anyWritePerm) {
    r |= ESP_GATT_CHAR_PROP_BIT_WRITE_NR;
  }
  if ((props & BLEProperty::Write) && anyWritePerm) {
    r |= ESP_GATT_CHAR_PROP_BIT_WRITE;
  }
  if (props & BLEProperty::Notify) {
    r |= ESP_GATT_CHAR_PROP_BIT_NOTIFY;
  }
  if (props & BLEProperty::Indicate) {
    r |= ESP_GATT_CHAR_PROP_BIT_INDICATE;
  }
  if ((props & BLEProperty::SignedWrite) && anyWritePerm) {
    r |= ESP_GATT_CHAR_PROP_BIT_AUTH;
  }
  if (props & BLEProperty::ExtendedProps) {
    r |= ESP_GATT_CHAR_PROP_BIT_EXT_PROP;
  }
  return r;
}

constexpr uint16_t CCCD_UUID16 = BLE_DSC_UUID16_CCCD;

}  // namespace

// --------------------------------------------------------------------------
// Static instance & makeHandle
// --------------------------------------------------------------------------

BLEServer::Impl *BLEServer::Impl::s_instance = nullptr;

// --------------------------------------------------------------------------
// Callback dispatch (snapshot under lock, invoke outside)
// --------------------------------------------------------------------------

namespace {

/**
 * @brief Finds a characteristic by its GATTS attribute handle.
 * @param impl Server state containing the service tree.
 * @param handle Attribute handle from the stack.
 * @return Pointer to the characteristic @c Impl, or null if not found.
 */
BLECharacteristic::Impl *findCharByHandle(BLEServer::Impl *impl, uint16_t handle) {
  for (auto &svc : impl->services) {
    for (auto &chr : svc->characteristics) {
      if (chr->handle == handle) {
        return chr.get();
      }
    }
  }
  return nullptr;
}

/**
 * @brief Finds a descriptor by its GATTS attribute handle.
 * @param impl Server state containing the service tree.
 * @param handle Attribute handle from the stack.
 * @return Pointer to the descriptor @c Impl, or null if not found.
 */
BLEDescriptor::Impl *findDescByHandle(BLEServer::Impl *impl, uint16_t handle) {
  for (auto &svc : impl->services) {
    for (auto &chr : svc->characteristics) {
      for (auto &desc : chr->descriptors) {
        if (desc->handle == handle) {
          return desc.get();
        }
      }
    }
  }
  return nullptr;
}

/**
 * @brief Finds the characteristic that owns a descriptor with the given handle.
 * @param impl Server state containing the service tree.
 * @param descHandle Attribute handle of the descriptor.
 * @return Parent characteristic @c Impl, or null if not found.
 */
BLECharacteristic::Impl *findCharForDesc(BLEServer::Impl *impl, uint16_t descHandle) {
  for (auto &svc : impl->services) {
    for (auto &chr : svc->characteristics) {
      for (auto &desc : chr->descriptors) {
        if (desc->handle == descHandle) {
          return chr.get();
        }
      }
    }
  }
  return nullptr;
}

}  // namespace

// --------------------------------------------------------------------------
// BLEServer public API -- Bluedroid backend
// --------------------------------------------------------------------------

// Bluedroid registers incrementally (create_service, add_char, add_char_descr, start_service
// per service), unlike NimBLE's full ble_gatts_reset and bulk re-register.
BTStatus BLEServer::start() {
  BLE_CHECK_IMPL(BTStatus::InvalidState);

  std::vector<std::shared_ptr<BLEService::Impl>> services;
  {
    BLELockGuard lock(impl.mtx);
    if (impl.started) {
      bool hasNew = false;
      for (auto &s : impl.services) {
        if (!s->started) {
          hasNew = true;
          break;
        }
      }
      if (!hasNew) {
        return BTStatus::OK;
      }
      log_d("Server: registering new service(s)");
    } else {
      log_d("Server: starting with %u service(s)", (unsigned)impl.services.size());
    }
    services = impl.services;
  }

  // First pass: create all unstarted services so every service obtains a GATT
  // handle. This is required before adding Included Service declarations
  // because the included service's handle must already be known.
  for (auto &svc : services) {
    if (svc->started || svc->handle != 0) {
      continue;
    }

    // Auto-create CCCD descriptors before counting handles.
    for (auto &chr : svc->characteristics) {
      if ((chr->properties & BLEProperty::Notify) || (chr->properties & BLEProperty::Indicate)) {
        bool hasCCCD = false;
        for (auto &d : chr->descriptors) {
          if (d->uuid == BLEUUID(CCCD_UUID16)) {
            hasCCCD = true;
            break;
          }
        }
        if (!hasCCCD) {
          auto cccd = std::make_shared<BLEDescriptor::Impl>();
          cccd->uuid = BLEUUID(CCCD_UUID16);
          cccd->chr = chr.get();
          cccd->permissions = BLEPermission::Read | BLEPermission::Write;
          cccd->value.resize(2, 0);
          chr->descriptors.push_back(cccd);
        }
      }
    }

    // 1 (service) + 1 per include + 2 per char + 1 per descriptor.
    uint32_t needed = 1 + svc->includedServices.size();
    for (const auto &chr : svc->characteristics) {
      needed += 2 + chr->descriptors.size();
    }
    if (svc->numHandles < needed) {
      svc->numHandles = needed;
    }

    log_d("Server: creating service %s", svc->uuid.toString().c_str());
    esp_gatt_srvc_id_t srvc_id = {};
    srvc_id.is_primary = true;
    srvc_id.id.inst_id = svc->instId;
    bluedroidUuidFromPublic(svc->uuid, srvc_id.id.uuid);

    impl.pendingHandle = &svc->handle;
    impl.createSync.take();
    esp_err_t err = esp_ble_gatts_create_service(impl.gattsIf, &srvc_id, svc->numHandles);
    if (err != ESP_OK) {
      log_e("esp_ble_gatts_create_service: %s", esp_err_to_name(err));
      impl.pendingHandle = nullptr;
      return BTStatus::Fail;
    }
    BTStatus st = impl.createSync.wait(5000);
    impl.pendingHandle = nullptr;
    if (st != BTStatus::OK) {
      log_e("Create service failed/timeout");
      return st;
    }
  }

  // Second pass: add included services, characteristics, descriptors, and start.
  for (auto &svc : services) {
    if (svc->started) {
      continue;
    }

    // 1. Add Included Service declarations (GATT §3.2).
    for (auto &inc : svc->includedServices) {
      if (inc->handle == 0) {
        log_e("Server: included service %s has no handle", inc->uuid.toString().c_str());
        return BTStatus::InvalidState;
      }
      uint16_t inclHandle = 0;
      impl.pendingHandle = &inclHandle;
      impl.createSync.take();
      esp_err_t err = esp_ble_gatts_add_included_service(svc->handle, inc->handle);
      if (err != ESP_OK) {
        log_e("esp_ble_gatts_add_included_service: %s", esp_err_to_name(err));
        impl.pendingHandle = nullptr;
        return BTStatus::Fail;
      }
      BTStatus st = impl.createSync.wait(5000);
      impl.pendingHandle = nullptr;
      if (st != BTStatus::OK) {
        log_e("Add included service failed/timeout");
        return st;
      }
    }

    // 2. Add characteristics
    for (auto &chr : svc->characteristics) {
      if (!bleValidateCharForRegistration(*chr, /*stackIsNimble=*/false)) {
        log_e("Server: characteristic %s rejected by validation", chr->uuid.toString().c_str());
        return BTStatus::InvalidState;
      }

      esp_bt_uuid_t char_uuid;
      bluedroidUuidFromPublic(chr->uuid, char_uuid);
      esp_gatt_perm_t perm = toEspGattPermissions(chr->permissions);
      esp_gatt_char_prop_t prop = toEspGattProperties(chr->properties, chr->permissions);

      impl.pendingHandle = &chr->handle;
      impl.createSync.take();
      esp_err_t err = esp_ble_gatts_add_char(svc->handle, &char_uuid, perm, prop, nullptr, nullptr);
      if (err != ESP_OK) {
        log_e("esp_ble_gatts_add_char: %s", esp_err_to_name(err));
        impl.pendingHandle = nullptr;
        return BTStatus::Fail;
      }
      BTStatus st = impl.createSync.wait(5000);
      impl.pendingHandle = nullptr;
      if (st != BTStatus::OK) {
        log_e("Add char failed/timeout");
        return st;
      }

      // 3. Add descriptors for this characteristic
      for (auto &desc : chr->descriptors) {
        esp_bt_uuid_t desc_uuid;
        bluedroidUuidFromPublic(desc->uuid, desc_uuid);
        esp_gatt_perm_t descPerm = toEspGattPermissions(desc->permissions);

        impl.pendingHandle = &desc->handle;
        impl.createSync.take();
        err = esp_ble_gatts_add_char_descr(svc->handle, &desc_uuid, descPerm, nullptr, nullptr);
        if (err != ESP_OK) {
          log_e("esp_ble_gatts_add_char_descr: %s", esp_err_to_name(err));
          impl.pendingHandle = nullptr;
          return BTStatus::Fail;
        }
        st = impl.createSync.wait(5000);
        impl.pendingHandle = nullptr;
        if (st != BTStatus::OK) {
          log_e("Add descr failed/timeout");
          return st;
        }
      }
    }

    // 4. Start service
    {
      impl.createSync.take();
      esp_err_t err = esp_ble_gatts_start_service(svc->handle);
      if (err != ESP_OK) {
        log_e("esp_ble_gatts_start_service: %s", esp_err_to_name(err));
        return BTStatus::Fail;
      }
      BTStatus st = impl.createSync.wait(5000);
      if (st != BTStatus::OK) {
        log_e("Start service failed/timeout");
        return st;
      }
    }
    log_i("Server: service %s started (handle=0x%04x)", svc->uuid.toString().c_str(), svc->handle);
    svc->started = true;
  }

  log_i("Server: started, %u service(s) registered", (unsigned)services.size());
  impl.started = true;
  return BTStatus::OK;
}

BTStatus BLEServer::disconnect(uint16_t connHandle, uint8_t /*reason*/) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  esp_err_t err = esp_ble_gatts_close(impl.gattsIf, connHandle);
  if (err != ESP_OK) {
    log_e("Server: esp_ble_gatts_close handle=%u: %s", connHandle, esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

// Blocks on connectSync until OPEN_EVT reports the result of esp_ble_gatts_open.
BTStatus BLEServer::connect(const BTAddress &address) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  esp_bd_addr_t bda;
  address.toEspBdAddr(bda);
  impl.connectSync.take();
  esp_err_t err = esp_ble_gatts_open(impl.gattsIf, bda, true);
  if (err != ESP_OK) {
    // Release the sync we just took: no OPEN_EVT will arrive to give() it.
    impl.connectSync.give(BTStatus::Fail);
    log_e("Server: esp_ble_gatts_open: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return impl.connectSync.wait(10000);
}

uint16_t BLEServer::getPeerMTU(uint16_t connHandle) const {
  BLE_CHECK_IMPL(0);
  BLELockGuard lock(impl.mtx);
  BLEConnInfo *info = const_cast<BLEServer::Impl &>(impl).connFind(connHandle);
  return info ? info->getMTU() : 23;
}

BTStatus BLEServer::updateConnParams(uint16_t connHandle, const BLEConnParams &params) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);

  if (!params.isValid()) {
    log_e("Server: updateConnParams — parameters out of spec");
    return BTStatus::InvalidParam;
  }

  BTAddress addr;
  {
    BLELockGuard lock(impl.mtx);
    BLEConnInfo *info = impl.connFind(connHandle);
    if (!info) {
      log_w("Server: updateConnParams - connection handle=%u not found", connHandle);
      return BTStatus::InvalidState;
    }
    addr = info->getAddress();
  }

  esp_ble_conn_update_params_t connParams = {};
  addr.toEspBdAddr(connParams.bda);
  connParams.min_int = params.minInterval;
  connParams.max_int = params.maxInterval;
  connParams.latency = params.latency;
  connParams.timeout = params.timeout;

  esp_err_t err = esp_ble_gap_update_conn_params(&connParams);
  if (err != ESP_OK) {
    log_e("Server: esp_ble_gap_update_conn_params: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

BTStatus BLEServer::setPhy(uint16_t connHandle, BLEPhy txPhy, BLEPhy rxPhy) {
#if BLE5_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  esp_bd_addr_t bda;
  {
    BLELockGuard lock(impl.mtx);
    BLEConnInfo *conn = impl.connFind(connHandle);
    if (!conn) {
      log_w("Server: setPhy - unknown connHandle %u", connHandle);
      return BTStatus::NotConnected;
    }
    conn->getAddress().toEspBdAddr(bda);
  }
  impl.phySync.take();
  esp_err_t err = esp_ble_gap_set_preferred_phy(
    bda, 0, blePhyToPrefMask(txPhy), blePhyToPrefMask(rxPhy), ESP_BLE_GAP_PHY_OPTIONS_NO_PREF
  );
  if (err != ESP_OK) {
    log_e("Server: esp_ble_gap_set_preferred_phy: %s", esp_err_to_name(err));
    impl.phySync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  return impl.phySync.wait(2000);
#else
  (void)connHandle;
  (void)txPhy;
  (void)rxPhy;
  log_w("%s not supported on Bluedroid (BLE 5.0 unavailable)", __func__);
  return BTStatus::NotSupported;
#endif
}

BTStatus BLEServer::getPhy(uint16_t connHandle, BLEPhy &txPhy, BLEPhy &rxPhy) const {
  txPhy = BLEPhy::PHY_1M;
  rxPhy = BLEPhy::PHY_1M;
#if BLE5_SUPPORTED
  if (!_impl) {
    return BTStatus::InvalidState;
  }
  auto &impl = *_impl;
  esp_bd_addr_t bda;
  {
    BLELockGuard lock(impl.mtx);
    BLEConnInfo *conn = impl.connFind(connHandle);
    if (!conn) {
      log_w("Server: getPhy - unknown connHandle %u", connHandle);
      return BTStatus::NotConnected;
    }
    conn->getAddress().toEspBdAddr(bda);
  }
  impl.phySync.take();
  esp_err_t err = esp_ble_gap_read_phy(bda);
  if (err != ESP_OK) {
    log_e("Server: esp_ble_gap_read_phy: %s", esp_err_to_name(err));
    impl.phySync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  BTStatus st = impl.phySync.wait(2000);
  if (st == BTStatus::OK) {
    txPhy = impl.pendingTxPhy;
    rxPhy = impl.pendingRxPhy;
  }
  return st;
#else
  (void)connHandle;
  log_w("%s not supported on Bluedroid (BLE 5.0 unavailable)", __func__);
  return BTStatus::NotSupported;
#endif
}

// Bluedroid's set_pkt_data_len takes only tx octets; txTime is accepted for API parity and ignored.
BTStatus BLEServer::setDataLen(uint16_t connHandle, uint16_t txOctets, uint16_t /*txTime*/) {
#if BLE5_SUPPORTED
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  esp_bd_addr_t bda;
  {
    BLELockGuard lock(impl.mtx);
    BLEConnInfo *conn = impl.connFind(connHandle);
    if (!conn) {
      log_w("Server: setDataLen - unknown connHandle %u", connHandle);
      return BTStatus::NotConnected;
    }
    conn->getAddress().toEspBdAddr(bda);
  }
  impl.dataLenSync.take();
  esp_err_t err = esp_ble_gap_set_pkt_data_len(bda, txOctets);
  if (err != ESP_OK) {
    log_e("Server: esp_ble_gap_set_pkt_data_len: %s", esp_err_to_name(err));
    impl.dataLenSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  return impl.dataLenSync.wait(2000);
#else
  (void)connHandle;
  (void)txOctets;
  log_w("%s not supported on Bluedroid (BLE 5.0 unavailable)", __func__);
  return BTStatus::NotSupported;
#endif
}

// Unused on the GATTS path: server GAP events are handled via handleGAP. Present for API symmetry.
int BLEServer::handleGapEvent(void * /*event*/) {
  return 0;
}

// --------------------------------------------------------------------------
// handleGATTS -- dispatches ESP GATTS events to server, services & chars
// --------------------------------------------------------------------------

/**
 * @brief Central GATTS event dispatcher: registration, service build, read/write, connections.
 * @param event GATTS callback event id.
 * @param gatts_if GATTS application interface, or @c ESP_GATT_IF_NONE for some events.
 * @param param Event parameters from the Bluedroid stack.
 */
void BLEServer::Impl::handleGATTS(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
  auto *impl = s_instance;
  if (!impl) {
    return;
  }

  // REG_EVT: match by app_id (gatts_if not assigned yet)
  if (event == ESP_GATTS_REG_EVT) {
    if (param->reg.app_id == impl->appId) {
      if (param->reg.status == ESP_GATT_OK) {
        impl->gattsIf = gatts_if;
        impl->regSync.give(BTStatus::OK);
      } else {
        log_e("GATTS REG failed: status=%d", param->reg.status);
        impl->regSync.give(BTStatus::Fail);
      }
    }
    return;
  }

  // Ignore events for other GATTS interfaces
  if (gatts_if != ESP_GATT_IF_NONE && gatts_if != impl->gattsIf) {
    return;
  }

  switch (event) {

      // ----- Connection lifecycle -----

    case ESP_GATTS_CONNECT_EVT:
    {
      uint16_t connId = param->connect.conn_id;
      log_i("Server: client connected, connId=%u", connId);
      BLEConnInfo connInfo = BLEConnInfoImpl::make(
        connId, param->connect.remote_bda, 23, false, static_cast<BTAddress::Type>(param->connect.ble_addr_type)
      );
      {
        BLELockGuard lock(impl->mtx);
        impl->connSet(connId, connInfo);
      }
      impl->dispatchConnect(connInfo);
      break;
    }

    case ESP_GATTS_DISCONNECT_EVT:
    {
      uint16_t connId = param->disconnect.conn_id;
      uint8_t reason = static_cast<uint8_t>(param->disconnect.reason);
      log_i("Server: client disconnected, connId=%u reason=0x%02x", connId, reason);
      BLEConnInfo connInfo;
      bool shouldAdvertise = false;
      {
        BLELockGuard lock(impl->mtx);
        if (const BLEConnInfo *existing = impl->connFind(connId)) {
          connInfo = *existing;
        } else {
          connInfo = BLEConnInfoImpl::make(connId, param->disconnect.remote_bda);
        }
        // Clean up subscriber state for this connection
        for (auto &svc : impl->services) {
          for (auto &chr : svc->characteristics) {
            auto &subs = chr->subscribers;
            for (auto it = subs.begin(); it != subs.end(); ++it) {
              if (it->first == connId) {
                subs.erase(it);
                break;
              }
            }
          }
        }
        impl->connErase(connId);
        shouldAdvertise = impl->advertiseOnDisconnect;
      }
      impl->dispatchDisconnect(connInfo, reason);
      if (shouldAdvertise && impl->advRestartTimer) {
        // Defer to esp_timer task — calling BLE.startAdvertising() directly
        // from the BTC callback would deadlock (BLESync blocks for GAP
        // events that are also delivered on the BTC task).
        esp_timer_start_once(impl->advRestartTimer, 0);
      }
      break;
    }

    case ESP_GATTS_MTU_EVT:
    {
      uint16_t connId = param->mtu.conn_id;
      uint16_t mtu = param->mtu.mtu;
      log_d("Server: MTU changed connId=%u mtu=%u", connId, mtu);

      BLEConnInfo connInfo;
      {
        BLELockGuard lock(impl->mtx);
        BLEConnInfo *existing = impl->connFind(connId);
        if (existing) {
          BLEConnInfoImpl::setMTU(*existing, mtu);
          connInfo = *existing;
        }
      }
      if (connInfo) {
        impl->dispatchMtuChanged(connInfo, mtu);
      }
      break;
    }

      // ----- Service/characteristic registration (sequential from start()) -----

    case ESP_GATTS_CREATE_EVT:
    {
      if (param->create.status == ESP_GATT_OK && impl->pendingHandle) {
        *impl->pendingHandle = param->create.service_handle;
      }
      impl->createSync.give(param->create.status == ESP_GATT_OK ? BTStatus::OK : BTStatus::Fail);
      break;
    }

    case ESP_GATTS_ADD_INCL_SRVC_EVT:
    {
      if (param->add_incl_srvc.status == ESP_GATT_OK && impl->pendingHandle) {
        *impl->pendingHandle = param->add_incl_srvc.attr_handle;
      }
      impl->createSync.give(param->add_incl_srvc.status == ESP_GATT_OK ? BTStatus::OK : BTStatus::Fail);
      break;
    }

    case ESP_GATTS_ADD_CHAR_EVT:
    {
      if (param->add_char.status == ESP_GATT_OK && impl->pendingHandle) {
        *impl->pendingHandle = param->add_char.attr_handle;
      }
      impl->createSync.give(param->add_char.status == ESP_GATT_OK ? BTStatus::OK : BTStatus::Fail);
      break;
    }

    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
    {
      if (param->add_char_descr.status == ESP_GATT_OK && impl->pendingHandle) {
        *impl->pendingHandle = param->add_char_descr.attr_handle;
      }
      impl->createSync.give(param->add_char_descr.status == ESP_GATT_OK ? BTStatus::OK : BTStatus::Fail);
      break;
    }

    case ESP_GATTS_START_EVT:
    {
      impl->createSync.give(param->start.status == ESP_GATT_OK ? BTStatus::OK : BTStatus::Fail);
      break;
    }

    case ESP_GATTS_DELETE_EVT:
    {
      impl->createSync.give(param->del.status == ESP_GATT_OK ? BTStatus::OK : BTStatus::Fail);
      break;
    }

      // ----- Read / Write dispatch -----

    case ESP_GATTS_READ_EVT:
    {
      uint16_t handle = param->read.handle;
      uint16_t connId = param->read.conn_id;
      uint32_t transId = param->read.trans_id;
      uint16_t offset = param->read.offset;

      BLECharacteristic::Impl *chr = findCharByHandle(impl, handle);
      BLEDescriptor::Impl *desc = chr ? nullptr : findDescByHandle(impl, handle);

      if (chr) {
        log_d("Server: read characteristic handle=0x%04x connId=%u offset=%u", handle, connId, offset);
      } else if (desc) {
        log_d("Server: read descriptor handle=0x%04x connId=%u offset=%u", handle, connId, offset);
      } else {
        log_w("Server: read unknown handle=0x%04x connId=%u", handle, connId);
      }

      if (chr) {
        // Software-level authorization gate for ReadAuthorized
        if (chr->permissions & BLEPermission::ReadAuthorized) {
          BLEConnInfo connInfo;
          {
            BLELockGuard lock(impl->mtx);
            BLEConnInfo *ci = impl->connFind(connId);
            if (ci) {
              connInfo = *ci;
            }
          }
          auto *sec = BLESecurity::Impl::instance();
          if (!(sec && sec->notifyAuthorization(connInfo, handle, true))) {
            if (param->read.need_rsp) {
              if (esp_ble_gatts_send_response(gatts_if, connId, transId, ESP_GATT_INSUF_AUTHORIZATION, nullptr) != ESP_OK) {
                log_e("esp_ble_gatts_send_response failed");
              }
            }
            break;
          }
        }

        if (offset == 0) {
          BLECharacteristic::ReadHandler readCb;
          BLEConnInfo connInfo;
          {
            BLELockGuard lock(impl->mtx);
            readCb = chr->onReadCb;
            BLEConnInfo *ci = impl->connFind(connId);
            if (ci) {
              connInfo = *ci;
            }
          }
          if (readCb) {
            auto chrHandle = BLECharacteristicImplCommon::makeHandle(chr->shared_from_this());
            readCb(chrHandle, connInfo);
          }
        }

        if (param->read.need_rsp) {
          esp_gatt_rsp_t rsp = {};
          rsp.attr_value.handle = handle;
          rsp.attr_value.offset = offset;
          {
            // value is guarded by the per-characteristic mtx (same lock as the
            // public getValue/setValue), not the server mtx.
            BLELockGuard lock(chr->mtx);
            size_t len = chr->value.size();
            if (offset < len) {
              size_t sendLen = len - offset;
              if (sendLen > ESP_GATT_MAX_ATTR_LEN) {
                sendLen = ESP_GATT_MAX_ATTR_LEN;
              }
              rsp.attr_value.len = sendLen;
              memcpy(rsp.attr_value.value, chr->value.data() + offset, sendLen);
            }
          }
          if (esp_ble_gatts_send_response(gatts_if, connId, transId, ESP_GATT_OK, &rsp) != ESP_OK) {
            log_e("esp_ble_gatts_send_response failed");
          }
        }
      } else if (desc) {
        if (offset == 0) {
          BLEDescriptor::ReadHandler readCb;
          BLEConnInfo connInfo;
          {
            BLELockGuard lock(impl->mtx);
            readCb = desc->onReadCb;
            BLEConnInfo *ci = impl->connFind(connId);
            if (ci) {
              connInfo = *ci;
            }
          }
          if (readCb) {
            auto descHandle = BLEDescriptorImplCommon::makeHandle(desc->shared_from_this());
            readCb(descHandle, connInfo);
          }
        }

        if (param->read.need_rsp) {
          esp_gatt_rsp_t rsp = {};
          rsp.attr_value.handle = handle;
          rsp.attr_value.offset = offset;
          {
            BLELockGuard lock(desc->mtx);
            size_t len = desc->value.size();
            if (offset < len) {
              size_t sendLen = len - offset;
              if (sendLen > ESP_GATT_MAX_ATTR_LEN) {
                sendLen = ESP_GATT_MAX_ATTR_LEN;
              }
              rsp.attr_value.len = sendLen;
              memcpy(rsp.attr_value.value, desc->value.data() + offset, sendLen);
            }
          }
          if (esp_ble_gatts_send_response(gatts_if, connId, transId, ESP_GATT_OK, &rsp) != ESP_OK) {
            log_e("esp_ble_gatts_send_response failed");
          }
        }
      } else {
        if (param->read.need_rsp) {
          if (esp_ble_gatts_send_response(gatts_if, connId, transId, ESP_GATT_NOT_FOUND, nullptr) != ESP_OK) {
            log_e("esp_ble_gatts_send_response failed");
          }
        }
      }
      break;
    }

    case ESP_GATTS_WRITE_EVT:
    {
      uint16_t handle = param->write.handle;
      uint16_t connId = param->write.conn_id;
      uint32_t transId = param->write.trans_id;
      bool needRsp = param->write.need_rsp;
      bool isPrep = param->write.is_prep;
      log_d("Server: write handle=0x%04x connId=%u len=%u needRsp=%d isPrep=%d", handle, connId, param->write.len, needRsp, isPrep);

      if (isPrep) {
        PrepWrite pw;
        pw.handle = handle;
        pw.offset = param->write.offset;
        pw.data.assign(param->write.value, param->write.value + param->write.len);
        impl->prepWritesFor(connId).push_back(std::move(pw));
        if (needRsp) {
          esp_gatt_rsp_t rsp = {};
          rsp.attr_value.handle = handle;
          rsp.attr_value.offset = param->write.offset;
          rsp.attr_value.len = param->write.len;
          if (param->write.len <= ESP_GATT_MAX_ATTR_LEN) {
            memcpy(rsp.attr_value.value, param->write.value, param->write.len);
          }
          if (esp_ble_gatts_send_response(gatts_if, connId, transId, ESP_GATT_OK, &rsp) != ESP_OK) {
            log_e("esp_ble_gatts_send_response failed");
          }
        }
        break;
      }

      BLECharacteristic::Impl *chr = findCharByHandle(impl, handle);
      BLEDescriptor::Impl *desc = chr ? nullptr : findDescByHandle(impl, handle);

      if (chr) {
        // Software-level authorization gate for WriteAuthorized
        if (chr->permissions & BLEPermission::WriteAuthorized) {
          BLEConnInfo connInfo;
          {
            BLELockGuard lock(impl->mtx);
            BLEConnInfo *ci = impl->connFind(connId);
            if (ci) {
              connInfo = *ci;
            }
          }
          auto *sec = BLESecurity::Impl::instance();
          if (!(sec && sec->notifyAuthorization(connInfo, handle, false))) {
            if (needRsp) {
              if (esp_ble_gatts_send_response(gatts_if, connId, transId, ESP_GATT_INSUF_AUTHORIZATION, nullptr) != ESP_OK) {
                log_e("esp_ble_gatts_send_response failed");
              }
            }
            break;
          }
        }

        {
          BLECharacteristic::WriteHandler writeCb;
          BLEConnInfo connInfo;
          {
            // value is guarded by the per-characteristic mtx (same lock as the
            // public getValue/setValue and the read/notify paths).
            BLELockGuard lock(chr->mtx);
            chr->value.assign(param->write.value, param->write.value + param->write.len);
          }
          {
            BLELockGuard lock(impl->mtx);
            writeCb = chr->onWriteCb;
            BLEConnInfo *ci = impl->connFind(connId);
            if (ci) {
              connInfo = *ci;
            }
          }
          if (writeCb) {
            auto chrHandle = BLECharacteristicImplCommon::makeHandle(chr->shared_from_this());
            writeCb(chrHandle, connInfo);
          }
        }
        if (needRsp) {
          if (esp_ble_gatts_send_response(gatts_if, connId, transId, ESP_GATT_OK, nullptr) != ESP_OK) {
            log_e("esp_ble_gatts_send_response failed");
          }
        }
      } else if (desc) {
        {
          BLELockGuard lock(desc->mtx);
          desc->value.assign(param->write.value, param->write.value + param->write.len);
        }

        // Handle CCCD writes (subscription state)
        if (desc->uuid == BLEUUID(CCCD_UUID16) && param->write.len >= 2) {
          uint16_t subVal = param->write.value[0] | (param->write.value[1] << 8);
          BLECharacteristic::Impl *ownerChr = findCharForDesc(impl, handle);
          if (ownerChr) {
            BLECharacteristic::SubscribeHandler subCb;
            BLEConnInfo connInfo;
            {
              BLELockGuard lock(impl->mtx);
              if (subVal > 0) {
                bool found = false;
                for (auto &kv : ownerChr->subscribers) {
                  if (kv.first == connId) {
                    kv.second = subVal;
                    found = true;
                    break;
                  }
                }
                if (!found) {
                  ownerChr->subscribers.emplace_back(connId, subVal);
                }
              } else {
                for (auto it = ownerChr->subscribers.begin(); it != ownerChr->subscribers.end(); ++it) {
                  if (it->first == connId) {
                    ownerChr->subscribers.erase(it);
                    break;
                  }
                }
              }
              subCb = ownerChr->onSubscribeCb;
              BLEConnInfo *ci = impl->connFind(connId);
              if (ci) {
                connInfo = *ci;
              }
            }
            if (subCb) {
              auto chrHandle = BLECharacteristicImplCommon::makeHandle(ownerChr->shared_from_this());
              subCb(chrHandle, connInfo, subVal);
            }
          }
        } else {
          BLEDescriptor::WriteHandler writeCb;
          BLEConnInfo connInfo;
          {
            BLELockGuard lock(impl->mtx);
            writeCb = desc->onWriteCb;
            BLEConnInfo *ci = impl->connFind(connId);
            if (ci) {
              connInfo = *ci;
            }
          }
          if (writeCb) {
            auto descHandle = BLEDescriptorImplCommon::makeHandle(desc->shared_from_this());
            writeCb(descHandle, connInfo);
          }
        }
        if (needRsp) {
          if (esp_ble_gatts_send_response(gatts_if, connId, transId, ESP_GATT_OK, nullptr) != ESP_OK) {
            log_e("esp_ble_gatts_send_response failed");
          }
        }
      } else {
        if (needRsp) {
          if (esp_ble_gatts_send_response(gatts_if, connId, transId, ESP_GATT_NOT_FOUND, nullptr) != ESP_OK) {
            log_e("esp_ble_gatts_send_response failed");
          }
        }
      }
      break;
    }

    case ESP_GATTS_EXEC_WRITE_EVT:
    {
      uint16_t connId = param->exec_write.conn_id;
      bool execute = (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC);

      auto *pending = impl->findPrepWrites(connId);
      if (execute && pending != nullptr && !pending->empty()) {
        // Assemble all fragments per handle into a contiguous buffer.
        // Fragments arrive in offset order so we can simply resize and copy.
        auto &connPrepWrites = *pending;
        uint16_t targetHandle = connPrepWrites[0].handle;
        std::vector<uint8_t> assembled;
        for (auto &pw : connPrepWrites) {
          size_t end = pw.offset + pw.data.size();
          if (end > assembled.size()) {
            assembled.resize(end);
          }
          memcpy(assembled.data() + pw.offset, pw.data.data(), pw.data.size());
        }

        BLECharacteristic::Impl *chr = findCharByHandle(impl, targetHandle);
        if (chr) {
          {
            BLELockGuard lock(chr->mtx);
            chr->value = std::move(assembled);
          }
          BLECharacteristic::WriteHandler writeCb;
          BLEConnInfo connInfo;
          {
            BLELockGuard lock(impl->mtx);
            writeCb = chr->onWriteCb;
            BLEConnInfo *ci = impl->connFind(connId);
            if (ci) {
              connInfo = *ci;
            }
          }
          if (writeCb) {
            auto chrHandle = BLECharacteristicImplCommon::makeHandle(chr->shared_from_this());
            writeCb(chrHandle, connInfo);
          }
        }
      }
      impl->erasePrepWrites(connId);

      if (esp_ble_gatts_send_response(gatts_if, connId, param->exec_write.trans_id, ESP_GATT_OK, nullptr) != ESP_OK) {
        log_e("esp_ble_gatts_send_response failed");
      }
      break;
    }

    case ESP_GATTS_CONF_EVT:
    {
      if (param->conf.status != ESP_GATT_OK) {
        log_w("Indicate confirm failed, status=%d", param->conf.status);
      }
      break;
    }

      // ----- connect() blocking -----

    case ESP_GATTS_OPEN_EVT:
    {
      impl->connectSync.give(param->open.status == ESP_GATT_OK ? BTStatus::OK : BTStatus::Fail);
      break;
    }

    default: break;
  }
}

// --------------------------------------------------------------------------
// handleGAP -- connection parameter updates
// --------------------------------------------------------------------------

/**
 * @brief GAP event handler for the server, currently connection parameter update completion.
 * @param event GAP event id.
 * @param param GAP event parameters; used for @c conn params in the update event.
 */
void BLEServer::Impl::handleGAP(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  auto *impl = s_instance;
  if (!impl) {
    return;
  }

  switch (event) {
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
    {
      if (param->update_conn_params.status != ESP_BT_STATUS_SUCCESS) {
        break;
      }

      BLEConnInfo connInfo;
      {
        BLELockGuard lock(impl->mtx);
        for (auto &entry : impl->connections) {
          if (memcmp(entry.second.getAddress().data(), param->update_conn_params.bda, 6) == 0) {
            BLEConnInfoImpl::setConnParams(
              entry.second, param->update_conn_params.conn_int, param->update_conn_params.latency, param->update_conn_params.timeout
            );
            connInfo = entry.second;
            break;
          }
        }
      }
      if (connInfo) {
        impl->dispatchConnParamsUpdate(connInfo);
      }
      break;
    }
#if BLE5_SUPPORTED
    case ESP_GAP_BLE_READ_PHY_COMPLETE_EVT:
    {
      BTStatus st = (param->read_phy.status == ESP_BT_STATUS_SUCCESS) ? BTStatus::OK : BTStatus::Fail;
      if (st == BTStatus::OK) {
        BLELockGuard lock(impl->mtx);
        for (auto &entry : impl->connections) {
          if (memcmp(entry.second.getAddress().data(), param->read_phy.bda, 6) == 0) {
            BLEConnInfoImpl::setPhy(entry.second, param->read_phy.tx_phy, param->read_phy.rx_phy);
            impl->pendingTxPhy = static_cast<BLEPhy>(param->read_phy.tx_phy);
            impl->pendingRxPhy = static_cast<BLEPhy>(param->read_phy.rx_phy);
            break;
          }
        }
      }
      impl->phySync.give(st);
      break;
    }
    case ESP_GAP_BLE_PHY_UPDATE_COMPLETE_EVT:
    {
      BTStatus st = (param->phy_update.status == ESP_BT_STATUS_SUCCESS) ? BTStatus::OK : BTStatus::Fail;
      if (st == BTStatus::OK) {
        BLELockGuard lock(impl->mtx);
        for (auto &entry : impl->connections) {
          if (memcmp(entry.second.getAddress().data(), param->phy_update.bda, 6) == 0) {
            BLEConnInfoImpl::setPhy(entry.second, param->phy_update.tx_phy, param->phy_update.rx_phy);
            break;
          }
        }
      }
      impl->phySync.give(st);
      break;
    }
    case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
      // Completion has no BDA; only one dataLenSync waiter is in flight at a time.
      impl->dataLenSync.give(
        param->pkt_data_length_cmpl.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail
      );
      break;
#endif /* BLE5_SUPPORTED */
#if BLE_SMP_SUPPORTED
    // The peer's identity is known once pairing/encryption completes, reported
    // here as AUTH_CMPL. Fire onIdentity for the matching link on success.
    case ESP_GAP_BLE_AUTH_CMPL_EVT:
    {
      if (!param->ble_security.auth_cmpl.success) {
        break;
      }
      BLEConnInfo connInfo;
      {
        BLELockGuard lock(impl->mtx);
        for (auto &entry : impl->connections) {
          if (memcmp(entry.second.getAddress().data(), param->ble_security.auth_cmpl.bd_addr, 6) == 0) {
            BLEConnInfoImpl::setAddressType(entry.second, static_cast<BTAddress::Type>(param->ble_security.auth_cmpl.addr_type));
            BLEConnInfoImpl::updateSecurityFromAuthComplete(entry.second, param->ble_security.auth_cmpl.auth_mode);
            connInfo = entry.second;
            break;
          }
        }
      }
      if (connInfo) {
        impl->dispatchIdentityResolved(connInfo);
      }
      break;
    }
#endif /* BLE_SMP_SUPPORTED */
    default: break;
  }
}

// --------------------------------------------------------------------------
// GATTS app registration (used by BLEClass::createServer in BluedroidCore.cpp)
// --------------------------------------------------------------------------

bool bluedroidRegisterGattsApp(const std::shared_ptr<BLEServer::Impl> &server) {
  server->regSync.take();
  esp_err_t err = esp_ble_gatts_app_register(server->appId);
  if (err != ESP_OK) {
    log_e("esp_ble_gatts_app_register: %s", esp_err_to_name(err));
    return false;
  }
  BTStatus st = server->regSync.wait(5000);
  if (st != BTStatus::OK) {
    log_e("GATTS app registration failed/timeout");
    return false;
  }
  return true;
}

// --------------------------------------------------------------------------
// Dynamic service removal (esp_ble_gatts_delete_service)
// --------------------------------------------------------------------------

/**
 * @brief Clears service, characteristic, and descriptor handles after a delete or teardown.
 * @param svc Service implementation whose handles and started flag are reset to force re-add.
 */
static void bluedroidClearServiceHandles(BLEService::Impl *svc) {
  if (!svc) {
    return;
  }
  svc->handle = 0;
  svc->started = false;
  for (auto &c : svc->characteristics) {
    c->handle = 0;
    for (auto &d : c->descriptors) {
      d->handle = 0;
    }
  }
}

// Called by BLEClass::end() to mark the server stale; see @ref invalidate.

/**
 * @brief Resets GATTS interface and attribute handles so a later @ref BLEClass::createServer
 *        will call @c esp_ble_gatts_app_register again.
 */
void BLEServer::Impl::invalidate() {
  auto *srv = s_instance;
  if (!srv) {
    return;
  }
  srv->gattsIf = ESP_GATT_IF_NONE;
  srv->started = false;
  for (auto &svc : srv->services) {
    bluedroidClearServiceHandles(svc.get());
  }
}

// Uses esp_ble_gatts_delete_service and clears local handles, instead of a NimBLE-wide
// ble_gatts_reset and full re-register of every service.
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

  if (!svc->started || !impl->started) {
    BLELockGuard lock(impl->mtx);
    for (auto it = impl->services.begin(); it != impl->services.end(); ++it) {
      if (it->get() == svc.get()) {
        impl->services.erase(it);
        break;
      }
    }
    return BTStatus::OK;
  }

  impl->createSync.take();
  esp_err_t err = esp_ble_gatts_delete_service(svc->handle);
  if (err != ESP_OK) {
    impl->createSync.give(BTStatus::Fail);
    log_e("esp_ble_gatts_delete_service: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  BTStatus st = impl->createSync.wait(5000);
  if (st != BTStatus::OK) {
    log_e("Server: delete service failed or timed out");
    return st;
  }

  bluedroidClearServiceHandles(svc.get());
  {
    BLELockGuard lock(impl->mtx);
    for (auto it = impl->services.begin(); it != impl->services.end(); ++it) {
      if (it->get() == svc.get()) {
        impl->services.erase(it);
        break;
      }
    }
  }
  return BTStatus::OK;
}

#else /* !BLE_GATT_SERVER_SUPPORTED -- stubs */

#include "BLE.h"
#include "impl/BLEBackend.h"
#include "esp32-hal-log.h"

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

BTStatus bleServerRemoveService(BLEServer::Impl *, std::shared_ptr<BLEService::Impl>) {
  return BTStatus::NotSupported;
}

#endif /* BLE_GATT_SERVER_SUPPORTED */

#endif /* BLE_BLUEDROID */
