// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
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

#ifdef __cplusplus
extern "C"
{
#endif

#if CONFIG_DIAG_ENABLE_HEAP_METRICS

/**
 * @brief Initialize the heap metrics
 *
 * Free heap, largest free block, and all time minimum free heap values are collected periodically.
 * Parameters are collected for RAM in internal memory and external memory (if device has PSRAM).
 *
 * Default periodic interval is 30 seconds and can be changed with esp_diag_heap_metrics_reset_interval().
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_heap_metrics_init(void);

/**
 * @brief Deinitialize the heap metrics
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_heap_metrics_deinit(void);

/**
 * @brief Reset the periodic interval
 *
 * By default, heap metrics are collected every 30 seconds, this function can be used to change the interval.
 * If the interval is set to 0, heap metrics collection disabled.
 *
 * @param[in] period Period interval in seconds
 */
void esp_diag_heap_metrics_reset_interval(uint32_t period);

/**
 * @brief Dumps the heap metrics and prints them to the console.
 *
 * This API collects and reports metrics value at any give point in time.
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_heap_metrics_dump(void);

#endif /* CONFIG_DIAG_ENABLE_HEAP_METRICS */

#if CONFIG_DIAG_ENABLE_WIFI_METRICS

/**
 * @brief Initialize the wifi metrics
 *
 * Wi-Fi RSSI and minimum ever Wi-Fi RSSI values are collected periodically.
 * Default periodic interval is 30 seconds and can be changed with esp_diag_wifi_metrics_reset_interval().
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_wifi_metrics_init(void);

/**
 * @brief Deinitialize the wifi metrics
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_wifi_metrics_deinit(void);

/**
 * @brief Dumps the wifi metrics and prints them to the console.
 *
 * This API can be used to collect wifi metrics at any given point in time.
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_wifi_metrics_dump(void);

/**
 * @brief Reset the periodic interval
 *
 * By default, wifi metrics are collected every 30 seconds, this function can be used to change the interval.
 * If the interval is set to 0, wifi metrics collection disabled.
 *
 * @param[in] period Period interval in seconds
 */
void esp_diag_wifi_metrics_reset_interval(uint32_t period);

#endif /* CONFIG_DIAG_ENABLE_WIFI_METRICS */

#ifdef __cplusplus
}
#endif
