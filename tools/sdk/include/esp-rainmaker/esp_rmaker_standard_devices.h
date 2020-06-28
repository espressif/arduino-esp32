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

/** Create a standard Switch device
 *
 * This creates a Switch device with the mandatory parameters and also assigns
 * the primary parameter. The default parameter names will be used.
 * Refer \ref esp_rmaker_standard_params.h for default names.
 *
 * @param[in] dev_name The unique device name
 * @param[in] cb Callback to be invoked for "write" request for device parameters.
 * Can be kept NULL if no write requests are expected
 * @param[in] priv_data (Optional) Private data that will be passed to the callback. This should stay
 * allocated throughout the lifetime of the device
 * #@param[in] power Default value of the mandatory parameter "power"
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_create_switch_device(const char *dev_name,
        esp_rmaker_param_callback_t cb, void *priv_data, bool power);

/** Create a standard Lightbulb device
 *
 * This creates a Lightbulb device with the mandatory parameters and also assigns
 * the primary parameter. The default parameter names will be used.
 * Refer \ref esp_rmaker_standard_params.h for default names.
 *
 * @param[in] dev_name The unique device name
 * @param[in] cb Callback to be invoked for "write" request for device parameters.
 * Can be kept NULL if no write requests are expected
 * @param[in] priv_data (Optional) Private data that will be passed to the callback. This should stay
 * allocated throughout the lifetime of the device
 * @param[in] power Default value of the mandatory parameter "power"
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_create_lightbulb_device(const char *dev_name,
        esp_rmaker_param_callback_t cb, void *priv_data, bool power);

/** Create a standard Fan device
 *
 * This creates a Fan device with the mandatory parameters and also assigns
 * the primary parameter. The default parameter names will be used.
 * Refer \ref esp_rmaker_standard_params.h for default names.
 *
 * @param[in] dev_name The unique device name
 * @param[in] cb Callback to be invoked for "write" request for device parameters.
 * Can be kept NULL if no write requests are expected
 * @param[in] priv_data (Optional) Private data that will be passed to the callback. This should stay
 * allocated throughout the lifetime of the device
 * @param[in] power Default value of the mandatory parameter "power"
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_create_fan_device(const char *dev_name,
        esp_rmaker_param_callback_t cb, void *priv_data, bool power);

/** Create a standard Temperature Sensor device
 *
 * This creates a Temperature Sensor device with the mandatory parameters and also assigns
 * the primary parameter. The default parameter names will be used.
 * Refer \ref esp_rmaker_standard_params.h for default names.
 *
 * @param[in] dev_name The unique device name
 * @param[in] cb Callback to be invoked for "write" request for device parameters.
 * Can be kept NULL if no write requests are expected
 * @param[in] priv_data (Optional) Private data that will be passed to the callback. This should stay
 * allocated throughout the lifetime of the device
 * @param[in] temperature Default value of the mandatory parameter "temperature"
 *
 * @return ESP_OK on success.
 * @return error in case of any error.
 */
esp_err_t esp_rmaker_create_temp_sensor_device(const char *dev_name,
        esp_rmaker_param_callback_t cb, void *priv_data, float temperature);
