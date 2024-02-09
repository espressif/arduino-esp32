/*
 * SPDX-FileCopyrightText: 2020-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @cond **/
/** ESP RainMaker Event Base */
ESP_EVENT_DECLARE_BASE(RMAKER_OTA_EVENT);
/** @endcond **/

/** ESP RainMaker Events */
typedef enum {
    /* Invalid event. Used for internal handling only */
    RMAKER_OTA_EVENT_INVALID = 0,
    /** RainMaker OTA is Starting */
    RMAKER_OTA_EVENT_STARTING,
    /** RainMaker OTA has Started */
    RMAKER_OTA_EVENT_IN_PROGRESS,
    /** RainMaker OTA Successful */
    RMAKER_OTA_EVENT_SUCCESSFUL,
    /** RainMaker OTA Failed */
    RMAKER_OTA_EVENT_FAILED,
    /** RainMaker OTA Rejected */
    RMAKER_OTA_EVENT_REJECTED,
    /** RainMaker OTA Delayed */
    RMAKER_OTA_EVENT_DELAYED,
    /** OTA Image has been flashed and active partition changed. Reboot is requested. Applicable only if Auto reboot is disabled **/
    RMAKER_OTA_EVENT_REQ_FOR_REBOOT,
} esp_rmaker_ota_event_t;

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
    /** OTA rejected due to some reason (wrong project, version, etc.) */
    OTA_STATUS_REJECTED,
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
    /** The firmware version of the OTA image **/
    char *fw_version;
    /** The OTA Job ID received from cloud **/
    char *ota_job_id;
    /** The server certificate passed in esp_rmaker_enable_ota() */
    const char *server_cert;
    /** The private data passed in esp_rmaker_enable_ota() */
    char *priv;
    /** OTA Metadata. Applicable only for OTA using Topics. Will be received (if applicable) from the backend, along with the OTA URL */
    char *metadata;
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

typedef enum {
    /** OTA Diagnostics Failed. Rollback the firmware. */
    OTA_DIAG_STATUS_FAIL,
    /** OTA Diagnostics Pending. Additional validations will be done later. */
    OTA_DIAG_STATUS_PENDING,
    /** OTA Diagnostics Succeeded. Firmware can be considered valid. */
    OTA_DIAG_STATUS_SUCCESS
} esp_rmaker_ota_diag_status_t;

typedef enum {
    /** OTA State: Initialised. */
    OTA_DIAG_STATE_INIT,
    /** OTA state: MQTT has connected. */
    OTA_DIAG_STATE_POST_MQTT
} esp_rmaker_ota_diag_state_t;

typedef struct {
    /** OTA diagnostic state */
    esp_rmaker_ota_diag_state_t state;
    /** Flag to indicate whether the OTA which has triggered the Diagnostics checks for rollback
     * was triggered via RainMaker or not. This would be useful only when your application has some
     * other mechanism for OTA too.
     */
    bool rmaker_ota;
} esp_rmaker_ota_diag_priv_t;

/** Function Prototype for Post OTA Diagnostics
 *
 * If the Application rollback feature is enabled, this callback will be invoked
 * as soon as you call esp_rmaker_ota_enable(), if it is the first
 * boot after an OTA. You may perform some application specific diagnostics and
 * report the status which will decide whether to roll back or not.
 *
 * This will be invoked once again after MQTT has connected, in case some additional validations
 * are to be done later.
 *
 * If OTA state == OTA_DIAG_STATE_INIT, then
 *      return OTA_DIAG_STATUS_FAIL to indicate failure and rollback.
 *      return OTA_DIAG_STATUS_SUCCESS or OTA_DIAG_STATUS_PENDING to tell internal OTA logic to continue further.
 *
 * If OTA state == OTA_DIAG_STATE_POST_MQTT, then
 *      return OTA_DIAG_STATUS_FAIL to indicate failure and rollback.
 *      return OTA_DIAG_STATUS_SUCCESS to indicate validation was successful and mark OTA as valid
 *      return OTA_DIAG_STATUS_PENDING to indicate that some additional validations will be done later
 *      and the OTA will eventually be marked valid/invalid using esp_rmaker_ota_mark_valid() or
 *      esp_rmaker_ota_mark_invalid() respectively.
 *
 * @return esp_rmaker_ota_diag_status_t as applicable
 */
typedef esp_rmaker_ota_diag_status_t (*esp_rmaker_post_ota_diag_t)(esp_rmaker_ota_diag_priv_t *ota_diag_priv, void *priv);

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

/** Default OTA callback
 *
 * This is the default OTA callback which will get used if you do not pass your own callback. You can call this
 * even from your callback, in case you want better control on when the OTA can proceed and yet let the actual
 * OTA process be managed by the RainMaker Core.
 *
 * @param[in] handle An OTA handle assigned by the ESP RainMaker Core
 * @param[in] ota_data The data to be used for the OTA
 *
 * @return ESP_OK if the OTA was successful
 * @return ESP_FAIL if the OTA failed.
 * */
esp_err_t esp_rmaker_ota_default_cb(esp_rmaker_ota_handle_t handle, esp_rmaker_ota_data_t *ota_data);

/** Fetch OTA Info
 *
 * For OTA using Topics, this API can be used to explicitly ask the backend if an OTA is available.
 * If it is, then the OTA callback would get invoked.
 *
 * @return ESP_OK if the OTA fetch publish message was successful.
 * @return error on failure
 */
esp_err_t esp_rmaker_ota_fetch(void);

/** Fetch OTA Info with a delay
 *
 * For OTA using Topics, this API can be used to explicitly ask the backend if an OTA is available
 * after a delay (in seconds) passed as an argument.
 *
 * @param[in] time Delay (in seconds)
 *
 * @return ESP_OK if the OTA fetch timer was created.
 * @return error on failure
 */
esp_err_t esp_rmaker_ota_fetch_with_delay(int time);

/** Mark OTA as valid
 *
 * This should be called if the OTA validation has been kept pending by returning OTA_DIAG_STATUS_PENDING
 * in the ota_diag callback and then, the validation was eventually successful. This can also be used to mark
 * the OTA valid even before RainMaker core does its own validations (primarily MQTT connection).
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_ota_mark_valid(void);

/** Mark OTA as invalid
 *
 * This should be called if the OTA validation has been kept pending by returning OTA_DIAG_STATUS_PENDING
 * in the ota_diag callback and then, the validation eventually failed. This can even be used to rollback
 * at any point of time before RainMaker core's internal logic and the application's logic mark the OTA
 * as valid.
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_ota_mark_invalid(void);
#ifdef __cplusplus
}
#endif
