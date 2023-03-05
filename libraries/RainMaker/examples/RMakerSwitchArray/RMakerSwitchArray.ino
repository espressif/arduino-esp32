// This example demonstrates the ESP RainMaker with a standard light switch device.
#include "RMaker.h"
#include "WiFi.h"
#include "WiFiProv.h"
#include <Preferences.h>
#include <AceButton.h>

#define NUM_RELAYS 8

#if CONFIG_IDF_TARGET_ESP32C3
  const byte resetPin = 9;
#else
  const byte resetPin = 0;
#endif

const char nodeName[] = "ESP32_Relay_16";
const char *service_name = "PROV_1234";
const char *pop = "abcd1234";
bool firstPress = true;

Preferences pref;

using namespace ace_button;

// Define the buttons in an array using the default constructor.
AceButton lightSwitch[NUM_RELAYS];

// The framework provides some standard device types like switch, lightbulb, fan, temperaturesensor.
static Switch myRelay[NUM_RELAYS];

struct INFOS
{
  const char *deviceName;
  uint8_t relayPin;
  bool relayState;
  uint8_t switchPin;
};

#define INITIAL_STATE LOW  // Initial state = HIGH to active LOW relay model.

// Define the device name, relay pin, relay default status and switch pin to an array.
INFOS info[NUM_RELAYS] = {
  { "Relay1", 4, INITIAL_STATE, 26},
  { "Relay2", 5, INITIAL_STATE, 27},
  { "Relay3", 18, INITIAL_STATE, 32},
  { "Relay4", 19, INITIAL_STATE, 33},
  { "Relay5", 21, INITIAL_STATE, 13},
  { "Relay6", 22, INITIAL_STATE, 12},
  { "Relay7", 23, INITIAL_STATE, 14},
  { "Relay8", 25, INITIAL_STATE, 15}
};

void sysProvEvent(arduino_event_t *sys_event)
{
  switch (sys_event->event_id)
  {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      {
        Serial.print("\nConnected IP address : ");
        Serial.println(IPAddress(sys_event->event_info.got_ip.ip_info.ip.addr));
        break;
      }
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      {
        Serial.println("\nDisconnected. Connecting to the AP again... ");
        break;
      }
    case ARDUINO_EVENT_PROV_START:
      {
        #if CONFIG_IDF_TARGET_ESP32S2
          Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on SoftAP\n", service_name, pop);
          printQR(service_name, pop, "softap");
        #else
          Serial.printf("\nProvisioning Started with name \"%s\" and PoP \"%s\" on BLE\n", service_name, pop);
          printQR(service_name, pop, "ble");
        #endif
        break;
      }
    case ARDUINO_EVENT_PROV_INIT:
      {
        wifi_prov_mgr_disable_auto_stop(10000);
        break;
      }
    case ARDUINO_EVENT_PROV_CRED_SUCCESS:
      {
        wifi_prov_mgr_stop_provisioning();
        break;
      }
    case ARDUINO_EVENT_PROV_CRED_RECV:
      {
        Serial.println("\nReceived Wi-Fi credentials");
        Serial.print("\tSSID : ");
        Serial.println((const char *) sys_event->event_info.prov_cred_recv.ssid);
        Serial.print("\tPassword : ");
        Serial.println((char const *) sys_event->event_info.prov_cred_recv.password);
        break;
      }
    case ARDUINO_EVENT_PROV_CRED_FAIL:
      {
        Serial.println("\nProvisioning failed!\nPlease reset to factory and retry provisioning\n");
        if(sys_event->event_info.prov_fail_reason == WIFI_PROV_STA_AUTH_ERROR)
        {
          Serial.println("\nWi-Fi AP password incorrect");
        }
        else
        {
          Serial.println("\nWi-Fi AP not found....Add API \" nvs_flash_erase() \" before beginProvision()");
        }
        break;
      }
    case ARDUINO_EVENT_PROV_CRED_SUCCESS:
      {
        Serial.println("\nProvisioning Successful");
        break;
      }
    case ARDUINO_EVENT_PROV_END:
      {
        Serial.println("\nProvisioning Ends");
        break;
      }
    default:
      {
        break;
      }
  }
}

void write_callback(Device *device, Param *param, const param_val_t val, void *priv_data, write_ctx_t *ctx)
{
  const char *device_name = device->getDeviceName();
  const char *param_name = param->getParamName();

  if (strcmp(param_name, ESP_RMAKER_DEF_POWER_NAME) == 0)
  {
    for (byte i = 0; i < NUM_RELAYS; i++)
    {
      if (strcmp(device_name, info[i].deviceName) == 0)
      {
        info[i].relayState = val.val.b;
        digitalWrite(info[i].relayPin, info[i].relayState);

        // Publish the new state to all listeners
        myRelay[i].updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, info[i].relayState);

        // Save the relay state.
        pref.putBool(info[i].deviceName, info[i].relayState);
        break;
      }
    }
  }
}

