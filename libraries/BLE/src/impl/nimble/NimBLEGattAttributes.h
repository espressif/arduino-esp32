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

#pragma once

/**
 * @file NimBLEGattAttributes.h
 * @brief NimBLE server-side GATT attribute Impls: BLEService, BLEDescriptor, BLECharacteristic.
 */

#include "impl/common/BLEGuards.h"
#if BLE_NIMBLE

#include "impl/common/BLEGattAttributesImpl.h"
#include "NimBLEConnInfo.h"
#include "NimBLEUUID.h"

#include <host/ble_hs.h>
#include <host/ble_gatt.h>
#include <host/ble_att.h>
#include <vector>
#include <memory>

/**
 * @brief NimBLE GATT-service implementation (@c BLEService::Impl).
 *
 * Defined in @c impl/nimble/, so everything here is NimBLE-specific; it inherits the
 * stack-agnostic @c BLEServiceImplCommon, giving one uniform @c impl.member type.
 */
struct BLEService::Impl : BLEServiceImplCommon {
  ble_uuid_any_t nimbleUUID{};
};

/**
 * @brief NimBLE-specific state for a GATT descriptor (@c BLEDescriptor::Impl).
 *
 * @c attFlags is stored alongside the shared @c permissions so the cross-backend
 * validator can reason about the original BLEPermission bits as well as the
 * NimBLE-native att flags.
 */
struct BLEDescriptor::Impl : BLEDescriptorImplCommon {
  uint8_t attFlags = 0;
  ble_uuid_any_t nimbleUUID{};
};

/**
 * @brief NimBLE GATT-characteristic implementation (@c BLECharacteristic::Impl).
 *
 * Defined in @c impl/nimble/, so everything here is NimBLE-specific; it inherits the
 * stack-agnostic @c BLECharacteristicImplCommon, giving one uniform @c impl.member type.
 */
struct BLECharacteristic::Impl : BLECharacteristicImplCommon {
  ble_uuid_any_t nimbleUUID{};

  static int accessCallback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
  static int descAccessCallback(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
};

/**
 * @brief Builds the NimBLE @c ble_gatt_svc_def table from the in-memory service tree, then calls @c ble_gatts_count_cfg and @c ble_gatts_add_svcs.
 * @param services All primary services to register, each with characteristics and descriptors.
 * @return 0 on success, or a negative NimBLE error code (e.g. @c BLE_HS_EINVAL) if validation or registration fails.
 * @note Called from BLEServer::start() before ble_gatts_start().
 */
int nimbleRegisterGattServices(const std::vector<std::shared_ptr<BLEService::Impl>> &services);

#endif /* BLE_NIMBLE */
