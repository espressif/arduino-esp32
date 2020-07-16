// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
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
#include <stdbool.h>
#include <esp_err.h>
#include <esp_rmaker_core.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Create a standard OTA service
 *
 * This creates an OTA service with the mandatory parameters. The default parameter names will be used.
 * Refer \ref esp_rmaker_standard_params.h for default names.
 *
 * @param[in] serv_name The unique service name
 * @param[in] cb Callback to be invoked for "write" request for service parameters.
 * Can be kept NULL if no write requests are expected
 * @param[in] priv_data (Optional) Private data that will be passed to the callback. This should stay
 * allocated throughout the lifetime of the service.
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_create_ota_service(const char *serv_name,
        esp_rmaker_param_callback_t cb, void *priv_data);

#ifdef __cplusplus
}
#endif
