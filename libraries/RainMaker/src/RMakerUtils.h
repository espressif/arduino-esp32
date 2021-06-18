#include "esp_system.h"
#if ESP_IDF_VERSION_MAJOR >= 4 && CONFIG_ESP_RMAKER_TASK_STACK && CONFIG_IDF_TARGET_ESP32

#include <esp_rmaker_utils.h>

static void RMakerFactoryReset(int seconds)
{
    esp_rmaker_factory_reset(seconds);
}

static void RMakerWiFiReset(int seconds)
{
    esp_rmaker_wifi_reset(seconds);
}

#endif