void getLastState()
{
  for (byte i = 0; i < NUM_RELAYS; i++)
  {
    info[i].relayState = pref.getBool(info[i].deviceName, info[i].relayState);
    digitalWrite(info[i].relayPin, info[i].relayState);
    myRelay[i].updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, info[i].relayState);
  }
}

void handleEvent(AceButton* button, uint8_t eventType, uint8_t buttonState)
{
  uint8_t numSwitch = button->getId();

  switch (eventType)
  {
    case AceButton::kEventPressed:
      {
        info[numSwitch].relayState = !info[numSwitch].relayState;
        Serial.print(F("Light switch "));
        Serial.print(numSwitch + 1);
        Serial.print(F(" pressed, relayState: "));
        Serial.println(info[numSwitch].relayState);
        digitalWrite(info[numSwitch].relayPin, info[numSwitch].relayState);
        myRelay[numSwitch].updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, info[numSwitch].relayState);
        pref.putBool(info[numSwitch].deviceName, info[numSwitch].relayState);
        break;
      }
    case AceButton::kEventReleased:
      {
        info[numSwitch].relayState = !info[numSwitch].relayState;
        Serial.print(F("Light switch "));
        Serial.print(numSwitch + 1);
        Serial.print(F(" released, relayState: "));
        Serial.println(info[numSwitch].relayState);
        digitalWrite(info[numSwitch].relayPin, info[numSwitch].relayState);
        myRelay[numSwitch].updateAndReportParam(ESP_RMAKER_DEF_POWER_NAME, info[numSwitch].relayState);
        pref.putBool(info[numSwitch].deviceName, info[numSwitch].relayState);
        break;
      }
  }
}

void setup()
{
  Serial.begin(115200);

  pinMode(resetPin, INPUT);

  // Starts the configuration file where the state will be saved.
  pref.begin(nodeName, false);

  // Configure the ButtonConfig with the event handler, enable click and long press events.
  ButtonConfig* buttonConfig = ButtonConfig::getSystemButtonConfig();

  buttonConfig->setEventHandler(handleEvent);
  buttonConfig->setFeature(ButtonConfig::kFeatureClick);
  buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);

  Node my_node;
  my_node = RMaker.initNode(nodeName);

  for (byte i = 0; i < NUM_RELAYS; i++)
  {
    pinMode(info[i].relayPin, OUTPUT);

    // Turn off all relays.
    digitalWrite(info[i].relayPin, info[i].relayState);

    // Initialize switch device
    myRelay[i] = Switch(info[i].deviceName);

    // Specify the callback function to relays.
    myRelay[i].addCb(write_callback);

    // Add switch device to the node.
    my_node.addDevice(myRelay[i]);

    // Switch uses the built-in pull up resistor.
    pinMode(info[i].switchPin, INPUT_PULLUP);

    // initialize the corresponding AceButton.
    lightSwitch[i].init(info[i].switchPin, HIGH, i);
  }

  // This is optional
  RMaker.enableOTA(OTA_USING_TOPICS);
  // If you want to enable scheduling, set time zone for your region using setTimeZone().
  // The list of available values are provided here https://rainmaker.espressif.com/docs/time-service.html
  // RMaker.setTimeZone("Asia/Shanghai");
  // Alternatively, enable the Timezone service and let the phone apps set the appropriate timezone
  RMaker.enableTZService();

  RMaker.enableSchedule();

  RMaker.enableScenes();

  RMaker.start();

  WiFi.onEvent(sysProvEvent);
  #if CONFIG_IDF_TARGET_ESP32S2
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP, WIFI_PROV_SCHEME_HANDLER_NONE, WIFI_PROV_SECURITY_1, pop, service_name);
  #else
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_FREE_BTDM, WIFI_PROV_SECURITY_1, pop, service_name);
  #endif

  // Get the last state of Relays.
  getLastState();
}

void loop()
{
  for (int i = 0; i < NUM_RELAYS; i++)
  {
    lightSwitch[i].check();
  }

  // Push button pressed.
  if(digitalRead(resetPin) == LOW)
  {
    // Key debounce handling.
    delay(100);
    int startTime = millis();
    while(digitalRead(resetPin) == LOW)
    {
      delay(50);
    }
    int endTime = millis();

    // If key pressed for more than 10secs, reset all.
    if ((endTime - startTime) > 10000)
    {
      Serial.printf("Factory reset!");
      RMakerFactoryReset(2);
    }
  }
}
