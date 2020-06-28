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

/* Suggested default names for the parameters.
 * These will also be used by default if you use any standard device helper APIs.
 *
 * @note These names are not mandatory. You can use the ESP RainMaker Core APIs
 * to create your own parameters with custom names, if required.
 */
#define ESP_RMAKER_DEF_NAME_PARAM           "name"
#define ESP_RMAKER_DEF_POWER_NAME           "power"
#define ESP_RMAKER_DEF_BRIGHTNESS_NAME      "brightness"
#define ESP_RMAKER_DEF_HUE_NAME             "hue"
#define ESP_RMAKER_DEF_SATURATION_NAME      "saturation"
#define ESP_RMAKER_DEF_INTENSITY_NAME       "intensity"
#define ESP_RMAKER_DEF_CCT_NAME             "cct"
#define ESP_RMAKER_DEF_DIRECTION_NAME       "direction"
#define ESP_RMAKER_DEF_SPEED_NAME           "speed"
#define ESP_RMAKER_DEF_TEMPERATURE_NAME     "temperature"
#define ESP_RMAKER_DEF_OTA_STATUS_NAME      "status"
#define ESP_RMAKER_DEF_OTA_INFO_NAME        "info"
#define ESP_RMAKER_DEF_OTA_URL_NAME         "url"

/**
 * Add name param to a Device
 *
 * This will add the standard name parameter to a device.
 * This should be added to all devices for which you want a user customisable name.
 * Default value will automatically be set to the device name.
 * All standard device creation APIs will add this internally.
 * No application registered callback will be called for this parameter,
 * and changes will be managed internally.
 *
 * @param[in] dev_name Name of the device
 * @param[in] param_name Name of the parameter
 *
 * @return ESP_OK if the parameter was added successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_device_add_name_param(const char *dev_name, const char *param_name);

/**
 * Add Power param to a Device
 *
 * This will add the standard power parameter to a device.
 *
 * @param[in] dev_name Name of the device
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return ESP_OK if the parameter was added successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_device_add_power_param(const char *dev_name, const char *param_name, bool val);

/**
 * Add Brightness param to a Device
 *
 * This will add the standard brightness parameter to a device.
 *
 * @param[in] dev_name Name of the device
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return ESP_OK if the parameter was added successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_device_add_brightness_param(const char *dev_name, const char *param_name, int val);

/**
 * Add Hue param to a Device
 *
 * This will add the standard hue parameter to a device.
 *
 * @param[in] dev_name Name of the device
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return ESP_OK if the parameter was added successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_device_add_hue_param(const char *dev_name, const char *param_name, int val);

/**
 * Add Saturation param to a Device
 *
 * This will add the standard saturation parameter to a device.
 *
 * @param[in] dev_name Name of the device
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return ESP_OK if the parameter was added successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_device_add_saturation_param(const char *dev_name, const char *param_name, int val);

/**
 * Add Intensity param to a Device
 *
 * This will add the standard intensity parameter to a device.
 *
 * @param[in] dev_name Name of the device
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return ESP_OK if the parameter was added successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_device_add_intensity_param(const char *dev_name, const char *param_name, int val);

/**
 * Add CCT param to a Device
 *
 * This will add the standard cct parameter to a device.
 *
 * @param[in] dev_name Name of the device
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return ESP_OK if the parameter was added successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_device_add_cct_param(const char *dev_name, const char *param_name, int val);

/**
 * Add Direction param to a Device
 *
 * This will add the standard direction parameter to a device.
 *
 * @param[in] dev_name Name of the device
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return ESP_OK if the parameter was added successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_device_add_direction_param(const char *dev_name, const char *param_name, int val);

/**
 * Add Speed param to a Device
 *
 * This will add the standard speed parameter to a device.
 *
 * @param[in] dev_name Name of the device
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return ESP_OK if the parameter was added successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_device_add_speed_param(const char *dev_name, const char *param_name, int val);

/**
 * Add Temperature param to a Device
 *
 * This will add the standard temperature parameter to a device.
 *
 * @param[in] dev_name Name of the device
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return ESP_OK if the parameter was added successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_device_add_temperature_param(const char *dev_name, const char *param_name, float val);

/**
 * Add OTA Status param to a Service
 *
 * This will add the standard ota status parameter to a service. Default value
 * is set internally.
 *
 * @param[in] serv_name Name of the service
 * @param[in] param_name Name of the parameter
 *
 * @return ESP_OK if the parameter was added successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_service_add_ota_status_param(const char *serv_name, const char *param_name);

/**
 * Add OTA Info param to a Service
 *
 * This will add the standard ota info parameter to a service. Default value
 * is set internally.
 *
 * @param[in] serv_name Name of the service
 * @param[in] param_name Name of the parameter
 *
 * @return ESP_OK if the parameter was added successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_service_add_ota_info_param(const char *serv_name, const char *param_name);

/**
 * Add OTA URL param to a Service
 *
 * This will add the standard ota url parameter to a service. Default value
 * is set internally.
 *
 * @param[in] serv_name Name of the service
 * @param[in] param_name Name of the parameter
 *
 * @return ESP_OK if the parameter was added successfully.
 * @return error in case of failures.
 */
esp_err_t esp_rmaker_service_add_ota_url_param(const char *serv_name, const char *param_name);
