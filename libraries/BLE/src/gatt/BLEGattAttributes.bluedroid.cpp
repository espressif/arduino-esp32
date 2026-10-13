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
 * @file BLEGattAttributes.bluedroid.cpp
 * @brief Bluedroid server-side GATT attributes: characteristic notify and descriptor permissions (BLE_GATT_SERVER_SUPPORTED).
 */

#include "core/BLEGuards.h"
#if BLE_BLUEDROID

#if BLE_GATT_SERVER_SUPPORTED

#include "BLE.h"
#include "gatt/BLEGattAttributes.bluedroid.h"
#include "server/BLEServer.bluedroid.h"
#include "core/BLEImplHelpers.h"
#include "gatt/BLECharacteristicValidation.h"
#include "esp32-hal-log.h"

#include <esp_gatts_api.h>

// --------------------------------------------------------------------------
// BLECharacteristic notification & descriptor helpers (Bluedroid)
// --------------------------------------------------------------------------

// Hands one notification (need_confirm=false) to the stack, or holds it if an
// indication on the same characteristic is currently in flight on connHandle
// (so ESP_GATTS_CONF_EVT can distinguish indication confirmations from
// notification completions).
BTStatus bleCharacteristicSendNotify(BLECharacteristic::Impl &impl, uint16_t connHandle, const uint8_t *data, size_t length) {
  BLEServer::Impl *server = impl.activeServer();
  if (!server) {
    return BTStatus::InvalidState;
  }

  esp_err_t err = ESP_OK;
  bool wasHeld = false;
  bool wasFull = false;
  bool pumpAfterReject = false;
  {
    BLELockGuard serverLock(server->mtx);
    if (!server->active || !server->started || server->gattsIf == ESP_GATT_IF_NONE || !server->connFind(connHandle) || !server->characteristicCanSend(&impl)) {
      return BTStatus::InvalidState;
    }
    auto *link = server->indicationLink(connHandle);
    if (link && link->inFlight.get() == &impl) {
      auto &ns = server->notifyStateFor(connHandle);
      if (ns.heldNotifies.size() >= BLEServer::Impl::kMaxHeldNotifications) {
        wasFull = true;
      } else {
        ns.heldNotifies.push_back({impl.shared_from_this(), std::vector<uint8_t>(data, data + length)});
        wasHeld = true;
      }
    } else {
      auto &ns = server->notifyStateFor(connHandle);
      ns.notifiesPending++;
      err = esp_ble_gatts_send_indicate(server->gattsIf, connHandle, impl.handle, length, const_cast<uint8_t *>(data), false);
      if (err != ESP_OK) {
        if (--ns.notifiesPending == 0 && ns.pumpWaiting) {
          ns.pumpWaiting = false;
          pumpAfterReject = true;
        }
      }
    }
  }

  if (wasHeld) {
    return BTStatus::OK;
  }
  if (wasFull) {
    log_w("Characteristic %s: notify conn=%u - too many held behind an unconfirmed indication", impl.uuid.toString().c_str(), connHandle);
    impl.dispatchStatus(BLECharacteristic::ERROR_GATT, 0);
    return BTStatus::Busy;
  }
  if (err != ESP_OK) {
    log_e("Characteristic %s: esp_ble_gatts_send_indicate (notify) conn=%u: %s", impl.uuid.toString().c_str(), connHandle, esp_err_to_name(err));
    impl.dispatchStatus((err == ESP_ERR_INVALID_STATE) ? BLECharacteristic::ERROR_NO_CLIENT : BLECharacteristic::ERROR_GATT, static_cast<uint32_t>(err));
    if (pumpAfterReject) {
      server->pumpIndications(connHandle);
    }
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

// The descriptor is only registered with the native stack when the GATT service is created/started, not here.
BLEDescriptor BLECharacteristic::createDescriptor(const BLEUUID &uuid, BLEPermission perms, size_t maxLen) {
  BLE_CHECK_IMPL(BLEDescriptor());

  // Stage-1 validation — reject descriptors that would be inaccessible or
  // violate the reserved-UUID access profile before the Impl is allocated.
  if (!bleValidateDescProps(uuid, _impl->uuid, perms)) {
    return BLEDescriptor();
  }

  auto desc = std::make_shared<BLEDescriptor::Impl>();
  desc->uuid = uuid;
  desc->chr = _impl.get();
  desc->permissions = perms;
  desc->maxLen = maxLen;
  desc->value.reserve(maxLen);
  impl.descriptors.push_back(desc);
  return BLEDescriptor(desc);
}

void BLEDescriptor::setPermissions(BLEPermission perms) {
  BLE_CHECK_IMPL();
  // Bluedroid applies descriptor permissions at GATT registration time
  // (see BLEServer::start's descriptor loop), so we stash the requested
  // permissions here.
  impl.permissions = perms;
}

#else /* !BLE_GATT_SERVER_SUPPORTED -- stubs */

void BLEDescriptor::setPermissions(BLEPermission) {}

#endif /* BLE_GATT_SERVER_SUPPORTED */

#endif /* BLE_BLUEDROID */
