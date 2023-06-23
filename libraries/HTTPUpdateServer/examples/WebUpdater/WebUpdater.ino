/*
  To upload through terminal you can use: curl -F "image=@firmware.bin" esp32-webupdate.local/update
*/

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPUpdateServer.h>

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

const char* host = "esp32-webupdate";

WebServer httpServer(80);
HTTPUpdateServer httpUpdater;

void setup(void) {

  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting Sketch...");
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    Serial.println("WiFi failed, retrying.");
  }

  MDNS.begin(host);
  if (MDNS.begin("esp32")) {
    Serial.println("mDNS responder started");
  }


  httpUpdater.setup(&httpServer);
  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", host);
}

void loop(void) {
  httpServer.handleClient();
}