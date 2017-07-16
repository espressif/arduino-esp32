#include <Ethernet.h>
#include "WiFi.h"

// declare the ethernet adapter
ESP32Ethernet ethernet;

// define the network settings
IPAddress ip(192, 168, 0, 123);
IPAddress nm(255, 255, 255, 0);
IPAddress gw(192, 168, 0, 1);

void WiFiEvent(WiFiEvent_t event){
    switch(event) {
        case SYSTEM_EVENT_ETH_START:
            //set eth hostname here
            ethernet.setHostname("esp32-eth");
            Serial.println("ETH started");
            Serial.print("ETH MAC: ");
            Serial.println(ethernet.getMacAddress());
            break;
        case SYSTEM_EVENT_ETH_CONNECTED:
            Serial.println("ETH connected");
            break;
        case SYSTEM_EVENT_ETH_GOT_IP:
            Serial.print("ETH IPv4: ");
            Serial.println(ethernet.localIP());
            break;
        case SYSTEM_EVENT_ETH_DISCONNECTED:
            Serial.println("ETH disconnected");
            break;
        default:
            break;
    }
}

void setup(){
  // start the serial console 
  Serial.begin(115200);
  // attach the callback event handler
  WiFi.onEvent(WiFiEvent);
  // start the ethernet
  ethernet.begin(ip,nm,gw);
}

void loop(){
  // nothing yet to do here
}

