#ifndef __MY_LOG__
#define __MY_LOG__
#include "stdio.h"
#include "esp32-hal-log.h"
#ifdef USE_ESP32_LOG
    void log_to_esp(esp_log_level_t level, const char *format, ...)
    {   
        va_list va_args;
        va_start(va_args, format);

        char log_buffer[512];
        int len = vsnprintf(log_buffer, sizeof(log_buffer), format, va_args);
        if (len > 0)
        {
            switch (level)
            {
            case ESP_LOG_ERROR:
                ESP_LOGE(TAG, "%s", log_buffer);
                break;
            case ESP_LOG_DEBUG:
                ESP_LOGD(TAG, "%s", log_buffer);
                break;
            case ESP_LOG_WARN:
                ESP_LOGW(TAG, "%s", log_buffer);
                break;
            case ESP_LOG_INFO:
                ESP_LOGI(TAG, "%s", log_buffer);
                break;
            case ESP_LOG_VERBOSE:
                ESP_LOGV(TAG, "%s", log_buffer);
                break;
            case ESP_LOG_NONE:
                //do nothing
                break;
            }
        }

        va_end(va_args);
    }
#endif
#endif

