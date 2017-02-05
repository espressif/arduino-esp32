// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
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
#ifndef __ARDUHAL_LOG_H__
#define __ARDUHAL_LOG_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "sdkconfig.h"

#define ARDUHAL_LOG_LEVEL_NONE       (0)
#define ARDUHAL_LOG_LEVEL_ERROR      (1)
#define ARDUHAL_LOG_LEVEL_WARN       (2)
#define ARDUHAL_LOG_LEVEL_INFO       (3)
#define ARDUHAL_LOG_LEVEL_DEBUG      (4)
#define ARDUHAL_LOG_LEVEL_VERBOSE    (5)

#ifndef CONFIG_ARDUHAL_LOG_DEFAULT_LEVEL
#define CONFIG_ARDUHAL_LOG_DEFAULT_LEVEL ARDUHAL_LOG_LEVEL_NONE
#endif

#ifndef CORE_DEBUG_LEVEL
#define ARDUHAL_LOG_LEVEL CONFIG_ARDUHAL_LOG_DEFAULT_LEVEL
#else
#define ARDUHAL_LOG_LEVEL CORE_DEBUG_LEVEL
#endif

#ifndef CONFIG_ARDUHAL_LOG_COLORS
#define CONFIG_ARDUHAL_LOG_COLORS 0
#endif

#if CONFIG_ARDUHAL_LOG_COLORS
#define ARDUHAL_LOG_COLOR_BLACK   "30"
#define ARDUHAL_LOG_COLOR_RED     "31" //ERROR
#define ARDUHAL_LOG_COLOR_GREEN   "32" //INFO
#define ARDUHAL_LOG_COLOR_YELLOW  "33" //WARNING
#define ARDUHAL_LOG_COLOR_BLUE    "34"
#define ARDUHAL_LOG_COLOR_MAGENTA "35"
#define ARDUHAL_LOG_COLOR_CYAN    "36" //DEBUG
#define ARDUHAL_LOG_COLOR_GRAY    "37" //VERBOSE
#define ARDUHAL_LOG_COLOR_WHITE   "38"

#define ARDUHAL_LOG_COLOR(COLOR)  "\033[0;" COLOR "m"
#define ARDUHAL_LOG_BOLD(COLOR)   "\033[1;" COLOR "m"
#define ARDUHAL_LOG_RESET_COLOR   "\033[0m"

#define ARDUHAL_LOG_COLOR_E       ARDUHAL_LOG_COLOR(ARDUHAL_LOG_COLOR_RED)
#define ARDUHAL_LOG_COLOR_W       ARDUHAL_LOG_COLOR(ARDUHAL_LOG_COLOR_YELLOW)
#define ARDUHAL_LOG_COLOR_I       ARDUHAL_LOG_COLOR(ARDUHAL_LOG_COLOR_GREEN)
#define ARDUHAL_LOG_COLOR_D       ARDUHAL_LOG_COLOR(ARDUHAL_LOG_COLOR_CYAN)
#define ARDUHAL_LOG_COLOR_V       ARDUHAL_LOG_COLOR(ARDUHAL_LOG_COLOR_GRAY)
#else
#define ARDUHAL_LOG_COLOR_E
#define ARDUHAL_LOG_COLOR_W
#define ARDUHAL_LOG_COLOR_I
#define ARDUHAL_LOG_COLOR_D
#define ARDUHAL_LOG_COLOR_V
#define ARDUHAL_LOG_RESET_COLOR
#endif

const char * pathToFileName(const char * path);
int log_printf(const char *fmt, ...);

#define ARDUHAL_SHORT_LOG_FORMAT(letter, format)  ARDUHAL_LOG_COLOR_ ## letter format ARDUHAL_LOG_RESET_COLOR "\r\n"
#define ARDUHAL_LOG_FORMAT(letter, format)  ARDUHAL_LOG_COLOR_ ## letter "[" #letter "][%s:%u] %s(): " format ARDUHAL_LOG_RESET_COLOR "\r\n", pathToFileName(__FILE__), __LINE__, __FUNCTION__

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
#define log_v(format, ...) log_printf(ARDUHAL_LOG_FORMAT(V, format), ##__VA_ARGS__)
#else
#define log_v(format, ...)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
#define log_d(format, ...) log_printf(ARDUHAL_LOG_FORMAT(D, format), ##__VA_ARGS__)
#else
#define log_d(format, ...)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
#define log_i(format, ...) log_printf(ARDUHAL_LOG_FORMAT(I, format), ##__VA_ARGS__)
#else
#define log_i(format, ...)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_WARN
#define log_w(format, ...) log_printf(ARDUHAL_LOG_FORMAT(W, format), ##__VA_ARGS__)
#else
#define log_w(format, ...)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
#define log_e(format, ...) log_printf(ARDUHAL_LOG_FORMAT(E, format), ##__VA_ARGS__)
#else
#define log_e(format, ...)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __ESP_LOGGING_H__ */
