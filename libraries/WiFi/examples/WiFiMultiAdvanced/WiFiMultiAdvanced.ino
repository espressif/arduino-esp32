/*
 *  This sketch tries to connect to the best AP available
 *  and tests for captive portals on open networks
 *
 */

#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>

WiFiMulti wifiMulti;

// callback used to check Internet connectivity
bool testConnection() {
  HTTPClient http;
  http.begin("http://www.espressif.com");
  int httpCode = http.GET();
  // we expect to get a 301 because it will ask to use HTTPS instead of HTTP
  if (httpCode == HTTP_CODE_MOVED_PERMANENTLY) return true;
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(10);

  wifiMulti.addAP("ssid_from_AP_1", "your_password_for_AP_1");
  wifiMulti.addAP("ssid_from_AP_2", "your_password_for_AP_2");
  wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3");

  // These options can help when you need ANY kind of wifi connection to get a config file, report errors, etc.
  wifiMulti.setStrictMode(false);                           // Default is true.  Library will disconnect and forget currently connected AP if it's not in the AP list.
  wifiMulti.setAllowOpenAP(true);                           // Default is false.  True adds open APs to the AP list.
  wifiMulti.setConnectionTestCallbackFunc(testConnection);  // Attempts to connect to a remote webserver in case of captive portals.

  Serial.println("Connecting Wifi...");
  if (wifiMulti.run() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void loop() {
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
