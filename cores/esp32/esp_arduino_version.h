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

#ifdef __cplusplus
extern "C" {
#endif

/** Major version number (X.x.x) */
#define ESP_ARDUINO_VERSION_MAJOR 3
/** Minor version number (x.X.x) */
#define ESP_ARDUINO_VERSION_MINOR 0
/** Patch version number (x.x.X) */
#define ESP_ARDUINO_VERSION_PATCH 3

/**
 * Macro to convert ARDUINO version number into an integer
 *
 * To be used in comparisons, such as ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2, 0, 0)
 */
#define ESP_ARDUINO_VERSION_VAL(major, minor, patch) ((major << 16) | (minor << 8) | (patch))

/**
 * Current ARDUINO version, as an integer
 *
 * To be used in comparisons, such as ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2, 0, 0)
 */
#define ESP_ARDUINO_VERSION ESP_ARDUINO_VERSION_VAL(ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH)

/**
 * Current ARDUINO version, as string
 */
#define df2xstr(s)              #s
#define df2str(s)               df2xstr(s)
#define ESP_ARDUINO_VERSION_STR df2str(ESP_ARDUINO_VERSION_MAJOR) "." df2str(ESP_ARDUINO_VERSION_MINOR) "." df2str(ESP_ARDUINO_VERSION_PATCH)

#ifdef __cplusplus
}
#endif
