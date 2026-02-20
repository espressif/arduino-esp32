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

#include "StringUtils.h"

#define U64_STR_SIZE 21  // 20 digits + '\0'
#define I64_STR_SIZE 22  // '-' + 19 digits + '\0'

char* u64_to_str(uint64_t value, char *buffer) {
    char *end = buffer + 20;
    char *p   = end;
    *p = '\0';

    // reverse fill
    do {
        *--p = '0' + (value % 10);
        value /= 10;
    } while (value);

    // shift to beginning
    char *dst = buffer;
    while ((*dst++ = *p++)) { }

    return buffer;
}

char* i64_to_str(int64_t value, char *buffer) {
    if (value >= 0) {
        return u64_to_str((uint64_t)value, buffer);
    }

    // Safe magnitude (handles INT64_MIN)
    uint64_t magnitude = (uint64_t)(-(value + 1)) + 1;

    u64_to_str(magnitude, buffer);

    // prepend '-'
    char *end = buffer;
    while (*end) end++;        // find null terminator
    while (end >= buffer) {    // shift right including '\0'
        *(end + 1) = *end;
        if (end == buffer) break;
        end--;
    }
    buffer[0] = '-';

    return buffer;
}

String u64_to_String(uint64_t value) {
    char buf[U64_STR_SIZE];
    return String(u64_to_str(value, buf));
}

String i64_to_String(int64_t value) {
    char buf[I64_STR_SIZE];
    return String(i64_to_str(value, buf));
}
