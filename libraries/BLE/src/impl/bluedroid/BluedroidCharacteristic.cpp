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
 * @file BluedroidCharacteristic.cpp
 * @brief Bluedroid GATT server characteristic notify, indicate, and User Description descriptor helpers (BLE_GATT_SERVER_SUPPORTED).
 */

#include "impl/BLEGuards.h"
#if BLE_BLUEDROID

#if BLE_GATT_SERVER_SUPPORTED

#include "BLE.h"

#include "BluedroidCharacteristic.h"
#include "BluedroidService.h"
#include "BluedroidServer.h"
#include "impl/BLEImplHelpers.h"
#include "impl/BLEConnInfoData.h"
#include "impl/BLECharacteristicValidation.h"
#include "esp32-hal-log.h"

#include <esp_gatts_api.h>
#include <vector>

// --------------------------------------------------------------------------
// BLECharacteristic -- Bluedroid
// --------------------------------------------------------------------------

/**
 * @brief Sends a notification to each subscribed connection using @c esp_ble_gatts_send_indicate with @a need_confirm false.
 * @param data Optional payload; if null or length 0, the characteristic's current value is used.
 * @param length Size of @a data in bytes, or 0 to send the stored value.
 * @return @c OK on success, or an error if the characteristic, notify property, or server back-reference is invalid.
 */
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

  const uint8_t *sendData = data;
  size_t sendLen = length;
  if (!sendData || sendLen == 0) {
    sendData = impl.value.data();
    sendLen = impl.value.size();
  }

  BLELockGuard lock(server->mtx);
  for (auto &sub : impl.subscribers) {
    if (sub.second & 0x0001) {
      esp_ble_gatts_send_indicate(server->gattsIf, sub.first, impl.handle, sendLen, const_cast<uint8_t *>(sendData), false);
    }
  }
  return BTStatus::OK;
}

/**
 * @brief Sends a notification on one connection if the client has notifications enabled.
 * @param connHandle GAP or GATT connection id for the peer.
 * @param data Optional payload; if null or length 0, the stored value is used.
 * @param length Byte count for @a data, or 0 to use the stored value.
 * @return @c OK when @c esp_ble_gatts_send_indicate succeeds, or a failure status on error.
 */
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

  const uint8_t *sendData = data;
  size_t sendLen = length;
  if (!sendData || sendLen == 0) {
    sendData = impl.value.data();
    sendLen = impl.value.size();
  }

  esp_err_t err = esp_ble_gatts_send_indicate(server->gattsIf, connHandle, impl.handle, sendLen, const_cast<uint8_t *>(sendData), false);
  if (err != ESP_OK) {
    log_e("Characteristic %s: esp_ble_gatts_send_indicate (notify) conn=%u: %s", impl.uuid.toString().c_str(), connHandle, esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

/**
 * @brief Sends an indication to all connections that have indication enabled ( @c esp_ble_gatts_send_indicate with confirmation).
 * @param data Optional payload; if null or length 0, the stored characteristic value is used.
 * @param length Size of @a data, or 0 to use the stored value.
 * @return @c OK when the loop completes, or an error if properties or server wiring are wrong.
 */
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

  const uint8_t *sendData = data;
  size_t sendLen = length;
  if (!sendData || sendLen == 0) {
    sendData = impl.value.data();
    sendLen = impl.value.size();
  }

  BLELockGuard lock(server->mtx);
  for (auto &sub : impl.subscribers) {
    if (sub.second & 0x0002) {
      esp_ble_gatts_send_indicate(server->gattsIf, sub.first, impl.handle, sendLen, const_cast<uint8_t *>(sendData), true);
    }
  }
  return BTStatus::OK;
}

/**
 * @brief Sends an indication on a single connection.
 * @param connHandle Target connection.
 * @param data Optional payload; if null or length 0, the stored value is used.
 * @param length Byte length of @a data, or 0 to use the stored value.
 * @return @c OK on @c ESP_OK, otherwise @c Fail.
 */
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

  const uint8_t *sendData = data;
  size_t sendLen = length;
  if (!sendData || sendLen == 0) {
    sendData = impl.value.data();
    sendLen = impl.value.size();
  }

  esp_err_t err = esp_ble_gatts_send_indicate(server->gattsIf, connHandle, impl.handle, sendLen, const_cast<uint8_t *>(sendData), true);
  if (err != ESP_OK) {
    log_e("Characteristic %s: esp_ble_gatts_send_indicate (indicate) conn=%u: %s", impl.uuid.toString().c_str(), connHandle, esp_err_to_name(err));
    return BTStatus::Fail;
  }
  return BTStatus::OK;
}

/**
 * @brief Creates a descriptor @c Impl, runs validation, and appends it to this characteristic (registration happens with the GATT service).
 * @param uuid Descriptor UUID.
 * @param perms Att permissions to enforce after registration.
 * @param maxLen Reserved capacity for the value buffer.
 * @return A @c BLEDescriptor facade, or an empty object if validation fails.
 */
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

/**
 * @brief Sets the standard User Description (0x2901) string, creating the descriptor if missing.
 * @param desc UTF-8 or Arduino @c String content for the description.
 */
void BLECharacteristic::setDescription(const String &desc) {
  if (!_impl) {
    return;
  }
  auto existing = getDescriptor(BLEUUID(static_cast<uint16_t>(0x2901)));
  if (existing) {
    existing.setValue(desc);
  } else {
    auto d = createDescriptor(BLEUUID(static_cast<uint16_t>(0x2901)), BLEPermission::Read, desc.length() + 1);
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

#endif /* BLE_BLUEDROID */
