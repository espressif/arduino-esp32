/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"

/*
 * This file provides wrapper implementations for esp_log_write and esp_log_writev
 * when CONFIG_DIAG_USE_EXTERNAL_LOG_WRAP is not enabled.
 *
 * When CONFIG_DIAG_USE_EXTERNAL_LOG_WRAP is enabled, the esp_diagnostics component
 * provides these wrappers. However, when it's disabled, WiFi libraries still expect
 * these wrapper functions to exist, causing linker errors.
 *
 * This implementation provides simple pass-through wrappers that call the real
 * ESP-IDF logging functions, ensuring compatibility without requiring esp_diagnostics.
 */

#ifndef CONFIG_DIAG_USE_EXTERNAL_LOG_WRAP

#include <stdarg.h>
#include "esp_log.h"

// Declare the real functions that will be wrapped by the linker
void __real_esp_log_write(esp_log_level_t level, const char *tag, const char *format, ...);
void __real_esp_log_writev(esp_log_level_t level, const char *tag, const char *format, va_list args);

// Wrapper implementations that simply call through to the real functions
void __wrap_esp_log_write(esp_log_level_t level, const char *tag, const char *format, ...) {
  va_list args;
  va_start(args, format);
  __real_esp_log_writev(level, tag, format, args);
  va_end(args);
}

void __wrap_esp_log_writev(esp_log_level_t level, const char *tag, const char *format, va_list args) {
  __real_esp_log_writev(level, tag, format, args);
}

#endif  // !CONFIG_DIAG_USE_EXTERNAL_LOG_WRAP
