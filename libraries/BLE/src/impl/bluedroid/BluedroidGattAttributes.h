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

#pragma once

/**
 * @file BluedroidGattAttributes.h
 * @brief Bluedroid server-side GATT attribute Impls: BLEService, BLEDescriptor, BLECharacteristic.
 *
 * Bluedroid keeps no stack-specific state beyond the shared *ImplCommon bases,
 * so all three are empty derivations; they exist to give the backend contract a
 * concrete per-type Impl.
 */

#include "impl/common/BLEGuards.h"
#if BLE_BLUEDROID

#include "impl/common/BLEGattAttributesImpl.h"
#include <vector>

struct BLEService::Impl : BLEServiceImplCommon {};

struct BLEDescriptor::Impl : BLEDescriptorImplCommon {};

struct BLECharacteristic::Impl : BLECharacteristicImplCommon {};

#endif /* BLE_BLUEDROID */
