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
#include <esp_err.h>
#include <esp_event.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @cond **/
/**
 * @brief RTC store event base
 */
ESP_EVENT_DECLARE_BASE(RTC_STORE_EVENT);
/** @endcond **/

/**
 * @brief RTC store events
 */
typedef enum {
    RTC_STORE_EVENT_CRITICAL_DATA_LOW_MEM,        /*!< Critical data configured threshold crossed */
    RTC_STORE_EVENT_CRITICAL_DATA_WRITE_FAIL,     /*!< Critical data write failed */
    RTC_STORE_EVENT_NON_CRITICAL_DATA_LOW_MEM,    /*!< Non critical data configured threshold crossed */
    RTC_STORE_EVENT_NON_CRITICAL_DATA_WRITE_FAIL, /*!< Non critical data write failed */
} rtc_store_event_t;

/**
 * @brief Non critical data header
 */
typedef struct {
    const char *dg;     /*!< Data group of non critical data eg: heap, wifi, ip */
    uint32_t len;       /*!< Length of data */
} rtc_store_non_critical_data_hdr_t;

/**
 * @brief Write critical data to the RTC storage
 *
 * @param[in] data Pointer to the data
 * @param[in] len Length of data
 *
 * @return ESP_OK on success, appropriate error code otherwise.
 */
esp_err_t rtc_store_critical_data_write(void *data, size_t len);

/**
 * @brief Read critical data from the RTC storage
 *
 * @param[out] size Number of bytes read
 *
 * @return Pointer to the data on success, otherwise NULL
 *
 * @note It is mandatory to call \ref rtc_store_critical_data_release_and_unlock() if \ref rtc_store_critical_data_read_and_lock() is successful.
 * @note Please avoid adding \ref ESP_DIAG_EVENT(), error/warning logs using esp_log module in between rtc_store_critical_data_read_and_lock() and rtc_store_critical_data_release_and_unlock() API calls. It may lead to a deadlock.
 */
const void *rtc_store_critical_data_read_and_lock(size_t *size);

/**
 * @brief Release the utilized data read using \ref rtc_store_critical_data_read_and_lock()
 *
 * This API releases the utilized data read using \ref rtc_store_critical_data_read_and_lock().
 * Utilization may involve encoding data, sending data to the cloud, post processing or printing on the console, etc.
 *
 * @param[in] size Number of bytes to free. If all the data is utilized then pass the size returned by \ref rtc_store_critical_data_read_and_lock() or 0 if no data is utilized (e.g. sending to cloud failed).
 *
 * @return ESP_OK on success, appropriate error code otherwise.
 *
 * @note Please avoid adding \ref ESP_DIAG_EVENT(), error/warning logs using esp_log module in between rtc_store_critical_data_read_and_lock() and rtc_store_critical_data_release_and_unlock() API calls. It may lead to a deadlock.
 */
esp_err_t rtc_store_critical_data_release_and_unlock(size_t size);

/**
 * @brief Release the size bytes critical data from RTC storage
 *
 * This API can be used to remove data from buffer when data is sent asynchronously.
 *
 * Consider data is read using \ref rtc_store_critical_data_read_and_lock() and sent to cloud asynchronously.
 * Since status of data send is unknown, call \ref rtc_store_critical_data_release_and_unlock() with zero length.
 * When acknowledgement for data send is received use this API with appropriate size to remove the data from the buffer.
 *
 * @param[in] size Number of bytes to free.
 *
 * @return ESP_OK on success, appropriate error code otherwise.
 */
esp_err_t rtc_store_critical_data_release(size_t size);

/**
 * @brief Write non critical data to the RTC storage
 *
 * This API overwrites the data if non critical storage is full
 *
 * @param[in] dg Data group of data eg: heap, wifi, ip(Must be the string stored in RODATA)
 * @param[in] data Pointer to non critical data
 * @param[in] len Length of non critical data
 *
 * @return ESP_OK on success, appropriate error code otherwise.
 *
 * @note Data is stored in Type-Length-Value format
 *       Type(Data group)  - 4 byte      - Pointer to the string in rodata
 *       Length            - 4 byte      - Length of data
 *       Value             - Length byte - Data
 */
esp_err_t rtc_store_non_critical_data_write(const char *dg, void *data, size_t len);

/**
 * @brief Read non critical data from the RTC storage
 *
 * @param[out] size Number of bytes read
 *
 * @return Pointer to the data on success, otherwise NULL
 *
 * @note It is mandatory to call \ref rtc_store_non_critical_data_release_and_unlock() if \ref rtc_store_non_critical_data_read_and_lock() is successful.
 */
const void *rtc_store_non_critical_data_read_and_lock(size_t *size);

/**
 * @brief Release the utilized data read using \ref rtc_store_non_critical_data_read_and_lock()
 *
 * This API releases the utilized data read using \ref rtc_store_non_critical_data_read_and_lock().
 * Utilization may involve encoding data, sending data to the cloud, post processing or printing on the console, etc.
 *
 * @param[in] size Number of bytes to free. If all the data is utilized then pass the size returned by \ref rtc_store_non_critical_data_read_and_lock() or 0 if no data is utilized (e.g. sending to cloud failed).
 *
 * @return ESP_OK on success, appropriate error code otherwise.
 *
 */
esp_err_t rtc_store_non_critical_data_release_and_unlock(size_t size);

/**
 * @brief Release the size bytes non critical data from RTC storage
 *
 * @param[in] size Number of bytes to free.
 *
 * @return ESP_OK on success, appropriate error code otherwise.
 */
esp_err_t rtc_store_non_critical_data_release(size_t size);

/**
 * @brief Initializes the RTC storage
 *
 * @return ESP_OK on success, appropriate error code otherwise
 */
esp_err_t rtc_store_init(void);

/**
 * @brief Deinitializes the RTC storage
 */
void rtc_store_deinit(void);

#ifdef __cplusplus
}
#endif
