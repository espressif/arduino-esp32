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

#include "impl/BLEGuards.h"
#if BLE_BLUEDROID

#if BLE_GATT_SERVER_SUPPORTED

#include "BLE.h"

#include "BluedroidServer.h"
#include "BluedroidService.h"
#include "BluedroidCharacteristic.h"
#include "BluedroidUUID.h"
#include "impl/BLESync.h"
#include "impl/BLEImplHelpers.h"
#include "impl/BLECharacteristicValidation.h"
#include "impl/BLEConnInfoData.h"
#include "impl/BLEMutex.h"
#include "impl/BLEServerBackend.h"
#include "impl/BLESecurityBackend.h"
#include "esp32-hal-log.h"

#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <esp_bt_device.h>
#include <string.h>

// --------------------------------------------------------------------------
// Helpers
// --------------------------------------------------------------------------

namespace {

// Pure mapping from user-declared permissions to Bluedroid's esp_gatt_perm_t;
// read @ref permToEsp documentation for the authorize-bit behavior.

/**
 * @brief Maps @ref BLEPermission to Bluedroid @c esp_gatt_perm_t for GATT registration.
 * @param perm Declared attribute permissions.
 * @return Bluedroid permission bitfield for @c esp_ble_gatts_add_char and descriptors.
 * @note Unlike NimBLE, there are no separate read/write authorize perm bits; authorized
 *       access is implemented in software in @ref BLEServer::Impl::handleGATTS.
 */
esp_gatt_perm_t permToEsp(BLEPermission perm) {
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

// Fail-closed property mapping: see @ref propToEsp.

/**
 * @brief Maps @ref BLEProperty and @ref BLEPermission to Bluedroid characteristic properties.
 * @param props Requested GATT property bits to advertise.
 * @param perms Attribute permissions; read/write GATT property bits are gated on these.
 * @return @c esp_gatt_char_prop_t for @c esp_ble_gatts_add_char.
 */
esp_gatt_char_prop_t propToEsp(BLEProperty props, BLEPermission perms) {
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

constexpr uint16_t CCCD_UUID16 = 0x2902;

}  // namespace

// --------------------------------------------------------------------------
// BLEConnInfoImpl -- Bluedroid bridge
// --------------------------------------------------------------------------

struct BLEConnInfoImpl {
  /**
   * @brief Constructs a @ref BLEConnInfo for a Bluedroid GATTS connection.
   * @param connId GATTS connection ID.
   * @param bda Remote device address (6 bytes, little-endian BDA order as from stack).
   * @param mtu Initial ATT MTU; default 23.
   * @return Populated @ref BLEConnInfo marked valid.
   */
  static BLEConnInfo make(uint16_t connId, const uint8_t bda[6], uint16_t mtu = 23) {
    BLEConnInfo info;
    info._valid = true;
    auto *d = info.data();
    d->handle = connId;
    d->address = BTAddress(bda, BTAddress::Type::Public);
    d->mtu = mtu;
    d->central = false;
    d->encrypted = false;
    d->authenticated = false;
    d->bonded = false;
    d->keySize = 0;
    d->interval = 0;
    d->latency = 0;
    d->supervisionTimeout = 0;
    d->txPhy = 1;
    d->rxPhy = 1;
    d->rssi = 0;
    return info;
  }

  /**
   * @brief Updates stored MTU in a @ref BLEConnInfo when valid.
   * @param info Connection info to update in place.
   * @param mtu New negotiated MTU.
   */
  static void setMTU(BLEConnInfo &info, uint16_t mtu) {
    if (info) {
      info.data()->mtu = mtu;
    }
  }

  /**
   * @brief Updates connection interval, latency, and supervision timeout when valid.
   * @param info Connection info to update in place.
   * @param interval Connection interval in 1.25ms units.
   * @param latency Slave latency in connection events.
   * @param timeout Supervision timeout in 10ms units.
   */
  static void setConnParams(BLEConnInfo &info, uint16_t interval, uint16_t latency, uint16_t timeout) {
    if (!info) {
      return;
    }
    auto *d = info.data();
    d->interval = interval;
    d->latency = latency;
    d->supervisionTimeout = timeout;
  }
};

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

/**
 * @brief Registers and starts GATT services that are not yet @c started on the GATTS interface.
 * @return @c BTStatus::OK when all new services are created and started, or an error status.
 * @note Bluedroid registers incrementally: @c create_service, @c add_char, @c add_char_descr, @c
 *       start_service per service, unlike NimBLE full @c ble_gatts_reset and bulk re-register.
 */
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

  // Pass 1: Create all unstarted services so every service obtains a GATT
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
    uuidToEsp(svc->uuid, srvc_id.id.uuid);

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

  // Pass 2: Add included services, characteristics, descriptors, and start.
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
      if (!bleValidateCharFinal(*chr, /*stackIsNimble=*/false)) {
        log_e("Server: characteristic %s rejected by validation", chr->uuid.toString().c_str());
        return BTStatus::InvalidState;
      }

      esp_bt_uuid_t char_uuid;
      uuidToEsp(chr->uuid, char_uuid);
      esp_gatt_perm_t perm = permToEsp(chr->permissions);
      esp_gatt_char_prop_t prop = propToEsp(chr->properties, chr->permissions);

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
        uuidToEsp(desc->uuid, desc_uuid);
        esp_gatt_perm_t descPerm = permToEsp(desc->permissions);

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

/**
 * @brief Closes a GATTS connection by connection ID.
 * @param connHandle GATTS connection id (not necessarily same encoding as all stacks).
 * @param reason Reserved/unused; Bluedroid path uses GATTS close.
 * @return @c OK on success, or @c InvalidState / @c Fail on error.
 */
BTStatus BLEServer::disconnect(uint16_t connHandle, uint8_t /*reason*/) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  esp_err_t err = esp_ble_gatts_close(impl.gattsIf, connHandle);
  if (err != ESP_OK) {
    log_e("Server: esp_ble_gatts_close handle=%u: %s", connHandle, esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

/**
 * @brief Opens an outgoing GATTS connection to a central (blocking on sync object).
 * @param address Target Bluetooth address.
 * @return @c OK on success from @c OPEN_EVT, or failure if open API or wait fails.
 */
BTStatus BLEServer::connect(const BTAddress &address) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  esp_bd_addr_t bda;
  memcpy(bda, address.data(), 6);
  impl.connectSync.take();
  esp_err_t err = esp_ble_gatts_open(impl.gattsIf, bda, true);
  if (err != ESP_OK) {
    log_e("Server: esp_ble_gatts_open: %s", esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return impl.connectSync.wait(10000);
}

/**
 * @brief Returns the negotiated per-connection MTU from tracked connection state.
 * @param connHandle GATTS connection id.
 * @return MTU in octets, or 23 if the connection is unknown and defaulting.
 */
uint16_t BLEServer::getPeerMTU(uint16_t connHandle) const {
  BLE_CHECK_IMPL(0);
  BLELockGuard lock(impl.mtx);
  BLEConnInfo *info = const_cast<BLEServer::Impl &>(impl).connFind(connHandle);
  return info ? info->getMTU() : 23;
}

/**
 * @brief Requests a connection parameter update using the known peer address.
 * @param connHandle GATTS connection id used to look up the BDA.
 * @param params Desired min/max interval, latency, and supervision timeout.
 * @return @c OK, @c InvalidState if the connection is unknown, @c InvalidParam if out of spec, or @c Fail.
 * @note Connection parameter constraints per BT Core Spec v5.x, Vol 6, Part B, §4.5.1:
 *       interval 6–3200 (1.25 ms units), latency 0–499, timeout 10–3200 (10 ms units),
 *       timeout > (1 + latency) × maxInterval × 2.
 */
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
  memcpy(connParams.bda, addr.data(), 6);
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

/**
 * @brief LE PHY selection is not implemented on the Bluedroid GATTS path in this build.
 * @param connHandle Unused.
 * @param txPhy Unused.
 * @param rxPhy Unused.
 * @return Always @c BTStatus::NotSupported.
 */
BTStatus BLEServer::setPhy(uint16_t /*connHandle*/, BLEPhy /*txPhy*/, BLEPhy /*rxPhy*/) {
  log_w("%s not supported on Bluedroid", __func__);
  return BTStatus::NotSupported;
}

/**
 * @brief LE PHY read is not implemented on the Bluedroid GATTS path in this build.
 * @param connHandle Unused.
 * @param txPhy Unused.
 * @param rxPhy Unused.
 * @return Always @c BTStatus::NotSupported.
 */
BTStatus BLEServer::getPhy(uint16_t /*connHandle*/, BLEPhy & /*txPhy*/, BLEPhy & /*rxPhy*/) const {
  log_w("%s not supported on Bluedroid", __func__);
  return BTStatus::NotSupported;
}

/**
 * @brief Data length extension is not implemented on the Bluedroid GATTS path in this build.
 * @param connHandle Unused.
 * @param txOctets Unused.
 * @param txTime Unused.
 * @return Always @c BTStatus::NotSupported.
 */
BTStatus BLEServer::setDataLen(uint16_t /*connHandle*/, uint16_t /*txOctets*/, uint16_t /*txTime*/) {
  log_w("%s not supported on Bluedroid", __func__);
  return BTStatus::NotSupported;
}

/**
 * @brief Gap event hook for API symmetry; GATTS uses @ref handleGATTS and @ref handleGAP.
 * @param event Unused in Bluedroid server.
 * @return Always 0.
 */
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
      BLEConnInfo connInfo = BLEConnInfoImpl::make(connId, param->connect.remote_bda);
      {
        BLELockGuard lock(impl->mtx);
        impl->connSet(connId, connInfo);
      }
      ble_server_dispatch::dispatchConnect(impl, connInfo);
      break;
    }

    case ESP_GATTS_DISCONNECT_EVT:
    {
      uint16_t connId = param->disconnect.conn_id;
      uint8_t reason = static_cast<uint8_t>(param->disconnect.reason);
      log_i("Server: client disconnected, connId=%u reason=0x%02x", connId, reason);
      BLEConnInfo connInfo = BLEConnInfoImpl::make(connId, param->disconnect.remote_bda);

      bool shouldAdvertise = false;
      {
        BLELockGuard lock(impl->mtx);
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
      ble_server_dispatch::dispatchDisconnect(impl, connInfo, reason);
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
        ble_server_dispatch::dispatchMtuChanged(impl, connInfo, mtu);
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
          BLESecurity sec = BLE.getSecurity();
          if (!BLESecurityBackend::notifyAuthorization(sec, connInfo, handle, true)) {
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
            auto chrHandle = BLECharacteristic(chr->shared_from_this());
            readCb(chrHandle, connInfo);
          }
        }

        if (param->read.need_rsp) {
          esp_gatt_rsp_t rsp = {};
          rsp.attr_value.handle = handle;
          rsp.attr_value.offset = offset;
          size_t len = chr->value.size();
          if (offset < len) {
            size_t sendLen = len - offset;
            if (sendLen > ESP_GATT_MAX_ATTR_LEN) {
              sendLen = ESP_GATT_MAX_ATTR_LEN;
            }
            rsp.attr_value.len = sendLen;
            memcpy(rsp.attr_value.value, chr->value.data() + offset, sendLen);
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
            auto descHandle = BLEDescriptor(desc->shared_from_this());
            readCb(descHandle, connInfo);
          }
        }

        if (param->read.need_rsp) {
          esp_gatt_rsp_t rsp = {};
          rsp.attr_value.handle = handle;
          rsp.attr_value.offset = offset;
          size_t len = desc->value.size();
          if (offset < len) {
            size_t sendLen = len - offset;
            if (sendLen > ESP_GATT_MAX_ATTR_LEN) {
              sendLen = ESP_GATT_MAX_ATTR_LEN;
            }
            rsp.attr_value.len = sendLen;
            memcpy(rsp.attr_value.value, desc->value.data() + offset, sendLen);
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
        Impl::PrepWrite pw;
        pw.handle = handle;
        pw.offset = param->write.offset;
        pw.data.assign(param->write.value, param->write.value + param->write.len);
        impl->prepWrites[connId].push_back(std::move(pw));
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
          BLESecurity sec = BLE.getSecurity();
          if (!BLESecurityBackend::notifyAuthorization(sec, connInfo, handle, false)) {
            if (needRsp) {
              if (esp_ble_gatts_send_response(gatts_if, connId, transId, ESP_GATT_INSUF_AUTHORIZATION, nullptr) != ESP_OK) {
                log_e("esp_ble_gatts_send_response failed");
              }
            }
            break;
          }
        }

        chr->value.assign(param->write.value, param->write.value + param->write.len);

        {
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
            auto chrHandle = BLECharacteristic(chr->shared_from_this());
            writeCb(chrHandle, connInfo);
          }
        }
        if (needRsp) {
          if (esp_ble_gatts_send_response(gatts_if, connId, transId, ESP_GATT_OK, nullptr) != ESP_OK) {
            log_e("esp_ble_gatts_send_response failed");
          }
        }
      } else if (desc) {
        desc->value.assign(param->write.value, param->write.value + param->write.len);

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
              auto chrHandle = BLECharacteristic(ownerChr->shared_from_this());
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
            auto descHandle = BLEDescriptor(desc->shared_from_this());
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

      auto it = impl->prepWrites.find(connId);
      if (execute && it != impl->prepWrites.end() && !it->second.empty()) {
        // Assemble all fragments per handle into a contiguous buffer.
        // Fragments arrive in offset order so we can simply resize and copy.
        auto &connPrepWrites = it->second;
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
          chr->value = std::move(assembled);
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
            auto chrHandle = BLECharacteristic(chr->shared_from_this());
            writeCb(chrHandle, connInfo);
          }
        }
      }
      impl->prepWrites.erase(connId);

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
        ble_server_dispatch::dispatchConnParamsUpdate(impl, connInfo);
      }
      break;
    }
    default: break;
  }
}

