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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** Schedule Handle */
typedef void *esp_schedule_handle_t;

/** Maximum length of the schedule name allowed. This value cannot be more than 16 as it is used for NVS key. */
#define MAX_SCHEDULE_NAME_LEN 16

/** Callback for schedule trigger
 *
 * This callback is called when the schedule is triggered.
 *
 * @param[in] handle Schedule handle.
 * @param[in] priv_data Pointer to the private data passed while creating/editing the schedule.
 */
typedef void (*esp_schedule_trigger_cb_t)(esp_schedule_handle_t handle, void *priv_data);

/** Callback for schedule timestamp
 *
 * This callback is called when the next trigger timestamp of the schedule is changed. This might be useful to check if
 * one time schedules have already passed while the device was powered off.
 *
 * @param[in] handle Schedule handle.
 * @param[in] next_timestamp timestamp at which the schedule will trigger next.
 * @param[in] priv_data Pointer to the user data passed while creating/editing the schedule.
 */
typedef void (*esp_schedule_timestamp_cb_t)(esp_schedule_handle_t handle, uint32_t next_timestamp, void *priv_data);

/** Schedule type */
typedef enum esp_schedule_type {
    ESP_SCHEDULE_TYPE_INVALID = 0,
    ESP_SCHEDULE_TYPE_DAYS_OF_WEEK,
    ESP_SCHEDULE_TYPE_DATE,
    ESP_SCHEDULE_TYPE_RELATIVE,
} esp_schedule_type_t;

/** Schedule days. Used for ESP_SCHEDULE_TYPE_DAYS_OF_WEEK. */
typedef enum esp_schedule_days {
    ESP_SCHEDULE_DAY_ONCE      = 0,
    ESP_SCHEDULE_DAY_EVERYDAY  = 0b1111111,
    ESP_SCHEDULE_DAY_MONDAY    = 1 << 0,
    ESP_SCHEDULE_DAY_TUESDAY   = 1 << 1,
    ESP_SCHEDULE_DAY_WEDNESDAY = 1 << 2,
    ESP_SCHEDULE_DAY_THURSDAY  = 1 << 3,
    ESP_SCHEDULE_DAY_FRIDAY    = 1 << 4,
    ESP_SCHEDULE_DAY_SATURDAY  = 1 << 5,
    ESP_SCHEDULE_DAY_SUNDAY    = 1 << 6,
} esp_schedule_days_t;

/** Schedule months. Used for ESP_SCHEDULE_TYPE_DATE. */
typedef enum esp_schedule_months {
    ESP_SCHEDULE_MONTH_ONCE         = 0,
    ESP_SCHEDULE_MONTH_ALL          = 0b1111111,
    ESP_SCHEDULE_MONTH_JANUARY      = 1 << 0,
    ESP_SCHEDULE_MONTH_FEBRUARY     = 1 << 1,
    ESP_SCHEDULE_MONTH_MARCH        = 1 << 2,
    ESP_SCHEDULE_MONTH_APRIL        = 1 << 3,
    ESP_SCHEDULE_MONTH_MAY          = 1 << 4,
    ESP_SCHEDULE_MONTH_JUNE         = 1 << 5,
    ESP_SCHEDULE_MONTH_JULY         = 1 << 6,
    ESP_SCHEDULE_MONTH_AUGUST       = 1 << 7,
    ESP_SCHEDULE_MONTH_SEPTEMBER    = 1 << 8,
    ESP_SCHEDULE_MONTH_OCTOBER      = 1 << 9,
    ESP_SCHEDULE_MONTH_NOVEMBER     = 1 << 10,
    ESP_SCHEDULE_MONTH_DECEMBER     = 1 << 11,
} esp_schedule_months_t;

/** Trigger details of the schedule */
typedef struct esp_schedule_trigger {
    /** Type of schedule */
    esp_schedule_type_t type;
    /** Hours in 24 hour format. Accepted values: 0-23 */
    uint8_t hours;
    /** Minutes in the given hour. Accepted values: 0-59. */
    uint8_t minutes;
    /** For type ESP_SCHEDULE_TYPE_DAYS_OF_WEEK */
    struct {
        /** 'OR' list of esp_schedule_days_t */
        uint8_t repeat_days;
    } day;
    /** For type ESP_SCHEDULE_TYPE_DATE */
    struct {
        /** Day of the month. Accepted values: 1-31. */
        uint8_t day;
        /* 'OR' list of esp_schedule_months_t */
        uint16_t repeat_months;
        /** Year */
        uint16_t year;
        /** If the schedule is to be repeated every year. */
        bool repeat_every_year;
    } date;
    /** For type ESP_SCHEDULE_TYPE_SECONDS */
    int relative_seconds;
    /** Used for passing the next schedule timestamp for
     * ESP_SCHEDULE_TYPE_RELATIVE */
    time_t next_scheduled_time_utc;
} esp_schedule_trigger_t;

