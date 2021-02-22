#ifndef __MY_LOG__
#define __MY_LOG__
#include "stdio.h"
#include "esp32-hal-log.h"
void log_to_esp(char* tag, esp_log_level_t level, const char *format, ...)
{
    va_list va_args;
    va_start(va_args, format);

    char log_buffer[512];
    int len = vsnprintf(log_buffer, sizeof(log_buffer), format, va_args);
    if (len > 0)
    {
        ESP_LOG_LEVEL_LOCAL(level, tag, "%s", log_buffer);
    }

    va_end(va_args);
}
#endif
