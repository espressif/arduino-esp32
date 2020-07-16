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
#include <esp_err.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct esp_rmaker_time_config {
    /** If not specified, then 'CONFIG_ESP_RMAKER_SNTP_SERVER_NAME' is used as the SNTP server. */
    char *sntp_server_name;
} esp_rmaker_time_config_t;

/** Reboot the chip after a delay
 *
 * This API just starts an esp_timer and executes a reboot from that.
 * Useful if you want to reboot after a delay, to allow other tasks to finish
 * their operations (Eg. MQTT publish to indicate OTA success)
 *
 * @param[in] seconds Time in seconds after which the chip should reboot
 *
 * @return ESP_OK on success
 * @return error on failure
 */
esp_err_t esp_rmaker_reboot(uint8_t seconds);

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

#ifdef __cplusplus
}
#endif
