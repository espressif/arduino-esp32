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
#include <sntp.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct esp_rmaker_time_config {
    /** If not specified, then 'CONFIG_ESP_RMAKER_SNTP_SERVER_NAME' is used as the SNTP server. */
    char *sntp_server_name;
    /** Optional callback to invoke, whenever time is synchronised. This will be called
     * periodically as per the SNTP polling interval (which is 60min by default).
     * If kept NULL, the default callback will be invoked, which will just print the
     * current local time.
     */
    sntp_sync_time_cb_t sync_time_cb;
} esp_rmaker_time_config_t;

/** Reboot the chip after a delay
 *
 * This API just starts a reboot timer and returns immediately.
 * The actual reboot is trigerred asynchronously in the timer callback.
 * This is useful if you want to reboot after a delay, to allow other tasks to finish
 * their operations (Eg. MQTT publish to indicate OTA success). The \ref RMAKER_EVENT_REBOOT
 * event is triggered when the reboot timer is started.
 *
 * @param[in] seconds Time in seconds after which the chip should reboot.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_reboot(uint8_t seconds);

/** Reset Wi-Fi credentials and reboot
 *
 * This will reset just the Wi-Fi credentials and trigger a reboot.
 * This is useful when you want to keep all the entries in NVS memory
 * intact, but just change the Wi-Fi credentials. The \ref RMAKER_EVENT_WIFI_RESET
 * event is triggered after the reset.
 *
 * @note This function internally calls esp_rmaker_reboot() and returns
 * immediately. The reboot happens asynchronously.
 *
 * @param[in] seconds Time in seconds after which the chip should reboot.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_wifi_reset(uint8_t seconds);

/** Reset to factory defaults and reboot
 *
 * This will clear entire NVS partition and trigger a reboot.
 * The \ref RMAKER_EVENT_FACTORY_RESET event is triggered after the reset.
 *
 * @note This function internally calls esp_rmaker_reboot() and returns.
 * The reboot happens asynchronously.
 *
 * @param[in] seconds Time in seconds after which the chip should reboot.
 *
 * @return ESP_OK on success.
 * @return error on failure.
 */
esp_err_t esp_rmaker_factory_reset(uint8_t seconds);

/** Initialize time synchronization
 *
 * This API initializes SNTP for time synchronization.
 *
 * @param[in] config Configuration to be used for SNTP time synchronization. The default configuration is used if NULL is passed.
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_time_sync_init(esp_rmaker_time_config_t *config);

/** Check if current time is updated
 *
 * This API checks if the current system time is updated against the reference time of 1-Jan-2019.
 *
 * @return true if time is updated
 * @return false if time is not updated
 */
bool esp_rmaker_time_check(void);

/** Wait for time synchronization
 *
 * This API waits for the system time to be updated against the reference time of 1-Jan-2019.
 * This is a blocking call.
 *
 * @param[in] ticks_to_wait Number of ticks to wait for time synchronization. Accepted values: 0 to portMAX_DELAY.
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_time_wait_for_sync(uint32_t ticks_to_wait);

/** Set POSIX timezone
 *
 * Set the timezone (TZ environment variable) as per the POSIX format
 * specified in the [GNU libc documentation](https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html).
 * Eg. For China: "CST-8"
 *     For US Pacific Time (including daylight saving information): "PST8PDT,M3.2.0,M11.1.0"
 *
 * @param[in] tz_posix NULL terminated TZ POSIX string
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_time_set_timezone_posix(const char *tz_posix);

/** Set timezone location string
 *
 * Set the timezone as a user friendly location string.
 * Check [here](https://rainmaker.espressif.com/docs/time-service.html) for a list of valid values.
 *
 * Eg. For China: "Asia/Shanghai"
 *     For US Pacific Time: "America/Los_Angeles"
 *
 * @note Setting timezone using this API internally also sets the POSIX timezone string.
 *
 * @param[in] tz NULL terminated Timezone location string
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_time_set_timezone(const char *tz);

/** Enable Timezone Service
 *
 * This enables the ESP RainMaker standard timezone service which can be used to set
 * timezone, either in POSIX or location string format. Please refer the specifications
 * for additional details.
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_timezone_service_enable(void);

/** Get printable local time string
 *
 * Get a printable local time string, with information of timezone and Daylight Saving.
 * Eg. "Tue Sep  1 09:04:38 2020 -0400[EDT], DST: Yes"
 * "Tue Sep  1 21:04:04 2020 +0800[CST], DST: No"
 *
 *
 * @param[out] buf Pointer to a pre-allocated buffer into which the time string will
 *                  be populated.
 * @param[in] buf_len Length of the above buffer.
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_get_local_time_str(char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif
