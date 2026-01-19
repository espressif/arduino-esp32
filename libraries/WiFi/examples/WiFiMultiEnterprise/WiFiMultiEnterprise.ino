/*
 *  This sketch tries to Connect to the best AP based on a given list, where
 *  the list includes APs requiring WPA2-Enterprise using PEAPv0/EAP-MSCHAPv2
 *
 */

#include <WiFi.h>
#include <WiFiMulti.h>

WiFiMulti wifiMulti;

void setup() {
  Serial.begin(115200);
  delay(10);

  wifiMulti.addAP("ssid_from_AP_1", "your_password_for_AP_1"); // Non WPA2-Enterprise (e.g. WPA2-PSK)
  wifiMulti.addAP("ssid_from_AP_2", "your_password_for_AP_2", "your_username_for_AP_2", "your_identity_for_AP_2"); // With anonymous identity
  wifiMulti.addAP("ssid_from_AP_3", "your_password_for_AP_3", "your_username_for_AP_3", ""); // With blank anonymous identity

  Serial.println("Connecting Wifi...");
  if (wifiMulti.run() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}

void loop() {
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("WiFi not connected!");
    delay(1000);
  }
}
