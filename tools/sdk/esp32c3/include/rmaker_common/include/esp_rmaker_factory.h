// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once
#include <stdint.h>
#include <esp_err.h>
#include <esp_event.h>
#ifdef __cplusplus
extern "C"
{
#endif

/** Initialize Factory NVS
 *
 * This initializes the Factory NVS partition which will store data
 * that should not be cleared even after a reset to factory.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_factory_init(void);

/** Get value from factory NVS
 *
 * This will search for the specified key in the Factory NVS partition,
 * allocate the required memory to hold it, copy the value and return
 * the pointer to it. It is responsibility of the caller to free the
 * memory when the value is no more required.
 *
 * @param[in] key The key of the value to be read from factory NVS.
 *
 * @return pointer to the value on success.
 * @return NULL on failure.
 */
void *esp_rmaker_factory_get(const char *key);

/** Set a value in factory NVS
 *
 * This will write the value for the specified key into factory NVS.
 *
 * @param[in] key The key for the value to be set in factory NVS.
 * @param[in] data Pointer to the value.
 * @param[in] len Length of the value.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_factory_set(const char *key, void *value, size_t len);
#ifdef __cplusplus
}
#endif
