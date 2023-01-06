//This example demonstrates the ESP RainMaker with a custom Air Cooler device
#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"
#include "led_strip.h"

#define DEFAULT_POWER_MODE true
#define DEFAULT_SWING false
#define DEFAULT_SPEED 0
#define DEFAULT_MODE "Auto"

const char *service_name = "PROV_1234";
const char *pop = "abcd1234";

#if CONFIG_IDF_TARGET_ESP32C3
//GPIO for push button
static int gpio_reset = 9;
//GPIO for virtual device
static int gpio_power = 7;
static int gpio_swing = 3;
static int gpio_mode_auto = 4;
static int gpio_mode_cool = 5;
static int gpio_mode_heat = 6;
static int gpio_speed = 10;

#else
//GPIO for push button
static int gpio_reset = 0;
//GPIO for virtual device
static int gpio_power = 16;
static int gpio_swing = 17;
static int gpio_mode_auto = 18;
static int gpio_mode_cool = 19;
static int gpio_mode_heat = 21;
static int gpio_speed = 22;
#endif

bool power_state = true;

// The framework provides some standard device types like switch, lightbulb, fan, temperature sensor.
// But, you can also define custom devices using the 'Device' base class object, as shown here
static Device my_device("Air Cooler", "my.device.air-cooler", NULL);

void sysProvEvent(arduino_event_t *sys_event)
{
    switch (sys_event->event_id) {
        case ARDUINO_EVENT_PROV_START:
#if CONFIG_IDF_TARGET_ESP32S2
            Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on SoftAP\n", service_name, pop);
            printQR(service_name, pop, "softap");
#else
            Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on BLE\n", service_name, pop);
            printQR(service_name, pop, "ble");
#endif
            break;
        case ARDUINO_EVENT_PROV_INIT:
            wifi_prov_mgr_disable_auto_stop(10000);
            break;
        case ARDUINO_EVENT_PROV_CRED_SUCCESS:
            wifi_prov_mgr_stop_provisioning();
            break;
        default:;
    }
}

