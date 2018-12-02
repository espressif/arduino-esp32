/*
 *  This sketch shows the WiFi event usage
 *
*/

/*
* WiFi Events

0  SYSTEM_EVENT_WIFI_READY               < ESP32 WiFi ready
1  SYSTEM_EVENT_SCAN_DONE                < ESP32 finish scanning AP
2  SYSTEM_EVENT_STA_START                < ESP32 station start
3  SYSTEM_EVENT_STA_STOP                 < ESP32 station stop
4  SYSTEM_EVENT_STA_CONNECTED            < ESP32 station connected to AP
5  SYSTEM_EVENT_STA_DISCONNECTED         < ESP32 station disconnected from AP
6  SYSTEM_EVENT_STA_AUTHMODE_CHANGE      < the auth mode of AP connected by ESP32 station changed
7  SYSTEM_EVENT_STA_GOT_IP               < ESP32 station got IP from connected AP
8  SYSTEM_EVENT_STA_LOST_IP              < ESP32 station lost IP and the IP is reset to 0
9  SYSTEM_EVENT_STA_WPS_ER_SUCCESS       < ESP32 station wps succeeds in enrollee mode
10 SYSTEM_EVENT_STA_WPS_ER_FAILED        < ESP32 station wps fails in enrollee mode
11 SYSTEM_EVENT_STA_WPS_ER_TIMEOUT       < ESP32 station wps timeout in enrollee mode
12 SYSTEM_EVENT_STA_WPS_ER_PIN           < ESP32 station wps pin code in enrollee mode
13 SYSTEM_EVENT_AP_START                 < ESP32 soft-AP start
14 SYSTEM_EVENT_AP_STOP                  < ESP32 soft-AP stop
15 SYSTEM_EVENT_AP_STACONNECTED          < a station connected to ESP32 soft-AP
16 SYSTEM_EVENT_AP_STADISCONNECTED       < a station disconnected from ESP32 soft-AP
17 SYSTEM_EVENT_AP_STAIPASSIGNED         < ESP32 soft-AP assign an IP to a connected station
18 SYSTEM_EVENT_AP_PROBEREQRECVED        < Receive probe request packet in soft-AP interface
19 SYSTEM_EVENT_GOT_IP6                  < ESP32 station or ap or ethernet interface v6IP addr is preferred
20 SYSTEM_EVENT_ETH_START                < ESP32 ethernet start
21 SYSTEM_EVENT_ETH_STOP                 < ESP32 ethernet stop
22 SYSTEM_EVENT_ETH_CONNECTED            < ESP32 ethernet phy link up
23 SYSTEM_EVENT_ETH_DISCONNECTED         < ESP32 ethernet phy link down
24 SYSTEM_EVENT_ETH_GOT_IP               < ESP32 ethernet got IP from connected AP
25 SYSTEM_EVENT_MAX
*/

#include <WiFi.h>

const char* ssid     = "your-ssid";
const char* password = "your-password";


void WiFiEvent(WiFiEvent_t event)
{
    Serial.printf("[WiFi-event] event: %d\n", event);

    switch (event) {
        case SYSTEM_EVENT_WIFI_READY: 
            Serial.println("WiFi interface ready");
            break;
        case SYSTEM_EVENT_SCAN_DONE:
            Serial.println("Completed scan for access points");
            break;
        case SYSTEM_EVENT_STA_START:
            Serial.println("WiFi client started");
            break;
        case SYSTEM_EVENT_STA_STOP:
            Serial.println("WiFi clients stopped");
            break;
        case SYSTEM_EVENT_STA_CONNECTED:
            Serial.println("Connected to access point");
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            Serial.println("Disconnected from WiFi access point");
            break;
        case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
            Serial.println("Authentication mode of access point has changed");
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            Serial.print("Obtained IP address: ");
            Serial.println(WiFi.localIP());
            break;
        case SYSTEM_EVENT_STA_LOST_IP:
            Serial.println("Lost IP address and IP address is reset to 0");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
            Serial.println("WiFi Protected Setup (WPS): succeeded in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_FAILED:
            Serial.println("WiFi Protected Setup (WPS): failed in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
            Serial.println("WiFi Protected Setup (WPS): timeout in enrollee mode");
            break;
        case SYSTEM_EVENT_STA_WPS_ER_PIN:
            Serial.println("WiFi Protected Setup (WPS): pin code in enrollee mode");
            break;
        case SYSTEM_EVENT_AP_START:
            Serial.println("WiFi access point started");
            break;
        case SYSTEM_EVENT_AP_STOP:
            Serial.println("WiFi access point  stopped");
            break;
        case SYSTEM_EVENT_AP_STACONNECTED:
            Serial.println("Client connected");
            break;
        case SYSTEM_EVENT_AP_STADISCONNECTED:
            Serial.println("Client disconnected");
            break;
        case SYSTEM_EVENT_AP_STAIPASSIGNED:
            Serial.println("Assigned IP address to client");
            break;
        case SYSTEM_EVENT_AP_PROBEREQRECVED:
            Serial.println("Received probe request");
            break;
        case SYSTEM_EVENT_GOT_IP6:
            Serial.println("IPv6 is preferred");
            break;
        case SYSTEM_EVENT_ETH_START:
            Serial.println("Ethernet started");
            break;
        case SYSTEM_EVENT_ETH_STOP:
            Serial.println("Ethernet stopped");
            break;
        case SYSTEM_EVENT_ETH_CONNECTED:
            Serial.println("Ethernet connected");
            break;
        case SYSTEM_EVENT_ETH_DISCONNECTED:
            Serial.println("Ethernet disconnected");
            break;
        case SYSTEM_EVENT_ETH_GOT_IP:
            Serial.println("Obtained IP address");
            break;
    }}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
}

void setup()
{
    Serial.begin(115200);

    // delete old config
    WiFi.disconnect(true);

    delay(1000);

    // Examples of different ways to register wifi events
    WiFi.onEvent(WiFiEvent);
    WiFi.onEvent(WiFiGotIP, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
    WiFiEventId_t eventID = WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info){
        Serial.print("WiFi lost connection. Reason: ");
        Serial.println(info.disconnected.reason);
    }, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);

    // Remove WiFi event
    Serial.print("WiFi Event ID: ");
    Serial.println(eventID);
    // WiFi.removeEvent(eventID);

    WiFi.begin(ssid, password);

    Serial.println();
    Serial.println();
    Serial.println("Wait for WiFi... ");
}

void loop()
{
    delay(1000);
}
