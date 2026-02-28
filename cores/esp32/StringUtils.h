// Copyright 2026 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef StringUtils_h
#define StringUtils_h

#include <stdint.h>
#include <WString.h>

/**
 * @brief Convert a uint64_t to a string in base 10
 *
 * @param value The value to convert
 * @param buffer The buffer to store the string. At least 21 bytes are required.
 * @return Pointer to the buffer
 */
char* u64_to_str(uint64_t value, char *buffer);

/**
 * @brief Convert a int64_t to a string in base 10
 *
 * @param value The value to convert
 * @param buffer The buffer to store the string. At least 22 bytes are required.
 * @return Pointer to the buffer
 */
char* i64_to_str(int64_t value, char *buffer);

/**
 * @brief Convert a uint64_t to a String in base 10.
 *
 * @note The buffer is allocated dynamically and returned as a String object.
 *       Do not overuse this function to avoid memory fragmentation.
 *       Prefer using @ref u64_to_str instead.
 *
 * @param value The value to convert
 * @return String
 */
String u64_to_String(uint64_t value);

/**
 * @brief Convert a int64_t to a String in base 10
 *
 * @note The buffer is allocated dynamically and returned as a String object.
 *       Do not overuse this function to avoid memory fragmentation.
 *       Prefer using @ref i64_to_str instead.
 *
 * @param value The value to convert
 * @return String
 */
String i64_to_String(int64_t value);

#endif
