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

#include "impl/common/BLEGuards.h"
#if BLE_NIMBLE

#include "impl/common/BLEServerImpl.h"

#include <host/ble_gap.h>
#include <memory>

/**
 * @brief NimBLE GATT-server implementation (@c BLEServer::Impl).
 *
 * Defined in @c impl/nimble/, so everything here is NimBLE-specific; it inherits the
 * stack-agnostic @c BLEServerImplCommon, giving one uniform @c impl.member type. Layer is
 * disclosed by file/type: members on @c BLEServer::Impl are NimBLE, members on
 * @c BLEServerImplCommon are shared.
 */
struct BLEServer::Impl : BLEServerImplCommon {
  static int gapEventHandler(struct ble_gap_event *event, void *arg);
};

#endif /* BLE_NIMBLE */
