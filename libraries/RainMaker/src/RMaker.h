#include "esp_system.h"
#if ESP_IDF_VERSION_MAJOR >= 4 && CONFIG_IDF_TARGET_ESP32

#include "Arduino.h"
#include "RMakerNode.h"
#include "RMakerQR.h"
#include "RMakerUtils.h"
#include <esp_rmaker_standard_types.h>

class RMakerClass
{
    private:
        esp_rmaker_config_t rainmaker_cfg = {false};
      
    public:
    
        void setTimeSync(bool val);
        Node initNode(const char *name, const char *type = "ESP RainMaker with Arduino");
        esp_err_t deinitNode(Node node);
        esp_err_t setTimeZone(const char *tz = "Asia/Shanghai");
        esp_err_t enableSchedule();
        esp_err_t enableTZService();
        esp_err_t enableOTA(ota_type_t type, const char *cert = ESP_RMAKER_OTA_DEFAULT_SERVER_CERT);
        esp_err_t start();
        esp_err_t stop();
};

extern RMakerClass RMaker;

#endif
