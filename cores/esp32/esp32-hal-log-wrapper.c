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

#include <stdarg.h>

#ifndef CONFIG_DIAG_USE_EXTERNAL_LOG_WRAP

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

/*
 * Wrapper for log_printf (Arduino logging function).
 * The --wrap=log_printf linker flag redirects calls to __wrap_log_printf.
 * This wrapper builds a va_list and forwards the call to log_printfv().
 * This must be outside the CONFIG_DIAG_USE_EXTERNAL_LOG_WRAP guard since
 * neither esp_diagnostics nor any other component provides this wrapper.
 */

extern int log_printfv(const char *format, va_list arg);

int __wrap_log_printf(const char *format, ...) {
  va_list args;
  va_start(args, format);
  int len = log_printfv(format, args);
  va_end(args);
  return len;
}
