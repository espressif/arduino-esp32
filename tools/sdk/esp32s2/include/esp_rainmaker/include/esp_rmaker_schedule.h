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
extern "C"
{
#endif

/** Enable Schedules
 *
 * This API enables the scheduling service for the node. For more information,
 * check [here](https://rainmaker.espressif.com/docs/scheduling.html)
 *
 * It is recommended to set the timezone while using schedules. Check [here](https://rainmaker.espressif.com/docs/time-service.html#time-zone) for more information on timezones
 *
 * @note This API should be called after esp_rmaker_node_init() but before esp_rmaker_start().
 *
 * @return ESP_OK on success.
 * @return error in case of failure.
 */
esp_err_t esp_rmaker_schedule_enable(void);

#ifdef __cplusplus
}
#endif
