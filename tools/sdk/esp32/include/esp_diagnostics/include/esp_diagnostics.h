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

#include <inttypes.h>
#include <stdbool.h>
#include <esp_err.h>
#include <esp_log.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Callback to write log to diagnostics storage
 */
typedef esp_err_t (*esp_diag_log_write_cb_t)(void *data, size_t len, void *priv_data);

/**
 * @brief Diagnostics log configurations
 */
typedef struct {
    esp_diag_log_write_cb_t write_cb;   /*!< Callback function to write diagnostics data */
    void *cb_arg;                       /*!< User data to pass in callback function */
} esp_diag_log_config_t;

/**
 * @brief Supported log types in diagnostics
 */
typedef enum {
    ESP_DIAG_LOG_TYPE_ERROR   = 1 << 0,   /*!< Diagnostics log type error */
    ESP_DIAG_LOG_TYPE_WARNING = 1 << 1,   /*!< Diagnostics log type warning */
    ESP_DIAG_LOG_TYPE_EVENT   = 1 << 2,   /*!< Diagnostics log type event */
} esp_diag_log_type_t;

/**
 * @brief Log argument data types
 */
typedef enum {
    ARG_TYPE_CHAR,      /*!< Argument type (char) */
    ARG_TYPE_SHORT,     /*!< Argument type (short) */
    ARG_TYPE_INT,       /*!< Argument type (int) */
    ARG_TYPE_L,         /*!< Argument type (long) */
    ARG_TYPE_LL,        /*!< Argument type (long long) */
    ARG_TYPE_INTMAX,    /*!< Argument type (intmax_t) */
    ARG_TYPE_PTRDIFF,   /*!< Argument type (ptrdiff_t) */
    ARG_TYPE_UCHAR,     /*!< Argument type (unsigned char) */
    ARG_TYPE_USHORT,    /*!< Argument type (unsigned short) */
    ARG_TYPE_UINT,      /*!< Argument type (unsigned int) */
    ARG_TYPE_UL,        /*!< Argument type (unsigned long) */
    ARG_TYPE_ULL,       /*!< Argument type (unsigned long long) */
    ARG_TYPE_UINTMAX,   /*!< Argument type (uintmax_t) */
    ARG_TYPE_SIZE,      /*!< Argument type (size_t) */
    ARG_TYPE_DOUBLE,    /*!< Argument type (double) */
    ARG_TYPE_LDOUBLE,   /*!< Argument type (long double) */
    ARG_TYPE_STR,       /*!< Argument type (char *) */
    ARG_TYPE_INVALID,   /*!< Argument type invalid */
} esp_diag_arg_type_t;

/**
 * @brief Log argument data value
 */
typedef union {
    char c;                 /*!< Value of type signed char */
    short s;                /*!< Value of type signed short */
    int i;                  /*!< Value of type signed integer */
    long l;                 /*!< Value of type signed long */
    long long ll;           /*!< Value of type signed long long */
    intmax_t imx;           /*!< Value of type intmax_t */
    ptrdiff_t ptrdiff;      /*!< Value of type ptrdiff_t */
    unsigned char uc;       /*!< Value of type unsigned char */
    unsigned short us;      /*!< Value of type unsigned short */
    unsigned int u;         /*!< Value of type unsigned integer */
    unsigned long ul;       /*!< Value of type unsigned long */
    unsigned long long ull; /*!< Value of type unsigned long long */
    uintmax_t umx;          /*!< Value of type uintmax_t */
    size_t sz;              /*!< Value of type size_t */
    double d;               /*!< Value of type double */
    long double ld;         /*!< Value of type long double */
    char *str;              /*!< value of type string */
} esp_diag_arg_value_t;

/**
 * @brief Diagnostics data point type
 */
typedef enum {
    ESP_DIAG_DATA_PT_METRICS,   /*!< Data point of type metrics */
    ESP_DIAG_DATA_PT_VARIABLE,  /*!< Data point of type variable */
} esp_diag_data_pt_type_t;

/**
 * @brief Diagnostics data types
 */
typedef enum {
    ESP_DIAG_DATA_TYPE_BOOL,     /*!< Data type boolean */
    ESP_DIAG_DATA_TYPE_INT,      /*!< Data type integer */
    ESP_DIAG_DATA_TYPE_UINT,     /*!< Data type unsigned integer */
    ESP_DIAG_DATA_TYPE_FLOAT,    /*!< Data type float */
    ESP_DIAG_DATA_TYPE_STR,      /*!< Data type string */
    ESP_DIAG_DATA_TYPE_IPv4,     /*!< Data type IPv4 address */
    ESP_DIAG_DATA_TYPE_MAC,      /*!< Data type MAC address */
} esp_diag_data_type_t;

/**
 * @brief Diagnostics log data structure
 */
typedef struct {
    esp_diag_log_type_t type;                           /*!< Type of diagnostics log */
    uint32_t pc;                                        /*!< Program Counter */
    uint64_t timestamp;                                 /*!< If NTP sync enabled then POSIX time,
                                                             otherwise relative time since bootup in microseconds */
    char tag[16];                                       /*!< Tag of log message */
    void *msg_ptr;                                      /*!< Address of err/warn/event message in rodata */
    uint8_t msg_args[CONFIG_DIAG_LOG_MSG_ARG_MAX_SIZE]; /*!< Arguments of log message */
    uint8_t msg_args_len;                               /*!< Length of argument */
    char task_name[CONFIG_FREERTOS_MAX_TASK_NAME_LEN];  /*!< Task name */
} esp_diag_log_data_t;

/**
 * @brief Device information structure
 */
