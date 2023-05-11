#include "sdkconfig.h"
#ifdef CONFIG_ESP_RMAKER_WORK_QUEUE_TASK_STACK
#include "RMaker.h"
#include <esp_rmaker_schedule.h>
#include <esp_rmaker_utils.h>
#include <esp_rmaker_scenes.h>
bool wifiLowLevelInit(bool persistent);
static esp_err_t err;

extern "C" bool verifyRollbackLater() { return true; }

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
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
    } else if (event_base == RMAKER_OTA_EVENT) {
        if (event_data == NULL) {
            event_data = (void*)"";
        }
        switch(event_id) {
            case RMAKER_OTA_EVENT_STARTING:
                log_i("Starting OTA");
                break;
            case RMAKER_OTA_EVENT_IN_PROGRESS:
                log_i("OTA in progress : %s", (char*)event_data);
                break;
            case RMAKER_OTA_EVENT_SUCCESSFUL:
                log_i("OTA Successful : %s", (char*)event_data);
                break;
            case RMAKER_OTA_EVENT_FAILED:
                log_i("OTA Failed : %s", (char*)event_data);
                break;
            case RMAKER_OTA_EVENT_DELAYED:
                log_i("OTA Delayed : %s", (char*)event_data);
                break;
            case RMAKER_OTA_EVENT_REJECTED:
                log_i("OTA Rejected : %s", (char*)event_data);
                break;
            default:
                log_i("Unhandled OTA Event");
                break;
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
    esp_event_handler_register(RMAKER_OTA_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL);
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
    esp_rmaker_ota_config_t ota_config = {
        .ota_cb = NULL,
        .ota_diag = NULL,
        .server_cert = cert,
    };
    err = esp_rmaker_ota_enable(&ota_config, type);
    if(err != ESP_OK) {
        log_e("OTA enable failed");
    }
    return err;
}

esp_err_t RMakerClass::enableScenes()
{
    err = esp_rmaker_scenes_enable();
    if (err != ESP_OK) {
        log_e("Scenes enable failed");
    }
    return err;
}

esp_err_t RMakerClass::enableSystemService(uint16_t flags, int8_t reboot_seconds, int8_t reset_seconds, int8_t reset_reboot_seconds)
{
    esp_rmaker_system_serv_config_t config = {
        .flags = flags,
        .reboot_seconds = reboot_seconds,
        .reset_seconds = reset_seconds,
        .reset_reboot_seconds = reset_reboot_seconds
    };
    err = esp_rmaker_system_service_enable(&config);
    return err;    
}

RMakerClass RMaker;
#endif
