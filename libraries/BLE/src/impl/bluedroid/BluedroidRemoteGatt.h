/*
 * Copyright 2017-2026 Espressif Systems (Shanghai) PTE LTD
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
#if BLE_BLUEDROID

#include "BLEClient.h"
#include "BLERemoteService.h"
#include "BLERemoteCharacteristic.h"
#include "impl/common/BLERemoteGattImpl.h"
#include "BLERemoteDescriptor.h"
#include "BLEUUID.h"

#include <vector>
#include <memory>
#include <stdint.h>

// Bluedroid stores the remote (client-side) GATT state entirely in the shared
// *ImplCommon bases -- discovery is synchronous, so there is no per-stack sync
// or callback plumbing to add on top.
struct BLERemoteService::Impl : BLERemoteServiceImplCommon {};

struct BLERemoteCharacteristic::Impl : BLERemoteCharacteristicImplCommon {};

struct BLERemoteDescriptor::Impl : BLERemoteDescriptorImplCommon {};

#endif /* BLE_BLUEDROID */
