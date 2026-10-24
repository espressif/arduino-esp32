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
 * @file BLEServer.bluedroid.cpp
 * @brief BLE GATT server implementation for the Arduino BLE library using the
 *        ESP-IDF Bluedroid GATTS stack.
 */

#include "core/BLEGuards.h"
#if BLE_BLUEDROID

#if BLE_GATT_SERVER_SUPPORTED

#include "BLE.h"

#include "server/BLEServer.bluedroid.h"
#include "BLE.bluedroid.h"
#include "gatt/BLEGattAttributes.bluedroid.h"
#include "types/BLEUUID.bluedroid.h"
#include "core/BLESync.h"
#include "core/BLEImplHelpers.h"
#include "gatt/BLECharacteristicValidation.h"
#include "types/BLEConnInfoData.h"
#include "types/BLEConnInfo.bluedroid.h"
#include "core/BLEMutex.h"
#include "core/BLEBackend.h"
#include "esp32-hal-log.h"

#include <esp_gap_ble_api.h>
#include <esp_gatts_api.h>
#include <esp_bt_device.h>
#include <atomic>
#include <string.h>

#include "core/BLEValidationHooks.h"

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

void BLEServer::Impl::scheduleBusyRetry() {
  BLELockGuard lock(mtx);
  if (!active) {
    return;
  }
  if (!indicationRetryTimer) {
    esp_timer_create_args_t retryArgs = {};
    retryArgs.callback = [](void *arg) {
      auto *s = static_cast<BLEServer::Impl *>(arg);
      s->retryIndications();
    };
    retryArgs.arg = this;
    retryArgs.name = "ble_ind_retry";
    if (esp_timer_create(&retryArgs, &indicationRetryTimer) != ESP_OK) {
      log_e("Server: failed to create indication retry timer");
      return;
    }
  }
  esp_timer_stop(indicationRetryTimer);
  esp_timer_start_once(indicationRetryTimer, 20000);
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
  if (perm & BLEPermission::ReadOpen) {
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
  if (perm & BLEPermission::WriteOpen) {
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
  const bool anyReadPerm = (perms & BLEPermission::ReadOpen) || (perms & BLEPermission::ReadEncrypted) || (perms & BLEPermission::ReadAuthenticated)
                           || (perms & BLEPermission::ReadAuthorized);
  const bool anyWritePerm = (perms & BLEPermission::WriteOpen) || (perms & BLEPermission::WriteEncrypted) || (perms & BLEPermission::WriteAuthenticated)
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

void bluedroidCancelPendingReAdvertise() {
  auto *srv = BLEServer::Impl::s_instance;
  if (!srv) {
    return;
  }
  srv->reAdvEpoch.fetch_add(1, std::memory_order_acq_rel);
  if (srv->advRestartTimer) {
    esp_timer_stop(srv->advRestartTimer);
  }
}

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

// A few connection intervals: long enough for the indication holding the slot to be confirmed.
/**
 * @brief ATT_MTU of a connection, or the LE default when it is unknown.
 */
uint16_t connMtu(BLEServer::Impl *impl, uint16_t connId) {
  BLELockGuard lock(impl->mtx);
  const BLEConnInfo *info = impl->connFind(connId);
  return info ? info->getMTU() : 23;
}

/**
 * @brief Answers a read of an attribute value at @p offset.
 * @param impl      Server state owning the per-connection long-read snapshot.
 * @param mtx       The attribute's mtx, guarding @p value.
 * @param value     The attribute's current value.
 * @note A Read Blob past the end of the value gets Invalid Offset (Core Vol 3 Part F §3.4.4.5).
 */
void sendReadResponse(
  BLEServer::Impl *impl, esp_gatt_if_t gattsIf, uint16_t connId, uint32_t transId, uint16_t handle, uint16_t offset, uint16_t mtu, SemaphoreHandle_t mtx,
  const std::vector<uint8_t> &value
) {
  esp_gatt_rsp_t rsp = {};
  rsp.attr_value.handle = handle;
  rsp.attr_value.offset = offset;
  esp_gatt_status_t status = ESP_GATT_OK;
  // Single lock order: snapshot the value under the attribute mtx alone,
  // release it, then take the server mtx for the long-read snapshot table.
  std::vector<uint8_t> snapshot;
  {
    BLELockGuard lock(mtx);
    snapshot = value;
  }
  {
    BLELockGuard serverLock(impl->mtx);
    const std::vector<uint8_t> &src = impl->selectLongRead(connId, handle, offset, mtu, snapshot);
    if (offset > src.size()) {
      status = ESP_GATT_INVALID_OFFSET;
    } else {
      size_t sendLen = src.size() - offset;
      if (sendLen > ESP_GATT_MAX_ATTR_LEN) {
        sendLen = ESP_GATT_MAX_ATTR_LEN;
      }
      rsp.attr_value.len = sendLen;
      if (sendLen > 0) {
        memcpy(rsp.attr_value.value, src.data() + offset, sendLen);
      }
    }
    impl->finishLongRead(connId, handle, offset, mtu);
  }
  if (esp_ble_gatts_send_response(gattsIf, connId, transId, status, (status == ESP_GATT_OK) ? &rsp : nullptr) != ESP_OK) {
    log_e("esp_ble_gatts_send_response failed");
  }
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

BLEConnInfo getConnInfoCopy(BLEServer::Impl *impl, uint16_t connId) {
  BLEConnInfo connInfo;
  BLELockGuard lock(impl->mtx);
  if (BLEConnInfo *ci = impl->connFind(connId)) {
    connInfo = *ci;
  }
  return connInfo;
}

bool checkAuthorized(BLEServer::Impl *impl, uint16_t connId, uint16_t handle, BLEPermission perms, BLEPermission reqPerm, bool isRead) {
  if (!(perms & reqPerm)) {
    return true;
  }
  auto *sec = BLESecurity::Impl::instance();
  if (!sec) {
    log_w("BLESecurity: attribute 0x%04x requires authorization but no handler is installed — denying %s", handle, isRead ? "read" : "write");
    return false;
  }
  return sec->notifyAuthorization(getConnInfoCopy(impl, connId), handle, isRead);
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
    if (!impl.active || impl.gattsIf == ESP_GATT_IF_NONE) {
      return BTStatus::InvalidState;
    }
    if (impl.started) {
      bool hasNew = false;
      for (auto &s : impl.services) {
        if (!s->started) {
          hasNew = true;
          break;
        }
        // A characteristic (or descriptor) with handle 0 added to an
        // already-started service is new too; NimBLE rebuilds for this case,
        // so Bluedroid must not silently ignore it (handled below by
        // stop / add / start of that one service).
        for (auto &c : s->characteristics) {
          if (c->handle == 0) {
            hasNew = true;
            break;
          }
          BLELockGuard chrLock(c->mtx);
          for (auto &d : c->descriptors) {
            if (d->handle == 0) {
              hasNew = true;
              break;
            }
          }
          if (hasNew) {
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
      log_d("Server: registering new service(s)");
    } else {
      log_d("Server: starting with %u service(s)", (unsigned)impl.services.size());
    }
    services = impl.services;
  }

  // Registration iterates snapshots, never the live vectors: characteristics
  // are appended under the server mtx and descriptors under each chr->mtx,
  // while the native calls below block waiting for stack events.
  auto snapChrs = [&impl](BLEService::Impl &svc) {
    BLELockGuard lock(impl.mtx);
    return svc.characteristics;
  };
  auto snapDescs = [](BLECharacteristic::Impl &chr) {
    BLELockGuard lock(chr.mtx);
    return chr.descriptors;
  };
  auto snapIncludes = [&impl](BLEService::Impl &svc) {
    BLELockGuard lock(impl.mtx);
    return svc.includedServices;
  };
  auto svcHasUnregistered = [&snapChrs, &snapDescs](BLEService::Impl &svc) {
    for (auto &chr : snapChrs(svc)) {
      if (chr->handle == 0) {
        return true;
      }
      for (auto &desc : snapDescs(*chr)) {
        if (desc->handle == 0) {
          return true;
        }
      }
    }
    return false;
  };

  // First pass: validate all unstarted services (and unregistered
  // characteristics on started ones), then create the unstarted services so
  // every service obtains a GATT handle before Included Service declarations
  // are added.
  for (auto &svc : services) {
    bool svcStarted;
    {
      BLELockGuard lock(impl.mtx);
      svcStarted = svc->started;
    }
    if (svcStarted && !svcHasUnregistered(*svc)) {
      continue;
    }
    if (!svcStarted && svc->handle != 0) {
      continue;
    }

    // Auto-create CCCD descriptors before validation and handle counting.
    for (auto &chr : snapChrs(*svc)) {
      if (svcStarted && chr->handle != 0) {
        continue;  // Already registered; its CCCD already exists.
      }
      if ((chr->properties & BLEProperty::Notify) || (chr->properties & BLEProperty::Indicate)) {
        bool hasCCCD = false;
        for (auto &d : snapDescs(*chr)) {
          if (d->uuid == BLEUUID(CCCD_UUID16)) {
            hasCCCD = true;
            break;
          }
        }
        if (!hasCCCD) {
          auto cccd = std::make_shared<BLEDescriptor::Impl>();
          cccd->uuid = BLEUUID(CCCD_UUID16);
          cccd->chr = chr.get();
      cccd->chrOwner = chr;
          cccd->permissions = BLEPermission::ReadWriteOpen;
          cccd->value.resize(2, 0);
          BLELockGuard lock(chr->mtx);
          chr->descriptors.push_back(cccd);
        }
      }
      if (!bleValidateCharForRegistration(*chr, /*stackIsNimble=*/false)) {
        log_e("Server: characteristic %s rejected by validation", chr->uuid.toString().c_str());
        return BTStatus::InvalidState;
      }
    }
  }

  for (auto &svc : services) {
    if (svc->started || svc->handle != 0) {
      continue;
    }

    // 1 (service) + 1 per include + 2 per char + 1 per descriptor.
    uint32_t needed = 1 + static_cast<uint32_t>(snapIncludes(*svc).size());
    for (const auto &chr : snapChrs(*svc)) {
      needed += 2 + snapDescs(*chr).size();
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
    bool wasStarted = false;
    {
      BLELockGuard lock(impl.mtx);
      wasStarted = svc->started;
    }
    if (wasStarted && !svcHasUnregistered(*svc)) {
      continue;
    }
    if (wasStarted) {
      // Bluedroid cannot add a characteristic to a started service. Stop the
      // service, add only the unregistered characteristics/descriptors
      // through the same incremental flow used at first registration, then
      // start it again. (If the service was created without spare attribute
      // handles the stack rejects the add and the createSync wait below
      // surfaces that as Fail instead of a silent no-op.)
      log_i("Server: service %s re-registering added characteristic(s) (stop/add/start)", svc->uuid.toString().c_str());
      impl.createSync.take();
      esp_err_t stopErr = esp_ble_gatts_stop_service(svc->handle);
      if (stopErr != ESP_OK) {
        log_e("esp_ble_gatts_stop_service: %s", esp_err_to_name(stopErr));
        return BTStatus::Fail;
      }
      BTStatus stopSt = impl.createSync.wait(5000);
      if (stopSt != BTStatus::OK) {
        log_e("Stop service for re-registration failed/timeout");
        return stopSt;
      }
      BLELockGuard lock(impl.mtx);
      svc->started = false;
    }

    // 1. Add Included Service declarations (GATT §3.2). Already declared on a
    // service being re-registered after a stop; do not declare them twice.
    std::vector<std::shared_ptr<BLEService::Impl>> includes = wasStarted ? std::vector<std::shared_ptr<BLEService::Impl>>{} : snapIncludes(*svc);
    for (auto &inc : includes) {
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

    // 2. Add characteristics (only unregistered ones; a handle != 0 means the
    // stack already has it, e.g. on a service re-registered after a stop).
    bool reregFailed = false;
    BTStatus reregSt = BTStatus::Fail;
    for (auto &chr : snapChrs(*svc)) {
      esp_err_t err = ESP_OK;
      BTStatus st = BTStatus::OK;
      if (chr->handle == 0) {
        esp_bt_uuid_t char_uuid;
        bluedroidUuidFromPublic(chr->uuid, char_uuid);
        esp_gatt_perm_t perm = toEspGattPermissions(chr->permissions);
        esp_gatt_char_prop_t prop = toEspGattProperties(chr->properties, chr->permissions);

        impl.pendingHandle = &chr->handle;
        impl.createSync.take();
        err = esp_ble_gatts_add_char(svc->handle, &char_uuid, perm, prop, nullptr, nullptr);
        if (err != ESP_OK) {
          log_e("esp_ble_gatts_add_char: %s", esp_err_to_name(err));
          impl.pendingHandle = nullptr;
          reregFailed = true;
          reregSt = BTStatus::Fail;
          break;
        }
        st = impl.createSync.wait(5000);
        impl.pendingHandle = nullptr;
        if (st != BTStatus::OK) {
          log_e("Add char failed/timeout");
          reregFailed = true;
          reregSt = st;
          break;
        }
      }

      // 3. Add descriptors for this characteristic (unregistered ones only).
      for (auto &desc : snapDescs(*chr)) {
        if (desc->handle != 0) {
          continue;
        }
        esp_bt_uuid_t desc_uuid;
        bluedroidUuidFromPublic(desc->uuid, desc_uuid);
        esp_gatt_perm_t descPerm = toEspGattPermissions(desc->permissions);

        impl.pendingHandle = &desc->handle;
        impl.createSync.take();
        err = esp_ble_gatts_add_char_descr(svc->handle, &desc_uuid, descPerm, nullptr, nullptr);
        if (err != ESP_OK) {
          log_e("esp_ble_gatts_add_char_descr: %s", esp_err_to_name(err));
          impl.pendingHandle = nullptr;
          reregFailed = true;
          reregSt = BTStatus::Fail;
          break;
        }
        st = impl.createSync.wait(5000);
        impl.pendingHandle = nullptr;
        if (st != BTStatus::OK) {
          log_e("Add descr failed/timeout");
          reregFailed = true;
          reregSt = st;
          break;
        }
      }
      if (reregFailed) {
        break;
      }
    }

    if (reregFailed) {
      if (wasStarted) {
        impl.createSync.take();
        if (esp_ble_gatts_start_service(svc->handle) == ESP_OK && impl.createSync.wait(5000) == BTStatus::OK) {
          BLELockGuard lock(impl.mtx);
          svc->started = true;
        }
      }
      return reregSt;
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
    {
      BLELockGuard lock(impl.mtx);
      svc->started = true;
    }
  }

  log_i("Server: started, %u service(s) registered", (unsigned)services.size());
  {
    BLELockGuard lock(impl.mtx);
    impl.started = true;
  }
  return BTStatus::OK;
}

BTStatus BLEServer::disconnect(uint16_t connHandle, uint8_t /*reason*/) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  {
    BLELockGuard lock(impl.mtx);
    if (!impl.connFind(connHandle)) {
      log_w("Server: disconnect handle=%u not connected", connHandle);
      return BTStatus::NotConnected;
    }
  }
  const char *taskName = pcTaskGetName(nullptr);
  bool wait = !(taskName && strcmp(taskName, "BTC_TASK") == 0);
  if (wait) {
    // Single wait slot (see the NimBLE disconnect): claim it atomically so
    // overlapping disconnect() waits cannot corrupt each other.
    uint16_t expected = 0xFFFF;
    if (!impl.pendingDisconnectHandle.compare_exchange_strong(expected, connHandle)) {
      log_w("Server: disconnect handle=%u - another disconnect wait is in progress", connHandle);
      return BTStatus::Busy;
    }
    impl.disconnectSync.take();
  }
  esp_err_t err = esp_ble_gatts_close(impl.gattsIf, connHandle);
  if (err != ESP_OK) {
    if (wait) {
      impl.pendingDisconnectHandle.store(0xFFFF);
      impl.disconnectSync.give(BTStatus::Fail);
    }
    log_e("Server: esp_ble_gatts_close handle=%u: %s", connHandle, esp_err_to_name(err));
    return BTStatus::Fail;
  }
  if (wait) {
    BTStatus waited = impl.disconnectSync.wait(5000);
    impl.pendingDisconnectHandle.store(0xFFFF);
    return waited;
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

BLEIndicationSend bleServerSendIndication(
  BLEServer::Impl *impl, uint16_t connHandle, uint16_t attrHandle, const uint8_t *data, size_t length, BLECharacteristic::NotifyStatus &status, uint32_t &code
) {
  esp_gatt_if_t gattsIf = ESP_GATT_IF_NONE;
  {
    BLELockGuard lock(impl->mtx);
    if (!impl->active || !impl->started || impl->gattsIf == ESP_GATT_IF_NONE || !impl->connFind(connHandle)) {
      status = BLECharacteristic::ERROR_NO_CLIENT;
      code = ESP_ERR_INVALID_STATE;
      return BLEIndicationSend::Failed;
    }
    gattsIf = impl->gattsIf;
  }
  // Bluedroid copies the payload; the outcome arrives as ESP_GATTS_CONF_EVT.
  esp_err_t err = esp_ble_gatts_send_indicate(gattsIf, connHandle, attrHandle, length, const_cast<uint8_t *>(data), true);
  if (err == ESP_OK) {
    return BLEIndicationSend::Sent;
  }
  if (err == ESP_FAIL) {
    // The public API returns ESP_FAIL when ATT's L2CAP channel is congested
    // (and also for a transient BTC queue allocation failure). Retry both
    // cases within the bounded queue deadline.
    status = BLECharacteristic::ERROR_INDICATE_FAILURE;
    code = static_cast<uint32_t>(err);
    impl->scheduleBusyRetry();
    return BLEIndicationSend::Busy;
  }
  log_e("Server: esp_ble_gatts_send_indicate (indicate) conn=%u: %s", connHandle, esp_err_to_name(err));
  status = (err == ESP_ERR_INVALID_STATE) ? BLECharacteristic::ERROR_NO_CLIENT : BLECharacteristic::ERROR_INDICATE_FAILURE;
  code = static_cast<uint32_t>(err);
  return BLEIndicationSend::Failed;
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
  esp_err_t err = esp_ble_gap_set_preferred_phy(bda, 0, blePhyToPrefMask(txPhy), blePhyToPrefMask(rxPhy), ESP_BLE_GAP_PHY_OPTIONS_NO_PREF);
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
#if !BLE5_SUPPORTED && BLE_ADVERTISING_SUPPORTED
      if (param->connect.link_role != 0 && BLEAdvertising::Impl::s_instance) {
        BLEAdvertising::Impl::s_instance->isAdvertising = false;
      }
#endif
      BLEConnInfo connInfo = BLEConnInfoImpl::make(connId, param->connect.remote_bda, 23, false, static_cast<BTAddress::Type>(param->connect.ble_addr_type));
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
      std::vector<std::pair<std::shared_ptr<BLECharacteristic::Impl>, uint16_t>> droppedNotifications;
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
        for (const auto &pending : impl->pendingNotifies) {
          if (pending.connHandle != connId || pending.total() == 0) {
            continue;
          }
          if (BLECharacteristic::Impl *found = findCharByHandle(impl, pending.attrHandle)) {
            droppedNotifications.emplace_back(found->shared_from_this(), pending.total());
          }
        }
        impl->erasePendingNotifies(connId);
        impl->connErase(connId);
        shouldAdvertise = impl->advertiseOnDisconnect;
      }
      // Prepared writes belong to this ATT bearer and must not survive a
      // disconnect; Bluedroid may reuse conn_id for a later peer.
      impl->erasePrepWrites(connId);
      // Bluedroid drops the link when an indication waits 30 s for its
      // confirmation, without a CONF event; dropIndications() reports that one.
      impl->dropIndications(connId);
      // Every accepted notification gets exactly one terminal status. Report
      // link-loss before onDisconnect; any late CONF is ignored below because
      // its pending entry has already been consumed here.
      for (auto &[chr, count] : droppedNotifications) {
        for (uint16_t i = 0; i < count; i++) {
          chr->dispatchStatus(BLECharacteristic::ERROR_NO_CLIENT, 0);
        }
      }
      impl->dispatchDisconnect(connInfo, reason);
      if (impl->pendingDisconnectHandle.load() == connId) {
        impl->disconnectSync.give(BTStatus::OK);
      }
      if (shouldAdvertise && impl->advRestartTimer) {
        // Defer to esp_timer task — calling BLE.startAdvertising() directly
        // from the BTC callback would deadlock (BLESync blocks for GAP
        // events that are also delivered on the BTC task).
        impl->reAdvArmedEpoch.store(impl->reAdvEpoch.load(std::memory_order_relaxed), std::memory_order_release);
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

    case ESP_GATTS_STOP_EVT:
    {
      impl->createSync.give(param->stop.status == ESP_GATT_OK ? BTStatus::OK : BTStatus::Fail);
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
        if (!checkAuthorized(impl, connId, handle, chr->permissions, BLEPermission::ReadAuthorized, true)) {
          if (param->read.need_rsp && esp_ble_gatts_send_response(gatts_if, connId, transId, ESP_GATT_INSUF_AUTHORIZATION, nullptr) != ESP_OK) {
            log_e("esp_ble_gatts_send_response failed");
          }
          break;
        }

        if (offset == 0) {
          BLECharacteristic::ReadHandler readCb;
          {
            BLELockGuard lock(chr->mtx);
            readCb = chr->onReadCb;
          }
          if (readCb) {
            auto chrHandle = BLECharacteristicImplCommon::makeHandle(chr->shared_from_this());
            readCb(chrHandle, getConnInfoCopy(impl, connId));
          }
        }

        if (param->read.need_rsp) {
          // value is guarded by the per-characteristic mtx (same lock as the
          // public getValue/setValue), not the server mtx.
          sendReadResponse(impl, gatts_if, connId, transId, handle, offset, connMtu(impl, connId), chr->mtx, chr->value);
        }
      } else if (desc) {
        if (!checkAuthorized(impl, connId, handle, desc->permissions, BLEPermission::ReadAuthorized, true)) {
          if (param->read.need_rsp && esp_ble_gatts_send_response(gatts_if, connId, transId, ESP_GATT_INSUF_AUTHORIZATION, nullptr) != ESP_OK) {
            log_e("esp_ble_gatts_send_response failed");
          }
          break;
        }
        bool isCccd = desc->uuid == BLEUUID(CCCD_UUID16);
        if (offset == 0 && !isCccd) {
          BLEDescriptor::ReadHandler readCb;
          {
            BLELockGuard lock(desc->mtx);
            readCb = desc->onReadCb;
          }
          if (readCb) {
            auto descHandle = BLEDescriptorImplCommon::makeHandle(desc->shared_from_this());
            readCb(descHandle, getConnInfoCopy(impl, connId));
          }
        }

        if (param->read.need_rsp) {
          if (isCccd) {
            uint16_t subVal = 0;
            BLECharacteristic::Impl *ownerChr = findCharForDesc(impl, handle);
            if (ownerChr) {
              BLELockGuard lock(impl->mtx);
              for (const auto &sub : ownerChr->subscribers) {
                if (sub.first == connId) {
                  subVal = sub.second;
                  break;
                }
              }
            }
            uint8_t cccdBytes[2] = {static_cast<uint8_t>(subVal), static_cast<uint8_t>(subVal >> 8)};
            esp_gatt_rsp_t rsp = {};
            rsp.attr_value.handle = handle;
            rsp.attr_value.offset = offset;
            esp_gatt_status_t status = (offset > sizeof(cccdBytes)) ? ESP_GATT_INVALID_OFFSET : ESP_GATT_OK;
            if (status == ESP_GATT_OK) {
              rsp.attr_value.len = sizeof(cccdBytes) - offset;
              if (rsp.attr_value.len > 0) {
                memcpy(rsp.attr_value.value, cccdBytes + offset, rsp.attr_value.len);
              }
            }
            if (esp_ble_gatts_send_response(gatts_if, connId, transId, status, (status == ESP_GATT_OK) ? &rsp : nullptr) != ESP_OK) {
              log_e("esp_ble_gatts_send_response failed");
            }
          } else {
            sendReadResponse(impl, gatts_if, connId, transId, handle, offset, connMtu(impl, connId), desc->mtx, desc->value);
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

      BLECharacteristic::Impl *chr = findCharByHandle(impl, handle);
      BLEDescriptor::Impl *desc = chr ? nullptr : findDescByHandle(impl, handle);

      if (isPrep) {
        esp_gatt_status_t prepStatus = ESP_GATT_OK;
        if (!chr && !desc) {
          prepStatus = ESP_GATT_NOT_FOUND;
        } else if (!checkAuthorized(impl, connId, handle, chr ? chr->permissions : desc->permissions, BLEPermission::WriteAuthorized, false)) {
          // CCCD prepare writes are queued like any other descriptor write
          // (parity with NimBLE, which has no prepare/execute split); the
          // subscription is validated and applied at Execute Write below.
          prepStatus = ESP_GATT_INSUF_AUTHORIZATION;
        }

        if (prepStatus == ESP_GATT_OK) {
          auto &pending = impl->prepWritesFor(connId);
          size_t queuedBytes = 0;
          for (const auto &queued : pending) {
            queuedBytes += queued.data.size();
          }
          // Core Spec Vol 3, Part F, §3.4.6.1: only queue capacity is checked on
          // Prepare Write Request; offset and attribute length are validated on
          // Execute Write Request (§3.4.6.2).
          if (pending.size() >= 32 || queuedBytes + param->write.len > 4096) {
            prepStatus = ESP_GATT_PREPARE_Q_FULL;
          } else {
            PrepWrite pw;
            pw.handle = handle;
            pw.offset = param->write.offset;
            pw.data.assign(param->write.value, param->write.value + param->write.len);
            pending.push_back(std::move(pw));
          }
        }
        if (prepStatus != ESP_GATT_OK) {
          if (needRsp && esp_ble_gatts_send_response(gatts_if, connId, transId, prepStatus, nullptr) != ESP_OK) {
            log_e("esp_ble_gatts_send_response failed");
          }
          break;
        }
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

      if (chr) {
        // Software-level authorization gate for WriteAuthorized
        if (!checkAuthorized(impl, connId, handle, chr->permissions, BLEPermission::WriteAuthorized, false)) {
          if (needRsp && esp_ble_gatts_send_response(gatts_if, connId, transId, ESP_GATT_INSUF_AUTHORIZATION, nullptr) != ESP_OK) {
            log_e("esp_ble_gatts_send_response failed");
          }
          break;
        }

        {
          BLECharacteristic::WriteHandler writeCb;
          {
            // value is guarded by the per-characteristic mtx (same lock as the
            // public getValue/setValue and the read/notify paths).
            BLELockGuard lock(chr->mtx);
            chr->value.assign(param->write.value, param->write.value + param->write.len);
            writeCb = chr->onWriteCb;
          }
          if (writeCb) {
            auto chrHandle = BLECharacteristicImplCommon::makeHandle(chr->shared_from_this());
            writeCb(chrHandle, getConnInfoCopy(impl, connId));
          }
        }
        if (needRsp) {
          if (esp_ble_gatts_send_response(gatts_if, connId, transId, ESP_GATT_OK, nullptr) != ESP_OK) {
            log_e("esp_ble_gatts_send_response failed");
          }
        }
      } else if (desc) {
        esp_gatt_status_t writeStatus = ESP_GATT_OK;
        bool isCccd = desc->uuid == BLEUUID(CCCD_UUID16);
        BLECharacteristic::Impl *ownerChr = isCccd ? findCharForDesc(impl, handle) : nullptr;
        uint16_t subVal = 0;
        if (!checkAuthorized(impl, connId, handle, desc->permissions, BLEPermission::WriteAuthorized, false)) {
          writeStatus = ESP_GATT_INSUF_AUTHORIZATION;
        }
        if (writeStatus == ESP_GATT_OK && isCccd) {
          if (param->write.offset != 0) {
            writeStatus = ESP_GATT_INVALID_OFFSET;
          } else if (param->write.len != 2) {
            writeStatus = ESP_GATT_INVALID_ATTR_LEN;
          } else if (!ownerChr) {
            writeStatus = ESP_GATT_NOT_FOUND;
          } else {
            subVal = param->write.value[0] | (param->write.value[1] << 8);
            uint16_t allowed = 0;
            if (ownerChr->properties & BLEProperty::Notify) {
              allowed |= BLE_CCCD_NOTIFY;
            }
            if (ownerChr->properties & BLEProperty::Indicate) {
              allowed |= BLE_CCCD_INDICATE;
            }
            if (subVal & ~allowed) {
              writeStatus = ESP_GATT_CCC_CFG_ERR;
            }
          }
        }

        BLECharacteristic::SubscribeHandler subCb;
        BLEConnInfo subConnInfo;
        bool subChanged = false;
        if (writeStatus == ESP_GATT_OK && isCccd) {
          if (ownerChr) {
            {
              BLELockGuard lock(impl->mtx);
              uint16_t prevVal = 0;
              if (subVal > 0) {
                bool found = false;
                for (auto &kv : ownerChr->subscribers) {
                  if (kv.first == connId) {
                    prevVal = kv.second;
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
                    prevVal = it->second;
                    ownerChr->subscribers.erase(it);
                    break;
                  }
                }
              }
              subChanged = (prevVal != subVal);
              if (subChanged) {
                if (BLEConnInfo *ci = impl->connFind(connId)) {
                  subConnInfo = *ci;
                }
              }
            }
            if (subChanged) {
              BLELockGuard lock(ownerChr->mtx);
              subCb = ownerChr->onSubscribeCb;
            }
          }
        } else if (writeStatus == ESP_GATT_OK && param->write.len > desc->maxLen) {
          writeStatus = ESP_GATT_INVALID_ATTR_LEN;
        } else if (writeStatus == ESP_GATT_OK) {
          BLEDescriptor::WriteHandler writeCb;
          {
            BLELockGuard lock(desc->mtx);
            desc->value.assign(param->write.value, param->write.value + param->write.len);
            writeCb = desc->onWriteCb;
          }
          if (writeCb) {
            auto descHandle = BLEDescriptorImplCommon::makeHandle(desc->shared_from_this());
            writeCb(descHandle, getConnInfoCopy(impl, connId));
          }
        }
        if (needRsp) {
          if (esp_ble_gatts_send_response(gatts_if, connId, transId, writeStatus, nullptr) != ESP_OK) {
            log_e("esp_ble_gatts_send_response failed");
          }
        }
        if (subChanged && subCb && ownerChr) {
          auto chrHandle = BLECharacteristicImplCommon::makeHandle(ownerChr->shared_from_this());
          subCb(chrHandle, subConnInfo, subVal);
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
      esp_gatt_status_t execStatus = ESP_GATT_OK;
      // CCCD subscriptions applied at Execute are reported after the Execute
      // response is sent, mirroring the immediate CCCD write path.
      struct DeferredSubscribe {
        BLECharacteristic::Impl *chr = nullptr;
        BLECharacteristic::SubscribeHandler cb;
        BLEConnInfo connInfo;
        uint16_t subVal = 0;
      };
      std::vector<DeferredSubscribe> deferredSubs;
      if (execute && pending != nullptr && !pending->empty()) {
        struct PreparedTrack {
          uint16_t handle = 0;
          BLECharacteristic::Impl *chr = nullptr;
          BLEDescriptor::Impl *desc = nullptr;
          size_t curLen = 0;
          size_t maxLen = 0;
          bool clearFirst = false;
          bool isCccd = false;
          BLECharacteristic::Impl *cccdChr = nullptr;
          uint16_t cccdValue = 0;
        };
        PreparedTrack tracks[32];
        size_t trackCount = 0;

        // First pass: validate all fragments across all queued attributes
        // (Core Spec Vol 3, Part F, §3.4.6.2) before committing any state.
        for (const auto &pw : *pending) {
          PreparedTrack *track = nullptr;
          for (size_t i = 0; i < trackCount; ++i) {
            if (tracks[i].handle == pw.handle) {
              track = &tracks[i];
              break;
            }
          }
          if (!track) {
            BLECharacteristic::Impl *foundChr = findCharByHandle(impl, pw.handle);
            BLEDescriptor::Impl *foundDesc = foundChr ? nullptr : findDescByHandle(impl, pw.handle);
            if (!foundChr && !foundDesc) {
              execStatus = ESP_GATT_NOT_FOUND;
              break;
            }
            track = &tracks[trackCount++];
            track->handle = pw.handle;
            track->chr = foundChr;
            track->desc = foundDesc;
            track->clearFirst = (pw.offset == 0);
            track->isCccd = foundDesc && foundDesc->uuid == BLEUUID(CCCD_UUID16);
            if (track->isCccd) {
              track->cccdChr = findCharForDesc(impl, pw.handle);
              if (!track->cccdChr) {
                execStatus = ESP_GATT_NOT_FOUND;
                break;
              }
            }
            track->maxLen = track->isCccd ? 2 : (foundDesc ? std::min<size_t>(foundDesc->maxLen, ESP_GATT_MAX_ATTR_LEN) : ESP_GATT_MAX_ATTR_LEN);
            if (!track->clearFirst) {
              if (foundChr) {
                BLELockGuard lock(foundChr->mtx);
                track->curLen = foundChr->value.size();
              } else {
                BLELockGuard lock(foundDesc->mtx);
                track->curLen = foundDesc->value.size();
              }
            }
          }

          size_t end = static_cast<size_t>(pw.offset) + pw.data.size();
          if (pw.offset > track->curLen) {
            execStatus = ESP_GATT_INVALID_OFFSET;
            break;
          }
          if (end > track->maxLen) {
            execStatus = ESP_GATT_INVALID_ATTR_LEN;
            break;
          }
          if (end > track->curLen) {
            track->curLen = end;
          }
        }

        // CCCD tracks: assemble and validate the subscription value before
        // committing anything, exactly as an immediate CCCD write is checked.
        if (execStatus == ESP_GATT_OK) {
          for (size_t i = 0; i < trackCount; ++i) {
            PreparedTrack &track = tracks[i];
            if (!track.isCccd) {
              continue;
            }
            if (track.curLen != 2) {
              execStatus = ESP_GATT_INVALID_ATTR_LEN;
              break;
            }
            uint8_t bytes[2] = {0, 0};
            if (!track.clearFirst) {
              BLELockGuard lock(impl->mtx);
              for (const auto &sub : track.cccdChr->subscribers) {
                if (sub.first == connId) {
                  bytes[0] = static_cast<uint8_t>(sub.second);
                  bytes[1] = static_cast<uint8_t>(sub.second >> 8);
                  break;
                }
              }
            }
            for (const auto &pw : *pending) {
              if (pw.handle == track.handle && !pw.data.empty()) {
                memcpy(bytes + pw.offset, pw.data.data(), pw.data.size());
              }
            }
            track.cccdValue = static_cast<uint16_t>(bytes[0] | (bytes[1] << 8));
            uint16_t allowed = 0;
            if (track.cccdChr->properties & BLEProperty::Notify) {
              allowed |= BLE_CCCD_NOTIFY;
            }
            if (track.cccdChr->properties & BLEProperty::Indicate) {
              allowed |= BLE_CCCD_INDICATE;
            }
            if (track.cccdValue & ~allowed) {
              execStatus = ESP_GATT_CCC_CFG_ERR;
              break;
            }
          }
        }

        if (execStatus == ESP_GATT_OK) {
          BLEConnInfo connInfo = getConnInfoCopy(impl, connId);
          for (size_t i = 0; i < trackCount; ++i) {
            PreparedTrack &track = tracks[i];
            if (track.isCccd) {
              // Apply the subscription (not the descriptor's stored value)
              // and defer onSubscribe until after the Execute response.
              BLECharacteristic::SubscribeHandler subCb;
              bool subChanged = false;
              {
                BLELockGuard lock(impl->mtx);
                uint16_t prevVal = 0;
                if (track.cccdValue > 0) {
                  bool found = false;
                  for (auto &kv : track.cccdChr->subscribers) {
                    if (kv.first == connId) {
                      prevVal = kv.second;
                      kv.second = track.cccdValue;
                      found = true;
                      break;
                    }
                  }
                  if (!found) {
                    track.cccdChr->subscribers.emplace_back(connId, track.cccdValue);
                  }
                } else {
                  for (auto it = track.cccdChr->subscribers.begin(); it != track.cccdChr->subscribers.end(); ++it) {
                    if (it->first == connId) {
                      prevVal = it->second;
                      track.cccdChr->subscribers.erase(it);
                      break;
                    }
                  }
                }
                subChanged = (prevVal != track.cccdValue);
              }
              if (subChanged) {
                BLELockGuard lock(track.cccdChr->mtx);
                subCb = track.cccdChr->onSubscribeCb;
              }
              if (subChanged && subCb) {
                deferredSubs.push_back({track.cccdChr, subCb, connInfo, track.cccdValue});
              }
              continue;
            }
            SemaphoreHandle_t attrMtx = track.chr ? track.chr->mtx : track.desc->mtx;
            std::vector<uint8_t> &attrVal = track.chr ? track.chr->value : track.desc->value;
            {
              BLELockGuard lock(attrMtx);
              if (track.clearFirst) {
                attrVal.clear();
              }
              attrVal.resize(track.curLen);
              for (const auto &pw : *pending) {
                if (pw.handle == track.handle && !pw.data.empty()) {
                  memcpy(attrVal.data() + pw.offset, pw.data.data(), pw.data.size());
                }
              }
            }
            if (track.chr) {
              BLECharacteristic::WriteHandler callback;
              {
                BLELockGuard lock(track.chr->mtx);
                callback = track.chr->onWriteCb;
              }
              if (callback) {
                auto chrHandle = BLECharacteristicImplCommon::makeHandle(track.chr->shared_from_this());
                callback(chrHandle, connInfo);
              }
            } else if (track.desc) {
              BLEDescriptor::WriteHandler callback;
              {
                BLELockGuard lock(track.desc->mtx);
                callback = track.desc->onWriteCb;
              }
              if (callback) {
                auto descHandle = BLEDescriptorImplCommon::makeHandle(track.desc->shared_from_this());
                callback(descHandle, connInfo);
              }
            }
          }
        }
      }
      impl->erasePrepWrites(connId);

      if (esp_ble_gatts_send_response(gatts_if, connId, param->exec_write.trans_id, execStatus, nullptr) != ESP_OK) {
        log_e("esp_ble_gatts_send_response failed");
      }
      for (auto &deferred : deferredSubs) {
        auto chrHandle = BLECharacteristicImplCommon::makeHandle(deferred.chr->shared_from_this());
        deferred.cb(chrHandle, deferred.connInfo, deferred.subVal);
      }
      break;
    }

    case ESP_GATTS_CONGEST_EVT:
      if (!param->congest.congested) {
        impl->retryIndications();
      }
      break;

    case ESP_GATTS_CONF_EVT:
    {
      uint16_t connId = param->conf.conn_id;
      uint16_t attrHandle = param->conf.handle;
      esp_gatt_status_t st = param->conf.status;
      std::shared_ptr<BLECharacteristic::Impl> chr;
      bool isIndication = false;
      bool wasPendingNotification = false;
      bool heldPendingNotification = false;
      {
        BLELockGuard lock(impl->mtx);
        if (BLECharacteristic::Impl *found = findCharByHandle(impl, attrHandle)) {
          chr = found->shared_from_this();
        }
        // Consume a submitted notification first, regardless of status: an
        // ESP_GATT_BUSY completion for a notification we sent must not be
        // routed into indicationBusy() just because an indication happens
        // to be in flight on the same handle. A reservation that has not
        // been handed to the stack must not steal an indication confirmation.
#if BLE_VALIDATION_HOOKS
        if (BLEHooks.gatt.holdNotifyCompletions.load(std::memory_order_relaxed)) {
          for (const auto &pending : impl->pendingNotifies) {
            if (pending.connHandle == connId && pending.attrHandle == attrHandle && pending.submitted > 0) {
              heldPendingNotification = true;
              break;
            }
          }
        }
#endif
        if (heldPendingNotification) {
          // Deterministic validation barrier: leave the accepted send pending
          // so the real disconnect path must terminate it.
        } else if (impl->consumePendingNotify(connId, attrHandle)) {
          wasPendingNotification = true;
          isIndication = false;
        } else if (auto *link = impl->indicationLink(connId); link && link->inFlight && link->inFlight->handle == attrHandle) {
          isIndication = true;
        }
      }

      if (heldPendingNotification) {
        break;
      }

      // ESP_GATT_CONGESTED: the PDU was queued anyway. For an indication the
      // confirmation still follows as a second CONF event.
      bool sent = st == ESP_GATT_OK || st == ESP_GATT_CONGESTED;
      if (!isIndication) {
        if (wasPendingNotification && chr) {
          if (!sent) {
            log_w("Server: notification on handle %u connId=%u failed, status=%d", attrHandle, connId, st);
          }
          chr->dispatchStatus(sent ? BLECharacteristic::SUCCESS_NOTIFY : BLECharacteristic::ERROR_GATT, sent ? 0 : st);
        }
      } else if (st == ESP_GATT_CONGESTED) {
        break;
      } else if (st == ESP_GATT_BUSY) {
        // The slot is held by an indication the queue did not send (e.g. the
        // stack's own Service Changed), whose confirmation raises no event here.
        if (impl->indicationBusy(connId, attrHandle, st)) {
          impl->scheduleBusyRetry();
        }
      } else {
        if (st != ESP_GATT_OK) {
          log_w("Server: indication on handle %u connId=%u failed, status=%d", attrHandle, connId, st);
        }
        impl->indicationDone(
          connId, attrHandle, (st == ESP_GATT_OK) ? BLECharacteristic::SUCCESS_INDICATE : BLECharacteristic::ERROR_INDICATE_FAILURE, st
        );
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
          if (entry.second.getAddress().equalsEspBdAddr(param->update_conn_params.bda)) {
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
          if (entry.second.getAddress().equalsEspBdAddr(param->read_phy.bda)) {
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
          if (entry.second.getAddress().equalsEspBdAddr(param->phy_update.bda)) {
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
      impl->dataLenSync.give(param->pkt_data_length_cmpl.status == ESP_BT_STATUS_SUCCESS ? BTStatus::OK : BTStatus::Fail);
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
          if (entry.second.getAddress().equalsEspBdAddr(param->ble_security.auth_cmpl.bd_addr)) {
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

// Uses esp_ble_gatts_delete_service and clears local handles, instead of a NimBLE-wide
// ble_gatts_reset and full re-register of every service.
BTStatus bleServerRemoveService(BLEServer::Impl *impl, std::shared_ptr<BLEService::Impl> svc) {
  if (!impl || !svc) {
    return BTStatus::InvalidState;
  }

  bool inList = false;
  {
    BLELockGuard lock(impl->mtx);
    if (impl->hasPendingNotifications(svc.get())) {
      log_w("Server: cannot remove a service with pending notifications");
      return BTStatus::Busy;
    }
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

  // Failed registration can leave a native service handle even though
  // started was never set. Delete every native service; only handle-less
  // staged services can be detached locally.
  if (svc->handle == 0 || impl->gattsIf == ESP_GATT_IF_NONE) {
    {
      BLELockGuard lock(impl->mtx);
      for (auto it = impl->services.begin(); it != impl->services.end(); ++it) {
        if (it->get() == svc.get()) {
          impl->services.erase(it);
          break;
        }
      }
    }
    bleDetachLocalService(svc);
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

  {
    BLELockGuard lock(impl->mtx);
    for (auto it = impl->services.begin(); it != impl->services.end(); ++it) {
      if (it->get() == svc.get()) {
        impl->services.erase(it);
        break;
      }
    }
  }
  bleDetachLocalService(svc);
  return BTStatus::OK;
}

#else /* !BLE_GATT_SERVER_SUPPORTED -- stubs */

#include "BLE.h"
#include "core/BLEBackend.h"
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