typedef struct {
    uint32_t chip_model;                                      /*!< Chip model */
    uint32_t chip_rev;                                        /*!< Chip revision */
    uint32_t reset_reason;                                    /*!< Reset reason */
    char app_version[32];                                     /*!< Application version */
    char project_name[32];                                    /*!< Project name */
    char app_elf_sha256[CONFIG_APP_RETRIEVE_LEN_ELF_SHA + 1]; /*!< SHA256 of application elf */
} esp_diag_device_info_t;

/**
 * @brief Task backtrace structure
 */
typedef struct {
    uint32_t bt[16];    /*!< Backtrace (array of PC) */
    uint32_t depth;     /*!< Number of backtrace entries */
    bool corrupted;     /*!< Status flag for backtrace is corrupt or not */
} esp_diag_task_bt_t;

/**
 * @brief Task information structure
 */
typedef struct {
    char name[CONFIG_FREERTOS_MAX_TASK_NAME_LEN];   /*!< Task name */
    uint32_t state;                                 /*!< Task state */
    uint32_t high_watermark;                        /*!< Task high watermark */
#ifndef CONFIG_IDF_TARGET_ARCH_RISCV
    esp_diag_task_bt_t bt_info;                     /*!< Backtrace of the task */
#endif /* !CONFIG_IDF_TARGET_ARCH_RISCV */
} esp_diag_task_info_t;

/**
 * @brief Structure for diagnostics data point
 */
typedef struct {
    uint16_t type;       /*!< Metrics or Variable */
    uint16_t data_type;  /*!< Data type */
    char key[16];        /*!< Key */
    uint64_t ts;         /*!< Timestamp */
    union {
        bool b;          /*!< Value for boolean data type */
        int32_t i;       /*!< Value for integer data type */
        uint32_t u;      /*!< Value for unsigned integer data type */
        float f;         /*!< Value for float data type */
        uint32_t ipv4;   /*!< Value for the IPv4 address */
        uint8_t mac[6];  /*!< Value for the MAC address */
    } value;
} esp_diag_data_pt_t;

/**
 * @brief Structure for string data type diagnostics data point
 */
typedef struct {
    uint16_t type;       /*!< Metrics or Variable */
    uint16_t data_type;  /*!< Data type */
    char key[16];        /*!< Key */
    uint64_t ts;         /*!< Timestamp */
    union {
        char str[32];    /*!< Value for string data type */
    } value;
} esp_diag_str_data_pt_t;

/**
 * @brief Initialize diagnostics log hook
 *
 * @param[in] config Pointer to a config structure of type \ref esp_diag_log_config_t
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 */
esp_err_t esp_diag_log_hook_init(esp_diag_log_config_t *config);

/**
 * @brief Enable the diagnostics log hook for provided log type
 *
 * @param[in] type Log type to enable, can be the bitwise OR of types from \ref esp_diag_log_type_t
 */
void esp_diag_log_hook_enable(uint32_t type);

/**
 * @brief Disable the diagnostics log hook for provided log type
 *
 * @param[in] type Log type to disable, can be the bitwise OR of types from \ref esp_diag_log_type_t
 *
 */
void esp_diag_log_hook_disable(uint32_t type);

/**
 * @brief Add diagnostics event
 *
 * @param[in] tag The tag of message
 * @param[in] format Message format
 * @param[in] ... Variable arguments
 *
 * @return ESP_OK if successful, appropriate error code otherwise.
 *
 * @note This function is not intended to be used directly, Instead, use macro \ref ESP_DIAG_EVENT
 */
esp_err_t esp_diag_log_event(const char *tag, const char *format, ...) __attribute__ ((format (printf, 2, 3)));

/**
 * @brief Macro to add the custom event
 *
 * @param[in] tag tag of the event
 * @param[in] format format of the event
 * @param[in] ... Variable arguments
 */
#define ESP_DIAG_EVENT(tag, format, ...) \
{ \
    esp_diag_log_event(tag, "EV (%" PRIu32 ") %s: " format, esp_log_timestamp(), tag, ##__VA_ARGS__); \
    ESP_LOGI(tag, format, ##__VA_ARGS__); \
}

/**
 * @brief Get the device information for diagnostics
 *
 * @param[out] device_info Pointer to device_info structure of type \ref esp_diag_device_info_t
 *
 * @return ESP_OK on success
 * @return ESP_ERR_INVALID_ARG if device_info is NULL
 */
esp_err_t esp_diag_device_info_get(esp_diag_device_info_t *device_info);

/**
 * @brief Get the timestamp
 *
 * This function returns POSIX time if NTP sync is enabled
 * otherwise returns time since bootup in microseconds
 *
 * @return timestamp
 */
uint64_t esp_diag_timestamp_get(void);

/**
 * @brief Get backtrace and some more details of all tasks in system
 *
 * @note On device backtrace parsing not available on RISC-V boards (ESP32C3)
 *
 * @param[out] tasks Array to store task info
 * @param[in] size Size of array, If size is less than the number of tasks in system,
 *                 then info of size tasks is filled in array
 *
 * @return Number of task info filled in array
 *
 * @note Allocate enough memory to store all tasks,
 *       Use uxTaskGetNumberOfTasks() to get number of tasks in system
 */
uint32_t esp_diag_task_snapshot_get(esp_diag_task_info_t *tasks, size_t size);

/**
 * @brief Dump backtrace and some more details of all tasks
 *        in system to console using \ref ESP_DIAG_EVENT
 */
void esp_diag_task_snapshot_dump(void);

/**
 * @brief Get CRC of diagnostics metadata
 *
 * @return crc
 */
uint32_t esp_diag_meta_crc_get(void);

#ifdef __cplusplus
}
#endif
