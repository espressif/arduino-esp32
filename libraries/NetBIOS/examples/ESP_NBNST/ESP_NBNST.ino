#include <WiFi.h>
#include <NetBIOS.h>

// To use secrets please read the documentation at https://espressif-docs.readthedocs-hosted.com/projects/arduino-esp32/en/latest/guides/secrets.html
#if __has_include("secrets.h")
  #include "secrets.h"
#endif

#ifdef SECRETS_WIFI_SSID_1
const char* ssid     = SECRETS_WIFI_SSID_1;
#else
const char* ssid     = "example-SSID1"; // Traditional way
#endif

#ifdef SECRETS_WIFI_PASSWORD_1
const char* password = SECRETS_WIFI_PASSWORD_1;
#else
const char* password = "example-password-1"; // Traditional way
#endif

void setup() {
  Serial.begin(115200);

  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  NBNS.begin("ESP");
}

void loop() {
  
}
