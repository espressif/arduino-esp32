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
 * @file NimBLERemoteService.cpp
 * @brief NimBLE implementation of GATT client remote service discovery and characteristic lookup.
 */

#include "impl/BLEGuards.h"
#if BLE_NIMBLE

#include "NimBLERemoteTypes.h"
#include "NimBLEUUID.h"
#include "esp32-hal-log.h"
#include "impl/BLEImplHelpers.h"

#if BLE_GATT_CLIENT_SUPPORTED

// ==========================================================================
// BLERemoteService implementation
// ==========================================================================

/**
 * @brief Resolves the BLE client that owns this remote GATT service.
 * @return Client handle, or an empty client if the implementation is missing.
 */
BLEClient BLERemoteService::getClient() const {
  return _impl && _impl->client ? BLEClient(_impl->client->shared_from_this()) : BLEClient();
}

/**
 * @brief Finds a remote characteristic by UUID, discovering characteristics for the service if needed.
 * @param uuid Target characteristic UUID (compared in 128-bit form).
 * @return Remote characteristic wrapper, or empty if not found, not connected, or discovery failed.
 * @note Blocking: uses a synchronous @c ble_gattc_disc_all_chrs / discovery callback with wait on failure paths.
 */
BLERemoteCharacteristic BLERemoteService::getCharacteristic(const BLEUUID &uuid) {
  BLE_CHECK_IMPL(BLERemoteCharacteristic());

  if (!impl.charsDiscovered) {
    if (!isGattConnected(impl.connHandle)) {
      return BLERemoteCharacteristic();
    }

    impl.chrDiscoverSync.take();
    int rc = ble_gattc_disc_all_chrs(impl.connHandle, impl.startHandle, impl.endHandle, Impl::chrDiscoveryCb, _impl.get());
    if (rc != 0) {
      impl.chrDiscoverSync.give(BTStatus::Fail);
      return BLERemoteCharacteristic();
    }
    BTStatus status = impl.chrDiscoverSync.wait(10000);
    if (status == BTStatus::OK) {
      impl.charsDiscovered = true;
      for (auto &c : impl.characteristics) {
        c->connHandle = impl.connHandle;
        c->service = _impl.get();
      }
    }
  }

  BLEUUID target = uuid.to128();
  for (auto &c : impl.characteristics) {
    if (c->uuid.to128() == target) {
      return BLERemoteCharacteristic(c);
    }
  }
  return BLERemoteCharacteristic();
}

/**
 * @brief Returns all characteristics for this service, triggering discovery when the cache is empty.
 * @return List of remote characteristics; may be empty if the service has none or discovery failed.
 * @note Blocking: may run the same discovery as @ref getCharacteristic when characteristics were not yet cached.
 */
std::vector<BLERemoteCharacteristic> BLERemoteService::getCharacteristics() const {
  std::vector<BLERemoteCharacteristic> result;
  BLE_CHECK_IMPL(result);

  // Lazy discovery: if the caller never triggered a targeted getCharacteristic(uuid),
  // NimBLE's per-service characteristic cache is still empty. Trigger discovery
  // transparently so the aggregate getter behaves symmetrically with the targeted one.
  if (!impl.charsDiscovered && isGattConnected(impl.connHandle)) {
    auto *self = const_cast<BLERemoteService *>(this);
    (void)self->getCharacteristic(BLEUUID(static_cast<uint16_t>(0)));
  }

  for (auto &c : impl.characteristics) {
    result.push_back(BLERemoteCharacteristic(c));
  }
  return result;
}

/**
 * @brief NimBLE callback for GATT characteristic discovery; fills the per-service list and signals completion.
 * @param connHandle Connection handle (unused for validation here).
 * @param error GATT error status; final completion uses @c BLE_HS_EDONE.
 * @param chr Discovered characteristic field, or nullptr when the iteration completes.
 * @param arg Opaque @c BLERemoteService::Impl pointer.
 * @return Zero to continue; stack convention for NimBLE GATT client callbacks.
 */
int BLERemoteService::Impl::chrDiscoveryCb(uint16_t connHandle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr, void *arg) {
  auto *impl = static_cast<BLERemoteService::Impl *>(arg);
  if (!impl) {
    return 0;
  }

  if (error->status == 0 && chr != nullptr) {
    auto cImpl = std::make_shared<BLERemoteCharacteristic::Impl>();
    cImpl->uuid = nimbleToUuid(chr->uuid);
    cImpl->defHandle = chr->def_handle;
    cImpl->valHandle = chr->val_handle;
    cImpl->properties = chr->properties;
    impl->characteristics.push_back(cImpl);
    return 0;
  }

  if (error->status == BLE_HS_EDONE) {
    impl->chrDiscoverSync.give(BTStatus::OK);
  } else {
    impl->chrDiscoverSync.give(BTStatus::Fail);
  }
  return 0;
}

#else /* !BLE_GATT_CLIENT_SUPPORTED -- stubs */

// Stubs for BLE_GATT_CLIENT_SUPPORTED == 0: log, return empty client/characteristic or lists.

BLEClient BLERemoteService::getClient() const {
  log_w("GATT client not supported");
  return BLEClient();
}

BLERemoteCharacteristic BLERemoteService::getCharacteristic(const BLEUUID &) {
  log_w("GATT client not supported");
  return BLERemoteCharacteristic();
}

std::vector<BLERemoteCharacteristic> BLERemoteService::getCharacteristics() const {
  log_w("GATT client not supported");
  return {};
}

#endif /* BLE_GATT_CLIENT_SUPPORTED */

#endif /* BLE_NIMBLE */
