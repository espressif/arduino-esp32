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
 * @file BluedroidGattAttributes.cpp
 * @brief Bluedroid server-side GATT attributes: characteristic notify/indicate and descriptor permissions (BLE_GATT_SERVER_SUPPORTED).
 */

#include "impl/common/BLEGuards.h"
#if BLE_BLUEDROID

// ==========================================================================
// BLECharacteristic - Bluedroid notify/indicate and descriptor helpers
// ==========================================================================

#if BLE_GATT_SERVER_SUPPORTED

#include "BLE.h"

#include "BluedroidGattAttributes.h"
#include "BluedroidServer.h"
#include "impl/common/BLEImplHelpers.h"
#include "impl/common/BLEConnInfoData.h"
#include "impl/common/BLECharacteristicValidation.h"
#include "esp32-hal-log.h"

#include <esp_gatts_api.h>
#include <vector>

// --------------------------------------------------------------------------
// BLECharacteristic -- Bluedroid
// --------------------------------------------------------------------------

// API contract is documented on the declarations in the public BLE*.h headers; the definitions below carry implementation notes only.

// Bluedroid sends notifications through esp_ble_gatts_send_indicate with need_confirm=false.
BTStatus BLECharacteristic::notify(const uint8_t *data, size_t length) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!(impl.properties & BLEProperty::Notify)) {
    log_w("Characteristic %s: notify called but Notify property not set", impl.uuid.toString().c_str());
    return BTStatus::InvalidState;
  }
  if (!impl.service || !impl.service->server) {
    log_e("Characteristic %s: notify called but service/server not set", impl.uuid.toString().c_str());
    return BTStatus::InvalidState;
  }
  auto *server = impl.service->server;

  // When no explicit payload is given, snapshot the stored value under the
  // per-characteristic mtx so a concurrent setValue() cannot reallocate it
  // mid-send.
  std::vector<uint8_t> stored;
  const uint8_t *sendData = data;
  size_t sendLen = length;
  if (!sendData || sendLen == 0) {
    BLELockGuard lock(impl.mtx);
    stored = impl.value;
    sendData = stored.data();
    sendLen = stored.size();
  }

  BLELockGuard lock(server->mtx);
  for (auto &sub : impl.subscribers) {
    if (sub.second & BLE_CCCD_NOTIFY) {
      esp_ble_gatts_send_indicate(server->gattsIf, sub.first, impl.handle, sendLen, const_cast<uint8_t *>(sendData), false);
    }
  }
  return BTStatus::OK;
}

BTStatus BLECharacteristic::notify(uint16_t connHandle, const uint8_t *data, size_t length) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!(impl.properties & BLEProperty::Notify)) {
    log_w("Characteristic %s: notify(conn) called but Notify property not set", impl.uuid.toString().c_str());
    return BTStatus::InvalidState;
  }
  if (!impl.service || !impl.service->server) {
    log_e("Characteristic %s: notify(conn) called but service/server not set", impl.uuid.toString().c_str());
    return BTStatus::InvalidState;
  }
  auto *server = impl.service->server;

  std::vector<uint8_t> stored;
  const uint8_t *sendData = data;
  size_t sendLen = length;
  if (!sendData || sendLen == 0) {
    BLELockGuard lock(impl.mtx);
    stored = impl.value;
    sendData = stored.data();
    sendLen = stored.size();
  }

  esp_err_t err = esp_ble_gatts_send_indicate(server->gattsIf, connHandle, impl.handle, sendLen, const_cast<uint8_t *>(sendData), false);
  if (err != ESP_OK) {
    log_e("Characteristic %s: esp_ble_gatts_send_indicate (notify) conn=%u: %s", impl.uuid.toString().c_str(), connHandle, esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

// Indications use esp_ble_gatts_send_indicate with need_confirm=true to request client confirmation.
BTStatus BLECharacteristic::indicate(const uint8_t *data, size_t length) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!(impl.properties & BLEProperty::Indicate)) {
    log_w("Characteristic %s: indicate called but Indicate property not set", impl.uuid.toString().c_str());
    return BTStatus::InvalidState;
  }
  if (!impl.service || !impl.service->server) {
    log_e("Characteristic %s: indicate called but service/server not set", impl.uuid.toString().c_str());
    return BTStatus::InvalidState;
  }
  auto *server = impl.service->server;

  std::vector<uint8_t> stored;
  const uint8_t *sendData = data;
  size_t sendLen = length;
  if (!sendData || sendLen == 0) {
    BLELockGuard lock(impl.mtx);
    stored = impl.value;
    sendData = stored.data();
    sendLen = stored.size();
  }

  BLELockGuard lock(server->mtx);
  for (auto &sub : impl.subscribers) {
    if (sub.second & BLE_CCCD_INDICATE) {
      esp_ble_gatts_send_indicate(server->gattsIf, sub.first, impl.handle, sendLen, const_cast<uint8_t *>(sendData), true);
    }
  }
  return BTStatus::OK;
}

