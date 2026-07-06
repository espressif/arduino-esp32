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

/**
 * @file
 * @brief Compile-time verification of the backend contract (see libraries/BLE/DESIGN.md).
 *
 * Included at the very end of @c impl/BLEBackend.h, i.e. after the active backend
 * has defined every per-type @c Foo::Impl. It turns two silent contract gaps into
 * clear @c static_assert failures at the point of use:
 *
 *   1. A class that has a shared @c FooImplCommon base MUST derive @c Foo::Impl from it
 *      (the two-layer model). A backend that forgets the base -- or accidentally
 *      reintroduces an intermediate mixin -- fails here instead of silently duplicating
 *      shared state.
 *   2. A fully backend-specific class (no common base) MUST still define a complete
 *      @c Foo::Impl. A missing definition fails here instead of at some distant use site.
 *
 * The linker remains the ultimate backstop for missing method definitions; this header
 * only checks the type-level shape of the contract.
 *
 * Note: @c BLEClass::Impl and @c BLEL2CAPServer::Impl are intentionally not checked here.
 * They are defined in translation units / feature-gated headers that are not pulled in by
 * the umbrella selector, so their shape is validated where they are compiled instead.
 */

#include "impl/common/BLEGuards.h"
#if BLE_ENABLED

#include <type_traits>

#include "impl/common/BLEServerImpl.h"
#include "impl/common/BLEGattAttributesImpl.h"
#include "impl/common/BLEClientImpl.h"
#include "impl/common/BLERemoteGattImpl.h"
#include "impl/common/BLESecurityImpl.h"

// --- Classes with a shared base: Foo::Impl must inherit FooImplCommon (two-layer model). ---
static_assert(std::is_base_of<BLEServerImplCommon, BLEServer::Impl>::value, "BLEServer::Impl must inherit BLEServerImplCommon");
static_assert(std::is_base_of<BLEServiceImplCommon, BLEService::Impl>::value, "BLEService::Impl must inherit BLEServiceImplCommon");
static_assert(
  std::is_base_of<BLECharacteristicImplCommon, BLECharacteristic::Impl>::value, "BLECharacteristic::Impl must inherit BLECharacteristicImplCommon"
);
static_assert(std::is_base_of<BLEDescriptorImplCommon, BLEDescriptor::Impl>::value, "BLEDescriptor::Impl must inherit BLEDescriptorImplCommon");
static_assert(std::is_base_of<BLEClientImplCommon, BLEClient::Impl>::value, "BLEClient::Impl must inherit BLEClientImplCommon");
static_assert(
  std::is_base_of<BLERemoteServiceImplCommon, BLERemoteService::Impl>::value, "BLERemoteService::Impl must inherit BLERemoteServiceImplCommon"
);
static_assert(
  std::is_base_of<BLERemoteCharacteristicImplCommon, BLERemoteCharacteristic::Impl>::value,
  "BLERemoteCharacteristic::Impl must inherit BLERemoteCharacteristicImplCommon"
);
static_assert(
  std::is_base_of<BLERemoteDescriptorImplCommon, BLERemoteDescriptor::Impl>::value,
  "BLERemoteDescriptor::Impl must inherit BLERemoteDescriptorImplCommon"
);
static_assert(std::is_base_of<BLESecurityImplCommon, BLESecurity::Impl>::value, "BLESecurity::Impl must inherit BLESecurityImplCommon");

// --- Fully backend-specific classes (no common base): Foo::Impl must still be defined. ---
static_assert(sizeof(BLEScan::Impl) > 0, "BLEScan::Impl must be defined by the active backend");
static_assert(sizeof(BLEAdvertising::Impl) > 0, "BLEAdvertising::Impl must be defined by the active backend");

#endif /* BLE_ENABLED */
