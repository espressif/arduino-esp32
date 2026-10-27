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

#include "core/BLEValidationHooks.h"

// --------------------------------------------------------------------------
// BLECharacteristic notification & descriptor helpers (Bluedroid)
// --------------------------------------------------------------------------

// Hands one notification (need_confirm=false) to the stack and records it in
// pendingNotifies so ESP_GATTS_CONF_EVT can distinguish local notification
// completions from peer indication confirmations without delaying notifications.
BTStatus bleCharacteristicSendNotify(BLECharacteristic::Impl &impl, uint16_t connHandle, const uint8_t *data, size_t length) {
  auto serverOwner = impl.activeServer();
  BLEServer::Impl *server = serverOwner.get();
  if (!server) {
    return BTStatus::InvalidState;
  }

  esp_gatt_if_t gattsIf = ESP_GATT_IF_NONE;
  uint16_t attrHandle = 0;
  {
    BLELockGuard serverLock(server->mtx);
    if (!server->active || !server->started || server->gattsIf == ESP_GATT_IF_NONE || !server->connFind(connHandle) || !server->characteristicCanSend(&impl)) {
      return BTStatus::InvalidState;
    }
    gattsIf = server->gattsIf;
    attrHandle = impl.handle;
    server->addPendingNotify(connHandle, attrHandle);
  }

#if BLE_VALIDATION_HOOKS
  if (BLEHooks.gatt.holdNotifySubmit.load(std::memory_order_acquire)) {
    BLEHooks.gatt.notifySubmitEntered.store(true, std::memory_order_release);
    BLEValidationHooks::holdWhile(BLEHooks.gatt.holdNotifySubmit);
  }
#endif
  esp_err_t err = esp_ble_gatts_send_indicate(gattsIf, connHandle, attrHandle, length, const_cast<uint8_t *>(data), false);
  if (err != ESP_OK) {
    {
      BLELockGuard serverLock(server->mtx);
      server->unreservePendingNotify(connHandle, attrHandle);
    }
    log_e("Characteristic %s: esp_ble_gatts_send_indicate (notify) conn=%u: %s", impl.uuid.toString().c_str(), connHandle, esp_err_to_name(err));
    impl.dispatchStatus((err == ESP_ERR_INVALID_STATE) ? BLECharacteristic::ERROR_NO_CLIENT : BLECharacteristic::ERROR_GATT, static_cast<uint32_t>(err));
    return BTStatus::Fail;
  }
  {
    BLELockGuard serverLock(server->mtx);
    server->markNotifySubmitted(connHandle, attrHandle);
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
  desc->chrOwner = _impl;
  desc->permissions = perms;
  desc->maxLen = maxLen;
  desc->value.reserve(maxLen);
  {
    BLELockGuard lock(impl.mtx);
    if (!impl.service) {
      log_w("Characteristic %s: createDescriptor called while detached", impl.uuid.toString().c_str());
      return BLEDescriptor();
    }
    desc->chr = _impl.get();
    impl.descriptors.push_back(desc);
  }
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
