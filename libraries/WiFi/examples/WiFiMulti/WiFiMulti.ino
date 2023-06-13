/*
 *  This sketch tries to Connect to the best AP based on a given list
 *
 */

#include <WiFi.h>
#include <WiFiMulti.h>
#include "secrets.h"

WiFiMulti wifiMulti;

void setup()
{
    Serial.begin(115200);
    delay(10);

    for(int i = 0; i < SECRETS_WIFI_ARRAY_MAX; ++i){
        wifiMulti.addAP(SECRET_WIFI_SSID_ARRAY[i], SECRET_WIFI_PASSWORD_ARRAY[i]);
    }
    Serial.println("Connecting Wifi...");
    if(wifiMulti.run() == WL_CONNECTED) {
        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    }
}

void loop()
{
    if(wifiMulti.run() != WL_CONNECTED) {
        Serial.println("WiFi not connected!");
        delay(1000);
    }
}