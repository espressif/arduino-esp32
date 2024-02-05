#include "Insights.h"
#include "WiFi.h"

const char insights_auth_key[] = "<ENTER YOUR AUTH KEY>";

#define WIFI_SSID       "<ENTER YOUR SSID>"
#define WIFI_PASSPHRASE "<ENTER YOUR PASSWORD>"

void setup()
{
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSPHRASE);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    if(!Insights.begin(insights_auth_key)){
        return;
    }
    Serial.println("=========================================");
    Serial.printf("ESP Insights enabled Node ID %s\n", Insights.nodeID());
    Serial.println("=========================================");
}

void loop()
{
    delay(1000);
}