// --------------------------------------------------------------------------
// BLEClass::createServer() -- Bluedroid factory
// --------------------------------------------------------------------------

/**
 * @brief Returns a singleton @ref BLEServer, registering the GATTS app on first or stale use.
 * @return A valid @ref BLEServer, or an invalid one if BLE is not ready or registration fails.
 * @note GATTS registration via @c esp_ble_gatts_app_register is specific to this stack; NimBLE
 *       does not use the same GATTS app + interface bootstrap pattern.
 */
BLEServer BLEClass::createServer() {
  if (!isInitialized()) {
    return BLEServer();
  }

  static std::shared_ptr<BLEServer::Impl> server;
  bool needRegister = !server || server->gattsIf == ESP_GATT_IF_NONE;
  if (!server) {
    server = std::make_shared<BLEServer::Impl>();
    BLEServer::Impl::s_instance = server.get();

    esp_timer_create_args_t timerArgs = {};
    timerArgs.callback = [](void *) {
      BLE.startAdvertising();
    };
    timerArgs.name = "ble_adv_restart";
    esp_timer_create(&timerArgs, &server->advRestartTimer);
  }
  if (needRegister) {
    server->regSync.take();
    esp_err_t err = esp_ble_gatts_app_register(server->appId);
    if (err != ESP_OK) {
      log_e("esp_ble_gatts_app_register: %s", esp_err_to_name(err));
      BLEServer::Impl::s_instance = nullptr;
      server.reset();
      return BLEServer();
    }
    BTStatus st = server->regSync.wait(5000);
    if (st != BTStatus::OK) {
      log_e("GATTS app registration failed/timeout");
      BLEServer::Impl::s_instance = nullptr;
      server.reset();
      return BLEServer();
    }
  }
  return BLEServer(server);
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

/**
 * @brief Removes a GATT service from the stack and list, or only from the list if not started.
 * @param impl Server owning @c gattsIf and the service list.
 * @param svc Service to delete; must be a member of @a impl.
 * @return @c OK on success, or @c InvalidState or @c Fail on validation or @c delete_service errors.
 * @note Uses @c esp_ble_gatts_delete_service and clears local handles, instead of a NimBLE-wide
 *       @c ble_gatts_reset and full re-register of every service.
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
#include "impl/BLEServerBackend.h"
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

BLEServer BLEClass::createServer() {
  log_w("GATT server not supported");
  return BLEServer();
}

BTStatus bleServerRemoveService(BLEServer::Impl *, std::shared_ptr<BLEService::Impl>) {
  return BTStatus::NotSupported;
}

#endif /* BLE_GATT_SERVER_SUPPORTED */

#endif /* BLE_BLUEDROID */
