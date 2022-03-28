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

/** Create a standard Switch device
 *
 * This creates a Switch device with the mandatory parameters and also assigns
 * the primary parameter. The default parameter names will be used.
 * Refer \ref esp_rmaker_standard_params.h for default names.
 *
 * @param[in] dev_name The unique device name
 * @param[in] priv_data (Optional) Private data associated with the device. This should stay
 * allocated throughout the lifetime of the device
 * #@param[in] power Default value of the mandatory parameter "power"
 *
 * @return Device handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_device_t *esp_rmaker_switch_device_create(const char *dev_name,
            void *priv_data, bool power);

/** Create a standard Lightbulb device
 *
 * This creates a Lightbulb device with the mandatory parameters and also assigns
 * the primary parameter. The default parameter names will be used.
 * Refer \ref esp_rmaker_standard_params.h for default names.
 *
 * @param[in] dev_name The unique device name
 * @param[in] priv_data (Optional) Private data associated with the device. This should stay
 * allocated throughout the lifetime of the device
 * @param[in] power Default value of the mandatory parameter "power"
 *
 * @return Device handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_device_t *esp_rmaker_lightbulb_device_create(const char *dev_name,
            void *priv_data, bool power);

/** Create a standard Fan device
 *
 * This creates a Fan device with the mandatory parameters and also assigns
 * the primary parameter. The default parameter names will be used.
 * Refer \ref esp_rmaker_standard_params.h for default names.
 *
 * @param[in] dev_name The unique device name
 * @param[in] priv_data (Optional) Private data associated with the device. This should stay
 * allocated throughout the lifetime of the device
 * @param[in] power Default value of the mandatory parameter "power"
 *
 * @return Device handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_device_t *esp_rmaker_fan_device_create(const char *dev_name,
            void *priv_data, bool power);

/** Create a standard Temperature Sensor device
 *
 * This creates a Temperature Sensor device with the mandatory parameters and also assigns
 * the primary parameter. The default parameter names will be used.
 * Refer \ref esp_rmaker_standard_params.h for default names.
 *
 * @param[in] dev_name The unique device name
 * @param[in] priv_data (Optional) Private data associated with the device. This should stay
 * allocated throughout the lifetime of the device
 * @param[in] temperature Default value of the mandatory parameter "temperature"
 *
 * @return Device handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_device_t *esp_rmaker_temp_sensor_device_create(const char *dev_name,
            void *priv_data, float temperature);

#ifdef __cplusplus
}
#endif
