/*
 *  This sketch tries to connect to the best AP available
 *  and tests for captive portals on open networks
 *
 */

#include <WiFi.h>
#include <WiFiMulti.h>

WiFiMulti wifiMulti;

// This is used to test the Internet connection of connected the AP
// Use a non-302 status code to ensure we bypass captive portals.  Can be any text in the webpage. 
String _testResp = "301 Moved"; // usually http:// is moves to https:// by a 301 code
// You can also set this to a simple test page on your own server to ensure you can reach it, 
// like "http://www.mysite.com/test.html"
String _testURL = "http://www.espressif.com"; // Must include "http://" if testing a HTTP host
const int _testPort = 80;  // HTTP port

bool testConnection(){
    //parse url
    int8_t split = _testURL.indexOf('/',7);
    String host = _testURL.substring(7, split);
    String url = (split < 0) ? "/":_testURL.substring(split,_testURL.length());
    log_i("Testing Connection to %s.  Test Respponse is \"%s\"",_testURL.c_str(), _testResp.c_str());
    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    if (!client.connect(host.c_str(), _testPort)) {
        log_e("Connection failed");
        return false;
    } else {
        log_i("Connected to test host");
    }

    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");
    unsigned long timeout = millis();
    while (client.available() == 0) {
        if (millis() - timeout > 5000) {
            log_e(">>>Client timeout!");
            client.stop();
            return false;
        }
    }

    bool bSuccess = false;
    timeout = millis();
    while(client.available()) {
        if (millis() - timeout < 5000) {
           String line = client.readStringUntil('\r');
           Serial.println("=============HTTP RESPONSE=============");
           Serial.print(line);
           Serial.println("\n=======================================");

            bSuccess = client.find(_testResp.c_str());
            if (bSuccess){
                log_i("Success. Found test response");
            } else {
                log_e("Failed.  Can't find test response");
            }
            return bSuccess;
        } else {
            log_e("Test Response checking has timed out!");
            break;
        }
    }
    return false; // timeout
}

void setup()
{
    Serial.begin(115200);
    delay(10);

    wifiMulti.addAP("ssid_from_AP_1", "your_password_for_AP_1");
    wifiMulti.addAP("ssid_from_AP_2", "your_password_for_AP_2");
    wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");

    // These options can help when you need ANY kind of wifi connection to get a config file, report errors, etc.
    wifiMulti.setStrictMode(false);  // Default is true.  Library will disconnect and forget currently connected AP if it's not in the AP list.
    wifiMulti.setAllowOpenAP(true);   // Default is false.  True adds open APs to the AP list.
    wifiMulti.setConnectionTestCallbackFunc(testConnection);   // Attempts to connect to a remote webserver in case of captive portals.

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
    static bool isConnected = false; 
    uint8_t WiFiStatus = wifiMulti.run();
    
    if (WiFiStatus == WL_CONNECTED) {
        if (!isConnected) {
            Serial.println("");
            Serial.println("WiFi connected");
            Serial.println("IP address: ");
            Serial.println(WiFi.localIP());
        }
        isConnected = true;
    } else {
        Serial.println("WiFi not connected!");
        isConnected = false; 
        delay(5000);
    }
}
