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
 * @param[in] priv_data (Optional) Private data associated with the service. This should stay
 * allocated throughout the lifetime of the service.
 *
 * @return service_handle on success.
 * @return NULL in case of any error.
 */
esp_rmaker_device_t *esp_rmaker_ota_service_create(const char *serv_name, void *priv_data);

/** Create a standard OTA service
 *
 * This creates an OTA service with the mandatory parameters. The default parameter names will be used.
 * Refer \ref esp_rmaker_standard_params.h for default names.
 *
 * @param[in] serv_name The unique service name
 * @param[in] timezone Default value of timezone string (Eg. "Asia/Shanghai"). Can be kept NULL.
 * @param[in] timezone_posix Default value of posix timezone string (Eg. "CST-8"). Can be kept NULL.
 * @param[in] priv_data (Optional) Private data associated with the service. This should stay
 * allocated throughout the lifetime of the service.
 *
 * @return service_handle on success.
 * @return NULL in case of any error.
 */
esp_rmaker_device_t *esp_rmaker_time_service_create(const char *serv_name, const char *timezone,
        const char *timezone_posix, void *priv_data);

/** Create a standard Schedule service
 *
 * This creates a Schedule service with the mandatory parameters. The default parameter names will be used.
 * Refer \ref esp_rmaker_standard_params.h for default names.
 *
 * @param[in] serv_name The unique service name
 * @param[in] write_cb Write callback.
 * @param[in] read_cb Read callback.
 * @param[in] max_schedules Maximum number of schedules supported.
 * @param[in] priv_data (Optional) Private data associated with the service. This should stay
 * allocated throughout the lifetime of the service.
 *
 * @return service_handle on success.
 * @return NULL in case of any error.
 */
esp_rmaker_device_t *esp_rmaker_create_schedule_service(const char *serv_name, esp_rmaker_device_write_cb_t write_cb, esp_rmaker_device_read_cb_t read_cb, int max_schedules, void *priv_data);

#ifdef __cplusplus
}
#endif
