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

#include "core/BLEGuards.h"
#if BLE_ENABLED

/**
 * @brief Task predicates for the BLE host/stack callback context.
 *
 * Namespaced (not bare `ble*` globals) like every other internal helper.
 */
namespace BLETask {

/**
 * @brief True when the caller is running on the BLE host/BTC callback task.
 *
 * Blocking native teardown (service removal, disconnect wait) must not run
 * here; defer it to an application or worker task.
 */
bool inStackCallbackTask();

/** @brief Backend-specific check for the native host callback task (NimBLE host / Bluedroid BTC). */
bool isHostStackTask();

}  // namespace BLETask

#endif /* BLE_ENABLED */