/** Schedule config */
typedef struct esp_schedule_config {
    /** Name of the schedule. This is like a primary key for the schedule. This is required. +1 for NULL termination. */
    char name[MAX_SCHEDULE_NAME_LEN + 1];
    /** Trigger details */
    esp_schedule_trigger_t trigger;
    /** Trigger callback */
    esp_schedule_trigger_cb_t trigger_cb;
    /** Timestamp callback */
    esp_schedule_timestamp_cb_t timestamp_cb;
    /** Private data associated with the schedule. This will be passed to callbacks. */
    void *priv_data;
} esp_schedule_config_t;

/** Initialize ESP Schedule
 *
 * This initializes ESP Schedule. This must be called first before calling any of the other APIs.
 * This API also gets all the schedules from NVS (if it has been enabled).
 *
 * Note: After calling this API, the pointers to the callbacks should be updated for all the schedules by calling
 * esp_schedule_get() followed by esp_schedule_edit() with the correct callbacks.
 *
 * @param[in] enable_nvs If NVS is to be enabled or not.
 * @param[in] nvs_partition (Optional) The NVS partition to be used. If NULL is passed, the default partition is used.
 * @param[out] schedule_count Number of active schedules found in NVS.
 *
 * @return Array of schedule handles if any schedules have been found.
 * @return NULL if no schedule is found in NVS (or if NVS is not enabled).
 */
esp_schedule_handle_t *esp_schedule_init(bool enable_nvs, char *nvs_partition, uint8_t *schedule_count);

/** Create Schedule
 *
 * This API can be used to create a new schedule. The schedule still needs to be enabled using
 * esp_schedule_enable().
 *
 * @param[in] schedule_config Configuration of the schedule to be created.
 *
 * @return Schedule handle if successfully created.
 * @return NULL in case of error.
 */
esp_schedule_handle_t esp_schedule_create(esp_schedule_config_t *schedule_config);

/** Remove Schedule
 *
 * This API can be used to remove an existing schedule.
 *
 * @param[in] handle Schedule handle for the schedule to be removed.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_schedule_delete(esp_schedule_handle_t handle);

/** Edit Schedule
 *
 * This API can be used to edit an existing schedule.
 * The schedule name should be same as when the schedule was created. The complete config must be provided
 * or the previously stored config might be over-written.
 *
 * Note: If a schedule is edited when it is on-going, the new changes will not be reflected.
 * You will need to disable the schedule, edit it, and then enable it again.
 *
 * @param[in] handle Schedule handle for the schedule to be edited.
 * @param[in] schedule_config Configuration of the schedule to be edited.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_schedule_edit(esp_schedule_handle_t handle, esp_schedule_config_t *schedule_config);

/** Enable Schedule
 *
 * This API can be used to enable an existing schedule.
 * It can be used to enable a schedule after it has been created using esp_schedule_create()
 * or if the schedule has been disabled using esp_schedule_disable().
 *
 * @param[in] handle Schedule handle for the schedule to be enabled.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_schedule_enable(esp_schedule_handle_t handle);

/** Disable Schedule
 *
 * This API can be used to disable an on-going schedule.
 * It does not remove the schedule, just stops it. The schedule can be enabled again using
 * esp_schedule_enable().
 *
 * @param[in] handle Schedule handle for the schedule to be disabled.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_schedule_disable(esp_schedule_handle_t handle);

/** Get Schedule
 *
 * This API can be used to get details of an existing schedule.
 * The schedule_config is populated with the schedule details.
 *
 * @param[in] handle Schedule handle.
 * @param[out] schedule_config Details of the schedule whose handle is passed.
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_schedule_get(esp_schedule_handle_t handle, esp_schedule_config_t *schedule_config);

#ifdef __cplusplus
}
#endif
