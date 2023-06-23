#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>

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

WebServer server(80);

const char* www_username = "admin";
const char* www_password = "esp32";

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Connect Failed! Rebooting...");
    delay(1000);
    ESP.restart();
  }
  ArduinoOTA.begin();

  server.on("/", []() {
    if (!server.authenticate(www_username, www_password)) {
      return server.requestAuthentication();
    }
    server.send(200, "text/plain", "Login OK");
  });
  server.begin();

  Serial.print("Open http://");
  Serial.print(WiFi.localIP());
  Serial.println("/ in your browser to see it working");
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  delay(2);//allow the cpu to switch to other tasks
}
