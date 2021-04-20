#include "RMaker.h"
#if ESP_IDF_VERSION_MAJOR >= 4 && CONFIG_IDF_TARGET_ESP32
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_utils.h>
bool wifiLowLevelInit(bool persistent);
static esp_err_t err;

static void event_handler(void *arg, esp_event_base_t event_base, int event_id, void *event_data)
{
    if (event_base == RMAKER_EVENT) {
        switch (event_id) {
            case RMAKER_EVENT_INIT_DONE:
                log_i("RainMaker Initialised.");
                break;
            case RMAKER_EVENT_CLAIM_STARTED:
                log_i("RainMaker Claim Started.");
                break;
            case RMAKER_EVENT_CLAIM_SUCCESSFUL:
                log_i("RainMaker Claim Successful.");
                break;
            case RMAKER_EVENT_CLAIM_FAILED:
                log_i("RainMaker Claim Failed.");
                break;
            default:
                log_i("Unhandled RainMaker Event:");
        }
    }
}

void RMakerClass::setTimeSync(bool val)
{
    rainmaker_cfg.enable_time_sync = val;
}

Node RMakerClass::initNode(const char *name, const char *type)
{
    wifiLowLevelInit(true);
    Node node;
    esp_rmaker_node_t *rnode = NULL;
    esp_event_handler_register(RMAKER_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);
    rnode = esp_rmaker_node_init(&rainmaker_cfg, name, type);
    if (!rnode){
        log_e("Node init failed");
        return node;
    }
    node.setNodeHandle(rnode);
    return node;
}

esp_err_t RMakerClass::start()
{
    err = esp_rmaker_start();    
    if(err != ESP_OK){
        log_e("ESP RainMaker core task failed");
    }
    return err;
}

esp_err_t RMakerClass::stop()
{
    err = esp_rmaker_stop();
    if(err != ESP_OK) {
        log_e("ESP RainMaker stop error");
    }
    return err;
}

esp_err_t RMakerClass::deinitNode(Node rnode)
{
    err = esp_rmaker_node_deinit(rnode.getNodeHandle());
    if(err != ESP_OK) {
        log_e("Node deinit failed");
    }
    return err;
}

esp_err_t RMakerClass::setTimeZone(const char *tz)
{
    err = esp_rmaker_time_set_timezone(tz);
    if(err != ESP_OK) {
        log_e("Setting time zone error");
    }
    return err;
}

esp_err_t RMakerClass::enableSchedule()
{
    err = esp_rmaker_schedule_enable();
    if(err != ESP_OK) {
        log_e("Schedule enable failed");
    }
    return err;
}

esp_err_t RMakerClass::enableTZService()
{
    err = esp_rmaker_timezone_service_enable();
    if(err != ESP_OK) {
        log_e("Timezone service enable failed");
    }
    return err;
}

esp_err_t RMakerClass::enableOTA(ota_type_t type, const char *cert)
{
    esp_rmaker_ota_config_t ota_config;
    ota_config.server_cert = cert;
    err = esp_rmaker_ota_enable(&ota_config, type);
    if(err != ESP_OK) {
        log_e("OTA enable failed");
    }
    return err;
}

RMakerClass RMaker;
#endif
