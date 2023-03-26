// Copyright 2018-2019 Espressif Systems (Shanghai) PTE LTD
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

#ifndef _DSP_TESTS_H_
#define _DSP_TESTS_H_

#include <stdlib.h>
#include "esp_idf_version.h"

#define TEST_ASSERT_EXEC_IN_RANGE(min_exec, max_exec, actual) \
    if (actual >= max_exec) { \
        ESP_LOGE("", "Time error. Expected max: %i, reached: %i", (int)max_exec, (int)actual);\
        TEST_ASSERT_MESSAGE (false, "Exec time takes more than expected! ");\
    }\
    if (actual < min_exec) {\
        ESP_LOGE("", "Time error. Expected min: %i, reached: %i", (int)min_exec, (int)actual);\
        TEST_ASSERT_MESSAGE (false, "Exec time takes less then expected!");\
    }


// memalign function is implemented in IDF 4.3 and later
#if ESP_IDF_VERSION <= ESP_IDF_VERSION_VAL(4, 3, 0)
#define memalign(align_, size_) malloc(size_)
#endif

#endif // _DSP_TESTS_H_