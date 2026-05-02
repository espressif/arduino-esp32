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
 * @file NimBLEDescriptor.cpp
 * @brief NimBLE server-side GATT descriptor permission and attribute flag handling.
 */

#include "impl/BLEGuards.h"
#if BLE_NIMBLE

#include "NimBLEDescriptor.h"
#include "impl/BLEImplHelpers.h"
#include "esp32-hal-log.h"

#if BLE_GATT_SERVER_SUPPORTED

// --------------------------------------------------------------------------
// BLEDescriptor -- NimBLE
// --------------------------------------------------------------------------

/**
 * @brief Maps @c BLEPermission flags to NimBLE @c ble_att / attribute access flags and stores them on the implementation.
 * @param perms Logical permissions to apply before registration.
 */
void BLEDescriptor::setPermissions(BLEPermission perms) {
  BLE_CHECK_IMPL();
  uint8_t flags = 0;
  uint16_t p = static_cast<uint16_t>(perms);
  if (p & static_cast<uint16_t>(BLEPermission::Read)) {
    flags |= BLE_ATT_F_READ;
  }
  if (p & static_cast<uint16_t>(BLEPermission::Write)) {
    flags |= BLE_ATT_F_WRITE;
  }
  if (p & static_cast<uint16_t>(BLEPermission::ReadEncrypted)) {
    flags |= BLE_ATT_F_READ | BLE_ATT_F_READ_ENC;
  }
  if (p & static_cast<uint16_t>(BLEPermission::ReadAuthenticated)) {
    flags |= BLE_ATT_F_READ | BLE_ATT_F_READ_AUTHEN;
  }
  if (p & static_cast<uint16_t>(BLEPermission::ReadAuthorized)) {
    flags |= BLE_ATT_F_READ | BLE_ATT_F_READ_AUTHOR;
  }
  if (p & static_cast<uint16_t>(BLEPermission::WriteEncrypted)) {
    flags |= BLE_ATT_F_WRITE | BLE_ATT_F_WRITE_ENC;
  }
  if (p & static_cast<uint16_t>(BLEPermission::WriteAuthenticated)) {
    flags |= BLE_ATT_F_WRITE | BLE_ATT_F_WRITE_AUTHEN;
  }
  if (p & static_cast<uint16_t>(BLEPermission::WriteAuthorized)) {
    flags |= BLE_ATT_F_WRITE | BLE_ATT_F_WRITE_AUTHOR;
  }
  // Store both the native flags and the source BLEPermission so the
  // cross-backend validator (which runs again at registration time) can
  // reason about logical permissions after a post-creation mutation.
  impl.attFlags = flags;
  impl.permissions = perms;
}

#else /* !BLE_GATT_SERVER_SUPPORTED -- stubs */

// Stubs for BLE_GATT_SERVER_SUPPORTED == 0: setPermissions is a no-op (log only).

void BLEDescriptor::setPermissions(BLEPermission) {
  log_w("GATT server not supported");
}

#endif /* BLE_GATT_SERVER_SUPPORTED */

#endif /* BLE_NIMBLE */
