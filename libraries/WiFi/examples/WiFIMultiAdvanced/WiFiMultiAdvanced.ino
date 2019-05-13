/*
 *  This sketch tries to connect to the best AP available, and tests for captive portals on open networks
 *
 */

#include <WiFi.h>
#include <WiFiMulti.h>

WiFiMulti wifiMulti;

bool bConnected = false;

void setup()
{
    Serial.begin(115200);
    delay(10);

    wifiMulti.addAP("ssid_from_AP_1", "your_password_for_AP_1");
    wifiMulti.addAP("ssid_from_AP_2", "your_password_for_AP_2");
    wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");

    // These options can help when you need ANY kind of wifi connection to get a config file, report errors, etc.
    wifiMulti.setStrictMode(false);  // Default is true.  Library will disconnect and forget currently connected AP if it's not in the AP list.
    wifiMulti.setAllowOpenAP(true);    // Default is false.  True adds open APs to the list.
    wifiMulti.setTestConnection(true);  // Default is false.  Attempts to connect to a webserver in case of captive portals.  Most useful with AllowOpenAP = true.

    // Optional - defaults to brainjar, but you should set this to a simple test page on your own server to ensure you can reach it
    // wifiMulti.setTestURL("http://www.brainjar.com/java/host/test.html");  // Must include http://
    // wifiMulti.setTestPhrase("This is a very simple HTML file.");  // Unique but short is best
    

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
    if( wifiMulti.run() != WL_CONNECTED) {
        Serial.println("WiFi not connected!");
        delay(1000);
    }
}