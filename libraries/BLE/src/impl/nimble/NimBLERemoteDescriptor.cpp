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
 * @file NimBLERemoteDescriptor.cpp
 * @brief NimBLE GATT client implementation for reading and writing remote GATT descriptors.
 */

#include "impl/BLEGuards.h"
#if BLE_NIMBLE

#include "NimBLERemoteTypes.h"
#include "esp32-hal-log.h"
#include "impl/BLEImplHelpers.h"

#if BLE_GATT_CLIENT_SUPPORTED

// ==========================================================================
// BLERemoteDescriptor implementation
// ==========================================================================

/**
 * @brief Reads the raw value of the remote descriptor into a string.
 * @param timeoutMs Wait time for the stack callback to complete the read.
 * @return Descriptor payload, or an empty string on error, timeout, or disconnect.
 * @note Blocking: @c ble_gattc_read plus synchronous wait; safe only when the connection is in a valid GATT state.
 */
String BLERemoteDescriptor::readValue(uint32_t timeoutMs) {
  if (!_impl || !isGattConnected(_impl->connHandle)) {
    return "";
  }

  _impl->lastValue.clear();
  _impl->readSync.take();

  int rc = ble_gattc_read(_impl->connHandle, _impl->handle, Impl::readCb, _impl.get());
  if (rc != 0) {
    log_e("RemoteDescriptor: ble_gattc_read rc=%d", rc);
    _impl->readSync.give(BTStatus::Fail);
    return "";
  }

  BTStatus status = _impl->readSync.wait(timeoutMs);
  if (status != BTStatus::OK) {
    log_w("RemoteDescriptor: read failed/timed out (handle=0x%04x)", _impl->handle);
    return "";
  }
  return String(reinterpret_cast<const char *>(_impl->lastValue.data()), _impl->lastValue.size());
}

/**
 * @brief Writes a value to the remote descriptor, optionally waiting for a write response.
 * @param data Bytes to write.
 * @param len Number of bytes.
 * @param withResponse If true, uses a confirmed write and waits; if false, uses a write-without-response path when supported.
 * @return Operation result (success, timeout, or failure).
 * @note Blocking: confirmed path uses @c ble_gattc_write_flat and waits; CCCD-style descriptor writes follow the same contract as characteristic writes.
 */
BTStatus BLERemoteDescriptor::writeValue(const uint8_t *data, size_t len, bool withResponse) {
  if (!_impl || !isGattConnected(_impl->connHandle)) {
    log_w("RemoteDescriptor: writeValue called but not connected");
    return BTStatus::InvalidState;
  }

  if (!withResponse) {
    int rc = ble_gattc_write_no_rsp_flat(_impl->connHandle, _impl->handle, data, len);
    if (rc != 0) {
      log_e("RemoteDescriptor: write no-rsp failed rc=%d", rc);
      return BTStatus::Fail;
    }
    return BTStatus::OK;
  }

  _impl->writeSync.take();
  int rc = ble_gattc_write_flat(_impl->connHandle, _impl->handle, data, len, Impl::writeCb, _impl.get());
  if (rc != 0) {
    log_e("RemoteDescriptor: ble_gattc_write_flat rc=%d", rc);
    _impl->writeSync.give(BTStatus::Fail);
    return BTStatus::Fail;
  }
  BTStatus status = _impl->writeSync.wait(5000);
  if (status != BTStatus::OK) {
    log_w("RemoteDescriptor: write timed out (handle=0x%04x)", _impl->handle);
  }
  return status;
}

/**
 * @brief NimBLE callback completing a GATT read for a remote descriptor.
 * @param connHandle Connection handle.
 * @param error GATT error from the stack.
 * @param attr Read attribute payload, or nullptr on error paths.
 * @param arg Opaque @c BLERemoteDescriptor::Impl pointer.
 * @return Zero (NimBLE GATT client callback convention).
 */
int BLERemoteDescriptor::Impl::readCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  auto *impl = static_cast<BLERemoteDescriptor::Impl *>(arg);
  if (!impl) {
    return 0;
  }

  if (error->status == 0 && attr != nullptr) {
    uint16_t len = OS_MBUF_PKTLEN(attr->om);
    impl->lastValue.resize(len);
    os_mbuf_copydata(attr->om, 0, len, impl->lastValue.data());
    impl->readSync.give(BTStatus::OK);
  } else {
    impl->readSync.give((error->status == BLE_HS_EDONE) ? BTStatus::OK : BTStatus::Fail);
  }
  return 0;
}

/**
 * @brief NimBLE callback completing a GATT write (including CCCD) for a remote descriptor.
 * @param connHandle Connection handle.
 * @param error GATT error from the stack.
 * @param attr Optional attribute; may be null on some paths.
 * @param arg Opaque @c BLERemoteDescriptor::Impl pointer.
 * @return Zero (NimBLE GATT client callback convention).
 */
int BLERemoteDescriptor::Impl::writeCb(uint16_t connHandle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr, void *arg) {
  auto *impl = static_cast<BLERemoteDescriptor::Impl *>(arg);
  if (!impl) {
    return 0;
  }
  impl->writeSync.give((error->status == 0) ? BTStatus::OK : BTStatus::Fail);
  return 0;
}

#else /* !BLE_GATT_CLIENT_SUPPORTED -- stubs */

// Stubs for BLE_GATT_CLIENT_SUPPORTED == 0: empty read, NotSupported write.

String BLERemoteDescriptor::readValue(uint32_t) {
  log_w("GATT client not supported");
  return "";
}

BTStatus BLERemoteDescriptor::writeValue(const uint8_t *, size_t, bool) {
  log_w("GATT client not supported");
  return BTStatus::NotSupported;
}

#endif /* BLE_GATT_CLIENT_SUPPORTED */

#endif /* BLE_NIMBLE */
