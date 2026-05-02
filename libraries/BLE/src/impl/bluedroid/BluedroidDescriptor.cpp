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
 * @file BluedroidDescriptor.cpp
 * @brief Bluedroid server-side GATT descriptor: stores logical permissions for registration-time application.
 */

#include "impl/BLEGuards.h"
#if BLE_BLUEDROID

#if BLE_GATT_SERVER_SUPPORTED

#include "BluedroidDescriptor.h"
#include "impl/BLEImplHelpers.h"

// --------------------------------------------------------------------------
// BLEDescriptor -- Bluedroid
// --------------------------------------------------------------------------

/**
 * @brief Records descriptor permissions; native Bluedroid applies them when the GATT DB is registered.
 * @param perms Logical permissions to use at @c startServer / registration.
 */
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