BTStatus BLECharacteristic::indicate(uint16_t connHandle, const uint8_t *data, size_t length) {
  BLE_CHECK_IMPL(BTStatus::InvalidState);
  if (!(impl.properties & BLEProperty::Indicate)) {
    log_w("Characteristic %s: indicate(conn) called but Indicate property not set", impl.uuid.toString().c_str());
    return BTStatus::InvalidState;
  }
  if (!impl.service || !impl.service->server) {
    log_e("Characteristic %s: indicate(conn) called but service/server not set", impl.uuid.toString().c_str());
    return BTStatus::InvalidState;
  }
  auto *server = impl.service->server;

  std::vector<uint8_t> stored;
  const uint8_t *sendData = data;
  size_t sendLen = length;
  if (!sendData || sendLen == 0) {
    BLELockGuard lock(impl.mtx);
    stored = impl.value;
    sendData = stored.data();
    sendLen = stored.size();
  }

  esp_err_t err = esp_ble_gatts_send_indicate(server->gattsIf, connHandle, impl.handle, sendLen, const_cast<uint8_t *>(sendData), true);
  if (err != ESP_OK) {
    log_e("Characteristic %s: esp_ble_gatts_send_indicate (indicate) conn=%u: %s", impl.uuid.toString().c_str(), connHandle, esp_err_to_name(err));
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
  desc->value.reserve(maxLen);
  impl.descriptors.push_back(desc);
  return BLEDescriptor(desc);
}

void BLECharacteristic::setDescription(const String &desc) {
  if (!_impl) {
    return;
  }
  auto existing = getDescriptor(BLEUUID(BLE_DSC_UUID16_USER_DESC));
  if (existing) {
    existing.setValue(desc);
  } else {
    auto d = createDescriptor(BLEUUID(BLE_DSC_UUID16_USER_DESC), BLEPermission::Read, desc.length() + 1);
    d.setValue(desc);
  }
}

#else /* !BLE_GATT_SERVER_SUPPORTED -- stubs */

#include "BLE.h"
#include "esp32-hal-log.h"

// Stubs for BLE_GATT_SERVER_SUPPORTED == 0: notify/indicate return NotSupported; no descriptors.

BTStatus BLECharacteristic::notify(const uint8_t *, size_t) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLECharacteristic::notify(uint16_t, const uint8_t *, size_t) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLECharacteristic::indicate(const uint8_t *, size_t) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BTStatus BLECharacteristic::indicate(uint16_t, const uint8_t *, size_t) {
  log_w("GATT server not supported");
  return BTStatus::NotSupported;
}

BLEDescriptor BLECharacteristic::createDescriptor(const BLEUUID &, BLEPermission, size_t) {
  return BLEDescriptor();
}

void BLECharacteristic::setDescription(const String &) {}

#endif /* BLE_GATT_SERVER_SUPPORTED */

// ==========================================================================
// BLEDescriptor - Bluedroid registration-time permission storage
// ==========================================================================

#if BLE_GATT_SERVER_SUPPORTED

#include "BluedroidGattAttributes.h"
#include "impl/common/BLEImplHelpers.h"

void BLEDescriptor::setPermissions(BLEPermission perms) {
  BLE_CHECK_IMPL();
  // Bluedroid applies descriptor permissions at GATT registration time
  // (see BluedroidServer::startServer's descriptor loop), so we just stash
  // the requested permissions here. Calling this after the service has
  // been started will not retroactively update the native permissions.
  impl.permissions = perms;
}

#else /* !BLE_GATT_SERVER_SUPPORTED -- stubs */

#include "esp32-hal-log.h"

// Stubs for BLE_GATT_SERVER_SUPPORTED == 0: setPermissions is a no-op.

void BLEDescriptor::setPermissions(BLEPermission) {}

#endif /* BLE_GATT_SERVER_SUPPORTED */

#endif /* BLE_BLUEDROID */
