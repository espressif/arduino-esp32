#include "RMakerUtils.h"
#ifdef CONFIG_ESP_RMAKER_WORK_QUEUE_TASK_STACK
void RMakerFactoryReset(int seconds)
{
    esp_rmaker_factory_reset(0, seconds);
}

void RMakerWiFiReset(int seconds)
{
    esp_rmaker_wifi_reset(0, seconds);
}
#endif