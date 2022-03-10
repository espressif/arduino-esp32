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

#ifdef __cplusplus
extern "C"
{
#endif

/** Default ESP RainMaker OTA Server Certificate */
extern const char *ESP_RMAKER_OTA_DEFAULT_SERVER_CERT;

/** OTA Status to be reported to ESP RainMaker Cloud */
typedef enum {
    /** OTA is in Progress. This can be reported multiple times as the OTA progresses. */
    OTA_STATUS_IN_PROGRESS = 1,
    /** OTA Succeeded. This should be reported only once, at the end of OTA. */
    OTA_STATUS_SUCCESS,
    /** OTA Failed. This should be reported only once, at the end of OTA. */
    OTA_STATUS_FAILED,
    /** OTA was delayed by the application */
    OTA_STATUS_DELAYED,
} ota_status_t;

/** OTA Workflow type */
typedef enum {
    /** OTA will be performed using services and parameters. */
    OTA_USING_PARAMS = 1,
    /** OTA will be performed using pre-defined MQTT topics. */
    OTA_USING_TOPICS
} esp_rmaker_ota_type_t;

/** The OTA Handle to be used by the OTA callback */
typedef void *esp_rmaker_ota_handle_t;

/** OTA Data */
typedef struct {
    /** The OTA URL received from ESP RainMaker Cloud */
    char *url;
    /** Size of the OTA File. Can be 0 if the file size isn't received from
     * the ESP RainMaker Cloud */
    int filesize;
    /** The server certificate passed in esp_rmaker_enable_ota() */
    const char *server_cert;
    /** The private data passed in esp_rmaker_enable_ota() */
    char *priv;
} esp_rmaker_ota_data_t;

/** Function prototype for OTA Callback
 *
 * This function will be invoked by the ESP RainMaker core whenever an OTA is available.
 * The esp_rmaker_report_ota_status() API should be used to indicate the progress and
 * success/fail status.
 *
 * @param[in] handle An OTA handle assigned by the ESP RainMaker Core
 * @param[in] ota_data The data to be used for the OTA
 *
 * @return ESP_OK if the OTA was successful
 * @return ESP_FAIL if the OTA failed.
 */
typedef esp_err_t (*esp_rmaker_ota_cb_t) (esp_rmaker_ota_handle_t handle,
            esp_rmaker_ota_data_t *ota_data);

/** Function Prototype for Post OTA Diagnostics
 *
 * If the Application rollback feature is enabled, this callback will be invoked
 * as soon as you call esp_rmaker_ota_enable(), if it is the first
 * boot after an OTA. You may perform some application specific diagnostics and
 * report the status which will decide whether to roll back or not.
 *
 * @return true if diagnostics are successful, meaning that the new firmware is fine.
 * @return false if diagnostics fail and a roolback to previous firmware is required.
 */
typedef bool (*esp_rmaker_post_ota_diag_t)(void);

/** ESP RainMaker OTA Configuration */
typedef struct {
    /** OTA Callback.
     * The callback to be invoked when an OTA Job is available.
     * If kept NULL, the internal default callback will be used (Recommended).
     */
    esp_rmaker_ota_cb_t ota_cb;
    /** OTA Diagnostics Callback.
     * A post OTA diagnostic handler to be invoked if app rollback feature is enabled.
     * If kept NULL, the new firmware will be assumed to be fine,
     * and no rollback will be performed.
     */
    esp_rmaker_post_ota_diag_t ota_diag;
    /** Server Certificate.
     * The certificate to be passed to the OTA callback for server authentication.
     * This is mandatory, unless you have disabled it in ESP HTTPS OTA config option.
     * If you are using the ESP RainMaker OTA Service, you can just set this to
     * `ESP_RMAKER_OTA_DEFAULT_SERVER_CERT`.
     */
    const char *server_cert;
    /** Private Data.
     * Optional private data to be passed to the OTA callback.
     */
    void *priv;
} esp_rmaker_ota_config_t;

/** Enable OTA
 *
 * Calling this API enables OTA as per the ESP RainMaker specification.
 * Please check the various ESP RainMaker configuration options to
 * use the different variants of OTA. Refer the documentation for
 * additional details.
 *
 * @param[in] ota_config Pointer to an OTA configuration structure
 * @param[in] type The OTA workflow type
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_ota_enable(esp_rmaker_ota_config_t *ota_config, esp_rmaker_ota_type_t type);

/** Report OTA Status
 *
 * This API must be called from the OTA Callback to indicate the status of the OTA. The OTA_STATUS_IN_PROGRESS
 * can be reported multiple times with appropriate additional information. The final success/failure should
 * be reported only once, at the end.
 *
 * This can be ignored if you are using the default internal OTA callback.
 *
 * @param[in] ota_handle The OTA handle received by the callback
 * @param[in] status Status to be reported
 * @param[in] additional_info NULL terminated string indicating additional information for the status
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_ota_report_status(esp_rmaker_ota_handle_t ota_handle, ota_status_t status, char *additional_info);

#ifdef __cplusplus
}
#endif
