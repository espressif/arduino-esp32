//This example demonstrate the Rainmaker Switch example
#include "RMaker.h"
#include "WiFi.h"

static esp_err_t switch_callback(const char *dev_name, const char *param_name, Param_t val, void *priv_data)
{
    if (strcmp(param_name, "power") == 0) {
//      log_i("Received value = %s for %s - %s", val.val.b? "true" : "false", dev_name, param_name);
        RMaker.updateParam(dev_name, param_name, val);
    }
    return ESP_OK;
}

void setup()
{
    Serial.begin(115200);
    WiFi.begin(); 
    
    RMaker.rmakerInit("ESP Rainmaker Device", "Switch");
    RMaker.createDevice("Switch", ESP_RMAKER_DEVICE_SWITCH, switch_callback, NULL);
    RMaker.deviceAddNameParam("Switch", "name");
    RMaker.deviceAddPowerParam("Switch", "power", true);
    RMaker.deviceAssignPrimaryParam("Switch", "power"); 
    RMaker.otaEnable(OTA_USING_PARAMS);
    RMaker.wifiProvision();
}

void loop()
{
}
