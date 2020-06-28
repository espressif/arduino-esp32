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
