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
#include <stdbool.h>
#include <esp_err.h>
#include <esp_diagnostics.h>

#ifdef __cplusplus
extern "C"
{
#endif

#if CONFIG_DIAG_ENABLE_VARIABLES

/**
 * @brief Callback to write variable's data
 *
 * @param[in] tag   Tag for variable
 * @param[in] data  Data for variable
 * @param[in] len   Length of variable
 * @param[in] cb_arg User data to pass in write callback
 */
typedef esp_err_t (*esp_diag_variable_write_cb_t)(const char *tag, void *data, size_t len, void *cb_arg);

/**
 * @brief Diagnostics variable config structure
 */
typedef struct {
    esp_diag_variable_write_cb_t write_cb; /*!< Callback function to write diagnostics data */
    void *cb_arg;                          /*!< User data to pass in callback function */
} esp_diag_variable_config_t;

/**
 * @brief Structure for diagnostics variable metadata
 */
typedef struct {
    const char *tag;           /*!< Tag of variable */
    const char *key;           /*!< Unique key for the variable */
    const char *label;         /*!< Label for the variable */
    const char *path;          /*!< Hierarchical path for the key, must be separated by '.' for more than one level,
                                    eg: "wifi", "heap.internal", "heap.external" */
    esp_diag_data_type_t type; /*!< Data type of variables */
} esp_diag_variable_meta_t;

/**
 * @brief Initialize the diagnostics variable
 *
 * @param[in] config Pointer to a config structure of type \ref esp_diag_variable_config_t
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_variable_init(esp_diag_variable_config_t *config);

/**
 * @brief Deinitialize the diagnostics variables
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_variables_deinit(void);

/**
 * @brief Register a diagnostics variable
 *
 * @param[in] tag   Tag of variable
 * @param[in] key   Unique key for the variable
 * @param[in] label Label for the variable
 * @param[in] path  Hierarchical path for key, must be separated by '.' for more than one level
 * @param[in] type  Data type of variable
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_variable_register(const char *tag,
                                     const char *key,
                                     const char *label,
                                     const char *path,
                                     esp_diag_data_type_t type);

/**
 * @brief Unregister a diagnostics variable
 *
 * @param[in] key Key for the variable
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_variable_unregister(const char *key);

/**
 * @brief Unregister all previously registered variables
 *
 * @return ESP_OK if successful, qppropriate error code otherwise.
 */
esp_err_t esp_diag_variable_unregister_all(void);

/**
 * @brief Get metadata for all variables
 *
 * @param[out] len Length of the variables  meta data array
 *
 * @return array Array of variables meta data
 */
const esp_diag_variable_meta_t *esp_diag_variable_meta_get_all(uint32_t *len);

/**
 * @brief Print metadata for all variables
 */
void esp_diag_variable_meta_print_all(void);

/**
 * @brief Add variable to storage
 *
 * @param[in] data_type Data type of variable \ref esp_diag_data_type_t
 * @param[in] key       Key of variable
 * @param[in] val       Value of variable
 * @param[in] val_sz    Size of val
 * @param[in] ts        Timestamp in microseconds, this should be the value at the time of data gathering
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 *
 * @note \ref esp_diag_timestamp_get() API can be used to get timestamp in mircoseconds.
 */
esp_err_t esp_diag_variable_add(esp_diag_data_type_t data_type,
                                const char *key, const void *val,
                                size_t val_sz, uint64_t ts);

/**
 * @brief Add the variable of data type boolean
 *
 * @param[in] key Key of the variable
 * @param[in] b   Value of the variable
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_variable_add_bool(const char *key, bool b);

/**
 * @brief Add the variable of data type integer
 *
 * @param[in] key Key of the variable
 * @param[in] i   Value of the variable
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_variable_add_int(const char *key, int32_t i);

/**
 * @brief Add the variable of data type unsigned integer
 *
 * @param[in] key Key of the variable
 * @param[in] u   Value of the variable
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_variable_add_uint(const char *key, uint32_t u);

/**
 * @brief Add the variable of data type float
 *
 * @param[in] key Key of the variable
 * @param[in] f   Value of the variable
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_variable_add_float(const char *key, float f);

/**
 * @brief Add the IPv4 address variable
 *
 * @param[in] key Key of the variable
 * @param[in] ip  IPv4 address
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_variable_add_ipv4(const char *key, uint32_t ip);

/**
 * @brief Add the MAC address variable
 *
 * @param[in] key Key of the variable
 * @param[in] mac Array of length 6 i.e 6 octets of mac address
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_variable_add_mac(const char *key, uint8_t *mac);

/**
 * @brief Add the variable of data type string
 *
 * @param[in] key Key of the variable
 * @param[in] str Value of the variable
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_variable_add_str(const char *key, const char *str);

#endif /* CONFIG_DIAG_ENABLE_VARIABLES */

#ifdef __cplusplus
}
#endif
