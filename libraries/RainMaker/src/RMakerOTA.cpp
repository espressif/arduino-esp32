#include "RMakerOTA.h"

esp_err_t RMakerOTAClass::otaEnable(OTAType_t type)
{
    esp_rmaker_ota_config_t ota_config;
    ota_config.server_cert = ESP_RMAKER_OTA_DEFAULT_SERVER_CERT;
    return esp_rmaker_ota_enable(&ota_config, type);
}
