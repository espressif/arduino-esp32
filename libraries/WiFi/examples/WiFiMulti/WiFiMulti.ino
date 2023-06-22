/*
 *  This sketch tries to Connect to the best AP based on a given list
 *
 */

#include <WiFi.h>
#include <WiFiMulti.h>

// To use secrets please read the documentation at https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/guides/secrets.html
#if __has_include("secrets.h")
  #include "secrets.h"
#endif

WiFiMulti wifiMulti;

void setup()
{
    Serial.begin(115200);
    delay(10);

#ifdef SECRETS_WIFI_ARRAY_MAX
    for(int i = 0; i < SECRETS_WIFI_ARRAY_MAX; ++i){
        wifiMulti.addAP(SECRET_WIFI_SSID_ARRAY[i], SECRET_WIFI_PASSWORD_ARRAY[i]);
    }
#else
    // Traditional way
    wifiMulti.addAP("example-SSID1", "example-password-1");
    wifiMulti.addAP("example-SSID2", "example-password-2");
    wifiMulti.addAP("example-SSID3", "example-password-3");
#endif

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