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
extern "C" {
#endif

#include "sdkconfig.h"
#include "esp_timer.h"

#define ARDUHAL_LOG_LEVEL_NONE (0)
#define ARDUHAL_LOG_LEVEL_ERROR (1)
#define ARDUHAL_LOG_LEVEL_WARN (2)
#define ARDUHAL_LOG_LEVEL_INFO (3)
#define ARDUHAL_LOG_LEVEL_DEBUG (4)
#define ARDUHAL_LOG_LEVEL_VERBOSE (5)

#ifndef CONFIG_ARDUHAL_LOG_DEFAULT_LEVEL
#define CONFIG_ARDUHAL_LOG_DEFAULT_LEVEL ARDUHAL_LOG_LEVEL_NONE
#endif

#ifndef CORE_DEBUG_LEVEL
#define ARDUHAL_LOG_LEVEL CONFIG_ARDUHAL_LOG_DEFAULT_LEVEL
#else
#define ARDUHAL_LOG_LEVEL CORE_DEBUG_LEVEL
#ifdef USE_ESP_IDF_LOG
#ifndef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL CORE_DEBUG_LEVEL
#endif
#endif
#endif

#ifndef CONFIG_ARDUHAL_LOG_COLORS
#define CONFIG_ARDUHAL_LOG_COLORS 0
#endif

#if CONFIG_ARDUHAL_LOG_COLORS
#define ARDUHAL_LOG_COLOR_BLACK "30"
#define ARDUHAL_LOG_COLOR_RED "31"     //ERROR
#define ARDUHAL_LOG_COLOR_GREEN "32"   //INFO
#define ARDUHAL_LOG_COLOR_YELLOW "33"  //WARNING
#define ARDUHAL_LOG_COLOR_BLUE "34"
#define ARDUHAL_LOG_COLOR_MAGENTA "35"
#define ARDUHAL_LOG_COLOR_CYAN "36"  //DEBUG
#define ARDUHAL_LOG_COLOR_GRAY "37"  //VERBOSE
#define ARDUHAL_LOG_COLOR_WHITE "38"

#define ARDUHAL_LOG_COLOR(COLOR) "\033[0;" COLOR "m"
#define ARDUHAL_LOG_BOLD(COLOR) "\033[1;" COLOR "m"
#define ARDUHAL_LOG_RESET_COLOR "\033[0m"

#define ARDUHAL_LOG_COLOR_E ARDUHAL_LOG_COLOR(ARDUHAL_LOG_COLOR_RED)
#define ARDUHAL_LOG_COLOR_W ARDUHAL_LOG_COLOR(ARDUHAL_LOG_COLOR_YELLOW)
#define ARDUHAL_LOG_COLOR_I ARDUHAL_LOG_COLOR(ARDUHAL_LOG_COLOR_GREEN)
#define ARDUHAL_LOG_COLOR_D ARDUHAL_LOG_COLOR(ARDUHAL_LOG_COLOR_CYAN)
#define ARDUHAL_LOG_COLOR_V ARDUHAL_LOG_COLOR(ARDUHAL_LOG_COLOR_GRAY)
#define ARDUHAL_LOG_COLOR_PRINT(letter) log_printf(ARDUHAL_LOG_COLOR_##letter)
#define ARDUHAL_LOG_COLOR_PRINT_END log_printf(ARDUHAL_LOG_RESET_COLOR)
#else
#define ARDUHAL_LOG_COLOR_E
#define ARDUHAL_LOG_COLOR_W
#define ARDUHAL_LOG_COLOR_I
#define ARDUHAL_LOG_COLOR_D
#define ARDUHAL_LOG_COLOR_V
#define ARDUHAL_LOG_RESET_COLOR
#define ARDUHAL_LOG_COLOR_PRINT(letter)
#define ARDUHAL_LOG_COLOR_PRINT_END
#endif



  const char *pathToFileName(const char *path);
  int log_printf(const char *fmt, ...);
  void log_print_buf(const uint8_t *b, size_t len);

#define ARDUHAL_SHORT_LOG_FORMAT(letter, format) ARDUHAL_LOG_COLOR_##letter format ARDUHAL_LOG_RESET_COLOR "\r\n"
#define ARDUHAL_LOG_FORMAT(letter, format) ARDUHAL_LOG_COLOR_##letter "[%6u][" #letter "][%s:%u] %s(): " format ARDUHAL_LOG_RESET_COLOR "\r\n", (unsigned long)(esp_timer_get_time() / 1000ULL), pathToFileName(__FILE__), __LINE__, __FUNCTION__

  //esp_rom_printf(DRAM_STR("ST:%d\n"), frame_pos);

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
#ifndef USE_ESP_IDF_LOG
#define log_v(format, ...) log_printf(ARDUHAL_LOG_FORMAT(V, format), ##__VA_ARGS__)
#define isr_log_v(format, ...) ets_printf(ARDUHAL_LOG_FORMAT(V, format), ##__VA_ARGS__)
#define log_buf_v(b, l) \
  do { \
    ARDUHAL_LOG_COLOR_PRINT(V); \
    log_print_buf(b, l); \
    ARDUHAL_LOG_COLOR_PRINT_END; \
  } while (0)
#else
#define log_v(format, ...) \
  do { ESP_LOG_LEVEL_LOCAL(ESP_LOG_VERBOSE, TAG, format, ##__VA_ARGS__); } while (0)
#define isr_log_v(format, ...) \
  do { ets_printf(LOG_FORMAT(V, format), esp_log_timestamp(), TAG, ##__VA_ARGS__); } while (0)
#define log_buf_v(b, l) \
  do { ESP_LOG_BUFFER_HEXDUMP(TAG, b, l, ESP_LOG_VERBOSE); } while (0)
#endif
#else
#define log_v(format, ...) \
  do { \
  } while (0)
#define isr_log_v(format, ...) \
  do { \
  } while (0)
#define log_buf_v(b, l) \
  do { \
  } while (0)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
#ifndef USE_ESP_IDF_LOG
#define log_d(format, ...) log_printf(ARDUHAL_LOG_FORMAT(D, format), ##__VA_ARGS__)
#define isr_log_d(format, ...) ets_printf(ARDUHAL_LOG_FORMAT(D, format), ##__VA_ARGS__)
#define log_buf_d(b, l) \
  do { \
    ARDUHAL_LOG_COLOR_PRINT(D); \
    log_print_buf(b, l); \
    ARDUHAL_LOG_COLOR_PRINT_END; \
  } while (0)
#else
#define log_d(format, ...) \
  do { ESP_LOG_LEVEL_LOCAL(ESP_LOG_DEBUG, TAG, format, ##__VA_ARGS__); } while (0)
#define isr_log_d(format, ...) \
  do { ets_printf(LOG_FORMAT(D, format), esp_log_timestamp(), TAG, ##__VA_ARGS__); } while (0)
#define log_buf_d(b, l) \
  do { ESP_LOG_BUFFER_HEXDUMP(TAG, b, l, ESP_LOG_DEBUG); } while (0)
#endif
#else
#define log_d(format, ...) \
  do { \
  } while (0)
#define isr_log_d(format, ...) \
  do { \
  } while (0)
#define log_buf_d(b, l) \
  do { \
  } while (0)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
#ifndef USE_ESP_IDF_LOG
#define log_i(format, ...) log_printf(ARDUHAL_LOG_FORMAT(I, format), ##__VA_ARGS__)
#define isr_log_i(format, ...) ets_printf(ARDUHAL_LOG_FORMAT(I, format), ##__VA_ARGS__)
#define log_buf_i(b, l) \
  do { \
    ARDUHAL_LOG_COLOR_PRINT(I); \
    log_print_buf(b, l); \
    ARDUHAL_LOG_COLOR_PRINT_END; \
  } while (0)
#else
#define log_i(format, ...) \
  do { ESP_LOG_LEVEL_LOCAL(ESP_LOG_INFO, TAG, format, ##__VA_ARGS__); } while (0)
#define isr_log_i(format, ...) \
  do { ets_printf(LOG_FORMAT(I, format), esp_log_timestamp(), TAG, ##__VA_ARGS__); } while (0)
#define log_buf_i(b, l) \
  do { ESP_LOG_BUFFER_HEXDUMP(TAG, b, l, ESP_LOG_INFO); } while (0)
#endif
#else
#define log_i(format, ...) \
  do { \
  } while (0)
#define isr_log_i(format, ...) \
  do { \
  } while (0)
#define log_buf_i(b, l) \
  do { \
  } while (0)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_WARN
#ifndef USE_ESP_IDF_LOG
#define log_w(format, ...) log_printf(ARDUHAL_LOG_FORMAT(W, format), ##__VA_ARGS__)
#define isr_log_w(format, ...) ets_printf(ARDUHAL_LOG_FORMAT(W, format), ##__VA_ARGS__)
#define log_buf_w(b, l) \
  do { \
    ARDUHAL_LOG_COLOR_PRINT(W); \
    log_print_buf(b, l); \
    ARDUHAL_LOG_COLOR_PRINT_END; \
  } while (0)
#else
#define log_w(format, ...) \
  do { ESP_LOG_LEVEL_LOCAL(ESP_LOG_WARN, TAG, format, ##__VA_ARGS__); } while (0)
#define isr_log_w(format, ...) \
  do { ets_printf(LOG_FORMAT(W, format), esp_log_timestamp(), TAG, ##__VA_ARGS__); } while (0)
#define log_buf_w(b, l) \
  do { ESP_LOG_BUFFER_HEXDUMP(TAG, b, l, ESP_LOG_WARN); } while (0)
#endif
#else
#define log_w(format, ...) \
  do { \
  } while (0)
#define isr_log_w(format, ...) \
  do { \
  } while (0)
#define log_buf_w(b, l) \
  do { \
  } while (0)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
#ifndef USE_ESP_IDF_LOG
#define log_e(format, ...) log_printf(ARDUHAL_LOG_FORMAT(E, format), ##__VA_ARGS__)
#define isr_log_e(format, ...) ets_printf(ARDUHAL_LOG_FORMAT(E, format), ##__VA_ARGS__)
#define log_buf_e(b, l) \
  do { \
    ARDUHAL_LOG_COLOR_PRINT(E); \
    log_print_buf(b, l); \
    ARDUHAL_LOG_COLOR_PRINT_END; \
  } while (0)
#else
#define log_e(format, ...) \
  do { ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR, TAG, format, ##__VA_ARGS__); } while (0)
#define isr_log_e(format, ...) \
  do { ets_printf(LOG_FORMAT(E, format), esp_log_timestamp(), TAG, ##__VA_ARGS__); } while (0)
#define log_buf_e(b, l) \
  do { ESP_LOG_BUFFER_HEXDUMP(TAG, b, l, ESP_LOG_ERROR); } while (0)
#endif
#else
#define log_e(format, ...) \
  do { \
  } while (0)
#define isr_log_e(format, ...) \
  do { \
  } while (0)
#define log_buf_e(b, l) \
  do { \
  } while (0)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_NONE
#ifndef USE_ESP_IDF_LOG
#define log_n(format, ...) log_printf(ARDUHAL_LOG_FORMAT(E, format), ##__VA_ARGS__)
#define isr_log_n(format, ...) ets_printf(ARDUHAL_LOG_FORMAT(E, format), ##__VA_ARGS__)
#define log_buf_n(b, l) \
  do { \
    ARDUHAL_LOG_COLOR_PRINT(E); \
    log_print_buf(b, l); \
    ARDUHAL_LOG_COLOR_PRINT_END; \
  } while (0)
#else
#define log_n(format, ...) \
  do { ESP_LOG_LEVEL_LOCAL(ESP_LOG_ERROR, TAG, format, ##__VA_ARGS__); } while (0)
#define isr_log_n(format, ...) \
  do { ets_printf(LOG_FORMAT(E, format), esp_log_timestamp(), TAG, ##__VA_ARGS__); } while (0)
#define log_buf_n(b, l) \
  do { ESP_LOG_BUFFER_HEXDUMP(TAG, b, l, ESP_LOG_ERROR); } while (0)
#endif
#else
#define log_n(format, ...) \
  do { \
  } while (0)
#define isr_log_n(format, ...) \
  do { \
  } while (0)
#define log_buf_n(b, l) \
  do { \
  } while (0)
#endif

#include "esp_log.h"

#ifdef USE_ESP_IDF_LOG
//#ifndef TAG
//#define TAG "ARDUINO"
//#endif
//#define log_n(format, ...) myLog(ESP_LOG_NONE, format, ##__VA_ARGS__)
#else
#ifdef CONFIG_ARDUHAL_ESP_LOG
#undef ESP_LOGE
#undef ESP_LOGW
#undef ESP_LOGI
#undef ESP_LOGD
#undef ESP_LOGV
#undef ESP_EARLY_LOGE
#undef ESP_EARLY_LOGW
#undef ESP_EARLY_LOGI
#undef ESP_EARLY_LOGD
#undef ESP_EARLY_LOGV

#define ESP_LOGE(tag, format, ...) log_e("[%s] " format, tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) log_w("[%s] " format, tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) log_i("[%s] " format, tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) log_d("[%s] " format, tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) log_v("[%s] " format, tag, ##__VA_ARGS__)
#define ESP_EARLY_LOGE(tag, format, ...) isr_log_e("[%s] " format, tag, ##__VA_ARGS__)
#define ESP_EARLY_LOGW(tag, format, ...) isr_log_w("[%s] " format, tag, ##__VA_ARGS__)
#define ESP_EARLY_LOGI(tag, format, ...) isr_log_i("[%s] " format, tag, ##__VA_ARGS__)
#define ESP_EARLY_LOGD(tag, format, ...) isr_log_d("[%s] " format, tag, ##__VA_ARGS__)
#define ESP_EARLY_LOGV(tag, format, ...) isr_log_v("[%s] " format, tag, ##__VA_ARGS__)
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* __ESP_LOGGING_H__ */
