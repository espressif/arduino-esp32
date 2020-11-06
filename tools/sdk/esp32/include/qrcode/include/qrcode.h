// Copyright 2019 Espressif Systems (Shanghai) PTE LTD
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

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief  Generate and display QR Code on the console
  *         Encodes the given string into a QR Code & displays it on the console
  *
  * @attention 1. Can successfully encode a UTF-8 string of up to 2953 bytes or an alphanumeric
  *               string of up to 4296 characters or any digit string of up to 7089 characters
  *
  * @param  text  string to encode into a QR Code.
  *
  * @return
  *    - ESP_OK: succeed
  *    - ESP_FAIL: Failed to encode string into a QR Code
  *    - ESP_ERR_NO_MEM: Failed to allocate buffer for given MAX_QRCODE_VERSION
  */
esp_err_t qrcode_display(const char *text);

#ifdef __cplusplus
}
#endif
