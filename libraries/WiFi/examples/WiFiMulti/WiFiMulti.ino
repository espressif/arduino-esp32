/*
 *  This sketch trys to Connect to the best AP based on a given list
 *
 */

#include <WiFi.h>
#include <WiFiMulti.h>

WiFiMulti wifiMulti;

void setup()
{
    Serial.begin(115200);
    delay(10);

    wifiMulti.addAP("ssid_from_AP_1", "your_password_for_AP_1");
    wifiMulti.addAP("ssid_from_AP_2", "your_password_for_AP_2");
    wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");

    Serial.println("Connecting Wifi...");

/*
 * wifiMulti.run() with no arguments defaults to
 * a connectTimeout of 5000 ms and scanHidden of false.
 * These defaults can be found in ../../src/WiFiMulti.h
 * 
 * The default of wifiMulti.run() will scan for the AP's
 * added above and will not locate SSIDs that are hidden.
 *
 * To find both visible and hidden SSIDs, one makes the 
 * call as follows: wifiMulti.run(5000,true) which will
 * scan and connect to visible and hidden SSIDs if the
 * password credentials match.
 *
 */

    /* scan and connect to visible APs */
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