void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx)
{
    const char *device_name = device->getDeviceName();
    const char *param_name = param->getParamName();

    if(strcmp(param_name, "Power") == 0) {
        Serial.printf("Received value = %s for %s - %s\n", val.val.b? "true" : "false", device_name, param_name);
        power_state = val.val.b;
        (power_state == false) ? digitalWrite(gpio_power, LOW) : digitalWrite(gpio_power, HIGH);
        param->updateAndReport(val);
    } else if (strcmp(param_name, "Swing") == 0) {
        Serial.printf("\nReceived value = %s for %s - %s\n", val.val.b? "true" : "false", device_name, param_name);
        bool swing = val.val.b;
        (swing == false) ? digitalWrite(gpio_swing, LOW) : digitalWrite(gpio_swing, HIGH);
        param->updateAndReport(val);
    } else if (strcmp(param_name, "Speed") == 0) {
        Serial.printf("\nReceived value = %d for %s - %s\n", val.val.i, device_name, param_name);
        int speed = val.val.i;
        analogWrite(gpio_speed, speed);
        param->updateAndReport(val);
    } else if (strcmp(param_name, "Mode") == 0) {
        const char* mode = val.val.s;
        if (strcmp(mode, "Auto") == 0) {
            digitalWrite(gpio_mode_auto, HIGH);
            digitalWrite(gpio_mode_heat, LOW);
            digitalWrite(gpio_mode_cool, LOW);
        } else if (strcmp(mode, "Heat") == 0) {
            digitalWrite(gpio_mode_auto, LOW);
            digitalWrite(gpio_mode_heat, HIGH);
            digitalWrite(gpio_mode_cool, LOW);
        } else if (strcmp(mode, "Cool") == 0) {
            digitalWrite(gpio_mode_auto, LOW);
            digitalWrite(gpio_mode_heat, LOW);
            digitalWrite(gpio_mode_cool, HIGH);
        }
        Serial.printf("\nReceived value = %s for %s - %s\n", val.val.s, device_name, param_name);
        param->updateAndReport(val);
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(gpio_reset, INPUT_PULLUP);
    pinMode(gpio_power, OUTPUT);
    digitalWrite(gpio_power, DEFAULT_POWER_MODE);
    pinMode(gpio_swing, OUTPUT);
    digitalWrite(gpio_swing, DEFAULT_SWING);
    pinMode(gpio_mode_auto, OUTPUT);
    if (strcmp(DEFAULT_MODE, "Auto") == 0) digitalWrite(gpio_mode_auto, HIGH);
    pinMode(gpio_mode_cool, OUTPUT);
    if (strcmp(DEFAULT_MODE, "Cool") == 0) digitalWrite(gpio_mode_auto, HIGH);
    pinMode(gpio_mode_heat, OUTPUT);
    if (strcmp(DEFAULT_MODE, "Heat") == 0) digitalWrite(gpio_mode_auto, HIGH);
    pinMode(gpio_speed, OUTPUT);
    analogWrite(gpio_speed, DEFAULT_SPEED);

    Node my_node;
    my_node = RMaker.initNode("ESP RainMaker Node");

    //Create custom air cooler device
    my_device.addNameParam();
    my_device.addPowerParam(DEFAULT_POWER_MODE);
    my_device.assignPrimaryParam(my_device.getParamByName(ESP_RMAKER_DEF_POWER_NAME));

    Param swing("Swing", ESP_RMAKER_PARAM_TOGGLE, value(DEFAULT_SWING), PROP_FLAG_READ | PROP_FLAG_WRITE);
    swing.addUIType(ESP_RMAKER_UI_TOGGLE);
    my_device.addParam(swing);

    Param speed("Speed", ESP_RMAKER_PARAM_RANGE, value(DEFAULT_SPEED), PROP_FLAG_READ | PROP_FLAG_WRITE);
    speed.addUIType(ESP_RMAKER_UI_SLIDER);
    speed.addBounds(value(0), value(255), value(1));
    my_device.addParam(speed);

    static const char* modes[] = { "Auto", "Cool", "Heat" };
    Param mode_param("Mode", ESP_RMAKER_PARAM_MODE, value("Auto"), PROP_FLAG_READ | PROP_FLAG_WRITE);
    mode_param.addValidStrList(modes, 3);
    mode_param.addUIType(ESP_RMAKER_UI_DROPDOWN);
    my_device.addParam(mode_param);

    my_device.addCb(write_callback);

    //Add custom Air Cooler device to the node
    my_node.addDevice(my_device);

    //This is optional
    // RMaker.enableOTA(OTA_USING_TOPICS);
    //If you want to enable scheduling, set time zone for your region using setTimeZone().
    //The list of available values are provided here https://rainmaker.espressif.com/docs/time-service.html
    // RMaker.setTimeZone("Asia/Shanghai");
    //Alternatively, enable the Timezone service and let the phone apps set the appropriate timezone
    // RMaker.enableTZService();

    RMaker.enableSchedule();

    RMaker.enableScenes();

    RMaker.start();

    WiFi.onEvent(sysProvEvent);
#if CONFIG_IDF_TARGET_ESP32S2
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP, WIFI_PROV_SCHEME_HANDLER_NONE, WIFI_PROV_SECURITY_1, pop, service_name);
#else
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_FREE_BTDM, WIFI_PROV_SECURITY_1, pop, service_name);
#endif
}

void loop()
{
    if(digitalRead(gpio_reset) == LOW) { //Push button pressed

        // Key debounce handling
        delay(100);
        int startTime = millis();
        while(digitalRead(gpio_reset) == LOW) delay(50);
        int press_duration = millis() - startTime;

        if (press_duration > 10000) {
          // If key pressed for more than 10secs, reset all
          Serial.printf("Reset to factory.\n");
          RMakerFactoryReset(2);
        } else if (press_duration > 3000) {
          Serial.printf("Reset Wi-Fi.\n");
          // If key pressed for more than 3secs, but less than 10, reset Wi-Fi
          RMakerWiFiReset(2);
        } else {
          // Toggle device state
          power_state = !power_state;
          Serial.printf("Toggle power state to %s.\n", power_state ? "true" : "false");
          my_device.updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, power_state);
          (power_state == false) ? digitalWrite(gpio_power, LOW) : digitalWrite(gpio_power, HIGH);
      }
    }
    delay(100);
}
