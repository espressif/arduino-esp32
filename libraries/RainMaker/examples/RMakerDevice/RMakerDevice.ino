//This example demonstrates the ESP RainMaker with the custom device and standard Switch device.
#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"

#define DEFAULT_FAN_SPEED 4
#define DEFAULT_POWER_MODE true
const char *service_name = "Prov_1234";
const char *pop = "abcd1234";

//GPIO for push button
static int gpio_0 = 0;
//GPIO for virtual device
static int gpio_switch = 16;
static int gpio_fan = 17;
/* Variable for reading pin status*/
bool switch_state = true;
bool fan_state = true;

//The framework provides some standard device types like switch, lightbulb, fan, temperaturesensor.
static Switch my_switch("Switch", &gpio_switch);
//You can also define custom devices using the 'Device' base class object, as shown here
static Device my_device("Fan", ESP_RMAKER_DEVICE_FAN, &gpio_fan);

void sysProvEvent(arduino_event_t *sys_event)
{
    switch (sys_event->event_id) {      
        case ARDUINO_EVENT_PROV_START:
#if CONFIG_IDF_TARGET_ESP32
        printQR(service_name, pop, "ble");
#else
        printQR(service_name, pop, "softap");
#endif        
        break;
    }
}

void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx)
{
    const char *device_name = device->getDeviceName();
    const char *param_name = param->getParamName();

    if(strcmp(param_name, "Power") == 0) {
        Serial.printf("\nReceived value = %s for %s - %s\n", val.val.b? "true" : "false", device_name, param_name);
        if(strcmp(device_name, "Switch") == 0) {
            switch_state = val.val.b;
            (switch_state == false) ? digitalWrite(gpio_switch, LOW) : digitalWrite(gpio_switch, HIGH);
        }
        if(strcmp(device_name, "Fan") == 0) {
            fan_state = val.val.b;
            (fan_state == false) ? digitalWrite(gpio_fan, LOW) : digitalWrite(gpio_fan, HIGH);
        }
    }
    else if(strcmp(param_name, "Speed") == 0) {
        Serial.printf("\nReceived value = %d for %s - %s\n", val.val.i, device_name, param_name);
    }
    param->updateAndReport(val);
}

void setup()
{
    Serial.begin(115200);
    pinMode(gpio_0, INPUT);
    pinMode(gpio_switch, OUTPUT);
    pinMode(gpio_fan, OUTPUT);

    Node my_node;    
    my_node = RMaker.initNode("ESP RainMaker Node");

    //Standard switch device
    my_switch.addCb(write_callback);
    
    //Creating custom fan device
    my_device.addNameParam();
    my_device.addPowerParam(DEFAULT_POWER_MODE);
    my_device.addSpeedParam(DEFAULT_FAN_SPEED);
    my_device.assignPrimaryParam(my_device.getParamByName(ESP_RMAKER_DEF_POWER_NAME));
    my_device.addCb(write_callback);
    
    //Add switch and fan device to the node   
    my_node.addDevice(my_switch);
    my_node.addDevice(my_device);

    //This is optional 
    RMaker.enableOTA(OTA_USING_PARAMS);
    //If you want to enable scheduling, set time zone for your region using setTimeZone(). 
    //The list of available values are provided here https://rainmaker.espressif.com/docs/time-service.html
    RMaker.setTimeZone("Asia/Shanghai");
    RMaker.enableSchedule();

    RMaker.start();

    WiFi.onEvent(sysProvEvent);
#if CONFIG_IDF_TARGET_ESP32
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_FREE_BTDM, WIFI_PROV_SECURITY_1, pop, service_name);
#else
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP, WIFI_PROV_SCHEME_HANDLER_NONE, WIFI_PROV_SECURITY_1, pop, service_name);
#endif

}

void loop()
{
    if(digitalRead(gpio_0) == LOW) { //Push button 
        switch_state = !switch_state;
        fan_state = !fan_state;
        my_switch.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, switch_state);
        my_device.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, fan_state);
        (switch_state == false) ? digitalWrite(gpio_switch, LOW) : digitalWrite(gpio_switch, HIGH);
        (fan_state == false) ? digitalWrite(gpio_fan, LOW) : digitalWrite(gpio_fan, HIGH); 
    }
    delay(100);
}
