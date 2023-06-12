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

#define SHA_SIZE  (CONFIG_APP_RETRIEVE_LEN_ELF_SHA / 2)

/**
 * @brief header record to identify firmware/boot data a record represent
 */
typedef struct {
    uint8_t gen_id;             // generated on each hard reset
    uint8_t boot_cnt;           // updated on each soft reboot
    char sha_sum[SHA_SIZE];     // elf shasum
    bool valid;                 //
} rtc_store_meta_header_t;

/**
 * @brief   get meta header for idx
 *
 * @param idx   idx of meta from records
 * @return rtc_store_meta_header_t*
 */
rtc_store_meta_header_t *rtc_store_get_meta_record_by_index(uint8_t idx);

/**
 * @brief   get current meta header
 *
 * @return rtc_store_meta_header_t*
 */
rtc_store_meta_header_t *rtc_store_get_meta_record_current();

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
 * @param[in] buf Buffer to read data in
 * @param[in] size Number of bytes to read
 *
 * @return Number of bytes read or -1 on error
 */
int rtc_store_critical_data_read(uint8_t *buf, size_t size);

/**
 * @brief Release the size bytes critical data from RTC storage
 *
 * This API can be used to remove data from buffer when data is sent asynchronously.
 *
 * @param[in] size Number of bytes to free.
 *
 * @return ESP_OK on success, appropriate error code otherwise.
 */
esp_err_t rtc_store_critical_data_release(size_t size);

/**
 * @brief Read critical data from the RTC storage and release that data
 *
 * @param[in] buf Buffer to read data in
 * @param[in] size Number of bytes to read
 *
 * @return Number of bytes read or -1 on error
 */
int rtc_store_critical_data_read_and_release(uint8_t *buf, size_t size);

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
 * @param[in] buf Buffer to read data in
 * @param[in] size Number of bytes read
 *
 * @return Number of bytes read or -1 on error
 */
int rtc_store_non_critical_data_read(uint8_t *buf, size_t size);

/**
 * @brief Release the size bytes non critical data from RTC storage
 *
 * @param[in] size Number of bytes to free.
 *
 * @return ESP_OK on success, appropriate error code otherwise.
 */
esp_err_t rtc_store_non_critical_data_release(size_t size);

/**
 * @brief Read non_critical data from the RTC storage and release that data
 *
 * @param[in] buf Buffer to read data in
 * @param[in] size Number of bytes read
 *
 * @return Number of bytes read or -1 on error
 */
int rtc_store_non_critical_data_read_and_release(uint8_t *buf, size_t size);

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

/**
 * @brief Get CRC of RTC Store configuration
 *
 * @return crc
 */
uint32_t rtc_store_get_crc(void);

/**
 * @brief Discard values from RTC Store. This API should be called after rtc_store_init();
 *
 * @return ESP_OK on success, appropriate error on failure.
 */
esp_err_t rtc_store_discard_data(void);

#ifdef __cplusplus
}
#endif
