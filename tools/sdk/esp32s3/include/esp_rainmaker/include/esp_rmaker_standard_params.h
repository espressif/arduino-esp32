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

/* Suggested default names for the parameters.
 * These will also be used by default if you use any standard device helper APIs.
 *
 * @note These names are not mandatory. You can use the ESP RainMaker Core APIs
 * to create your own parameters with custom names, if required.
 */

#define ESP_RMAKER_DEF_NAME_PARAM           "Name"
#define ESP_RMAKER_DEF_POWER_NAME           "Power"
#define ESP_RMAKER_DEF_BRIGHTNESS_NAME      "Brightness"
#define ESP_RMAKER_DEF_HUE_NAME             "Hue"
#define ESP_RMAKER_DEF_SATURATION_NAME      "Saturation"
#define ESP_RMAKER_DEF_INTENSITY_NAME       "Intensity"
#define ESP_RMAKER_DEF_CCT_NAME             "CCT"
#define ESP_RMAKER_DEF_DIRECTION_NAME       "Direction"
#define ESP_RMAKER_DEF_SPEED_NAME           "Speed"
#define ESP_RMAKER_DEF_TEMPERATURE_NAME     "Temperature"
#define ESP_RMAKER_DEF_OTA_STATUS_NAME      "Status"
#define ESP_RMAKER_DEF_OTA_INFO_NAME        "Info"
#define ESP_RMAKER_DEF_OTA_URL_NAME         "URL"
#define ESP_RMAKER_DEF_TIMEZONE_NAME        "TZ"
#define ESP_RMAKER_DEF_TIMEZONE_POSIX_NAME  "TZ-POSIX"
#define ESP_RMAKER_DEF_SCHEDULE_NAME        "Schedules"
#define ESP_RMAKER_DEF_SCENES_NAME          "Scenes"
#define ESP_RMAKER_DEF_REBOOT_NAME          "Reboot"
#define ESP_RMAKER_DEF_FACTORY_RESET_NAME   "Factory-Reset"
#define ESP_RMAKER_DEF_WIFI_RESET_NAME      "Wi-Fi-Reset"
#define ESP_RMAKER_DEF_LOCAL_CONTROL_POP    "POP"
#define ESP_RMAKER_DEF_LOCAL_CONTROL_TYPE   "Type"

/**
 * Create standard name param
 *
 * This will create the standard name parameter.
 * This should be added to all devices for which you want a user customisable name.
 * The value should be same as the device name.
 *
 * All standard device creation APIs will add this internally.
 * No application registered callback will be called for this parameter,
 * and changes will be managed internally.
 *
 * @param[in] param_name Name of the parameter
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_name_param_create(const char *param_name, const char *val);

/**
 * Create standard Power param
 *
 * This will create the standard power parameter.
 *
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_power_param_create(const char *param_name, bool val);

/**
 * Create standard Brightness param
 *
 * This will create the standard brightness parameter.
 *
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_brightness_param_create(const char *param_name, int val);

/**
 * Create standard Hue param
 *
 * This will create the standard hue parameter.
 *
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_hue_param_create(const char *param_name, int val);

/**
 * Create standard Saturation param
 *
 * This will create the standard saturation parameter.
 *
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_saturation_param_create(const char *param_name, int val);

/**
 * Create standard Intensity param
 *
 * This will create the standard intensity parameter.
 *
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_intensity_param_create(const char *param_name, int val);

/**
 * Create standard CCT param
 *
 * This will create the standard cct parameter.
 *
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_cct_param_create(const char *param_name, int val);

/**
 * Create standard Direction param
 *
 * This will create the standard direction parameter.
 *
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_direction_param_create(const char *param_name, int val);

/**
 * Create standard Speed param
 *
 * This will create the standard speed parameter.
 *
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_speed_param_create(const char *param_name, int val);

/**
 * Create standard Temperature param
 *
 * This will create the standard temperature parameter.
 *
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_temperature_param_create(const char *param_name, float val);

/**
 * Create standard OTA Status param
 *
 * This will create the standard ota status parameter. Default value
 * is set internally.
 *
 * @param[in] param_name Name of the parameter
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_ota_status_param_create(const char *param_name);

/**
 * Create standard OTA Info param
 *
 * This will create the standard ota info parameter. Default value
 * is set internally.
 *
 * @param[in] param_name Name of the parameter
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_ota_info_param_create(const char *param_name);

/**
 * Create standard OTA URL param
 *
 * This will create the standard ota url parameter. Default value
 * is set internally.
 *
 * @param[in] param_name Name of the parameter
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_ota_url_param_create(const char *param_name);

/**
 * Create standard Timezone param
 *
 * This will create the standard timezone parameter.
 *
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter (Eg. "Asia/Shanghai"). Can be kept NULL.
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_timezone_param_create(const char *param_name, const char *val);

/**
 * Create standard POSIX Timezone param
 *
 * This will create the standard posix timezone parameter.
 *
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter (Eg. "CST-8"). Can be kept NULL.
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_timezone_posix_param_create(const char *param_name, const char *val);

/**
 * Create standard Schedules param
 *
 * This will create the standard schedules parameter. Default value
 * is set internally.
 *
 * @param[in] param_name Name of the parameter
 * @param[in] max_schedules Maximum number of schedules allowed
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_schedules_param_create(const char *param_name, int max_schedules);

/**
 * Create standard Scenes param
 *
 * This will create the standard scenes parameter. Default value
 * is set internally.
 *
 * @param[in] param_name Name of the parameter
 * @param[in] max_scenes Maximum number of scenes allowed
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_scenes_param_create(const char *param_name, int max_scenes);

/**
 * Create standard Reboot param
 *
 * This will create the standard reboot parameter.
 * Set value to true (via write param) for the action to trigger.
 *
 * @param[in] param_name Name of the parameter
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_reboot_param_create(const char *param_name);

/**
 * Create standard Factory Reset param
 *
 * This will create the standard factory reset parameter.
 * Set value to true (via write param) for the action to trigger.
 *
 * @param[in] param_name Name of the parameter
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_factory_reset_param_create(const char *param_name);

/**
 * Create standard Wi-Fi Reset param
 *
 * This will create the standard Wi-Fi Reset parameter.
 * Set value to true (via write param) for the action to trigger.
 *
 * @param[in] param_name Name of the parameter
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_wifi_reset_param_create(const char *param_name);

/**
 * Create standard Local Control POP param
 *
 * This will create the standard Local Control POP parameter.
 *
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter (Eg. "abcd1234"). Can be kept NULL.
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_local_control_pop_param_create(const char *param_name, const char *val);

/**
 * Create standard Local Control Type param
 *
 * This will create the standard Local Control security type parameter.
 *
 * @param[in] param_name Name of the parameter
 * @param[in] val Default Value of the parameter
 *
 * @return Parameter handle on success.
 * @return NULL in case of failures.
 */
esp_rmaker_param_t *esp_rmaker_local_control_type_param_create(const char *param_name, int val);

#ifdef __cplusplus
}
#endif
