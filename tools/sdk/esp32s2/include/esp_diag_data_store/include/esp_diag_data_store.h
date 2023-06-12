/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once
#include <esp_err.h>
#include <esp_event.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Data store event base
 */
ESP_EVENT_DECLARE_BASE(ESP_DIAG_DATA_STORE_EVENT);

/**
 * @brief Data store events
 *
 * Diagnostics data store emits following events using default event loop,
 * every event has event data of type \ref esp_diag_data_store_event_data_t
 */
typedef enum {
    ESP_DIAG_DATA_STORE_EVENT_CRITICAL_DATA_WRITE_FAIL,
    ESP_DIAG_DATA_STORE_EVENT_NON_CRITICAL_DATA_WRITE_FAIL,
    ESP_DIAG_DATA_STORE_EVENT_CRITICAL_DATA_LOW_MEM,
    ESP_DIAG_DATA_STORE_EVENT_NON_CRITICAL_DATA_LOW_MEM,
} esp_diag_data_store_events_t;

/**
 * @brief Write critical data to the diagnostics data store
 *
 * @param[in] data Buffer holding the data
 * @param[in] len length of the data to be written
 *
 * @return ESP_OK on success, appropriate error code otherwise.
 */
esp_err_t esp_diag_data_store_critical_write(void *data, size_t len);

/**
 * @brief Write non_critical data to the diagnostics data store
 *
 * @param[in] dg Data group of the data
 * @param[in] data Buffer holding the data
 * @param[in] len length of the data to be written
 *
 * @return ESP_OK on success, appropriate error code otherwise.
 */
esp_err_t esp_diag_data_store_non_critical_write(const char *dg, void *data, size_t len);

/**
 * @brief Read critical data from the diagnostics data store
 *
 * @param[in]  buf buffer to hold the data
 * @param[out] size Number of bytes read
 *
 * @return int bytes > 0 on success. Appropriate error otherwise
 */
int esp_diag_data_store_critical_read(uint8_t *buf, size_t size);

/**
 * @brief Read non_critical data from the diagnostics data store
 *
 * @param[in]  buf buffer to hold the data
 * @param[out] size Number of bytes read
 *
 * @return int bytes > 0 on success. Appropriate error otherwise
 */
int esp_diag_data_store_non_critical_read(uint8_t *buf, size_t size);

/**
 * @brief Release the size bytes of critical data from diagnostics data store
 *
 * This API can be used to remove data from buffer when data is sent asynchronously.
 *
 * @param[in] size Number of bytes to free.
 *
 * @return ESP_OK on success, appropriate error code otherwise.
 */
esp_err_t esp_diag_data_store_critical_release(size_t size);

/**
 * @brief Release the size bytes of non_critical data from diagnostics data store
 *
 * This API can be used to remove data from buffer when data is sent asynchronously.
 *
 * @param[in] size Number of bytes to free.
 *
 * @return ESP_OK on success, appropriate error code otherwise.
 */
esp_err_t esp_diag_data_store_non_critical_release(size_t size);

/**
 * @brief Initializes the diagnostics data store
 *
 * @return ESP_OK on success, appropriate error code otherwise
 */
esp_err_t esp_diag_data_store_init(void);

/**
 * @brief Deinitializes the diagnostics data store
 */
void esp_diag_data_store_deinit(void);

/**
 * @brief Get CRC of diagnostics data store configuration
 *
 * @return crc
 */
uint32_t esp_diag_data_store_get_crc(void);

/**
 * @brief Discard values from diagnostics data store. This API should be called after esp_diag_data_store_init();
 *
 * @return ESP_OK on success, appropriate error on failure.
 */
esp_err_t esp_diag_data_discard_data(void);
#ifdef __cplusplus
}
#endif
