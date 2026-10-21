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
 * @file BLEGattAttributes.bluedroid.h
 * @brief Bluedroid server-side GATT attribute Impls: BLEService, BLEDescriptor, BLECharacteristic.
 *
 * Services, characteristics, and descriptors keep no stack-specific state
 * beyond the shared *ImplCommon bases.
 */

#include "core/BLEGuards.h"
#if BLE_BLUEDROID

#include "gatt/BLEGattAttributesImpl.h"

struct BLEService::Impl : BLEServiceImplCommon {};

struct BLEDescriptor::Impl : BLEDescriptorImplCommon {};

struct BLECharacteristic::Impl : BLECharacteristicImplCommon {};

#endif /* BLE_BLUEDROID */
