/*
This example enables catch-all Captive portal for ESP32 Access-Point
It will allow modern devices/OSes to detect that WiFi connection is
limited and offer a user to access a banner web-page.
There is no need to find and open device's IP address/URL, i.e. http://192.168.4.1/
This works for Android, Ubuntu, FireFox, Windows, maybe others...
*/

#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>

DNSServer dnsServer;
WebServer server(80);

static const char responsePortal[] = R"===(
<!DOCTYPE html><html><head><title>ESP32 CaptivePortal</title></head><body>
<h1>Hello World!</h1><p>This is a captive portal example page. All unknown http requests will
be redirected here.</p></body></html>
)===";

// index page handler
void handleRoot() {
  server.send(200, "text/plain", "Hello from esp32!");
}

// this will redirect unknown http req's to our captive portal page
// based on this redirect various systems could detect that WiFi AP has a captive portal page
void handleNotFound() {
  server.sendHeader("Location", "/portal");
  server.send(302, "text/plain", "redirect to captive portal");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32-DNSServer");

  // by default DNSServer is started serving any "*" domain name. It will reply
  // AccessPoint's IP to all DNS request (this is required for Captive Portal detection)
  if (dnsServer.start()) {
    Serial.println("Started DNS server in captive portal-mode");
  } else {
    Serial.println("Err: Can't start DNS server!");
  }

  // serve a simple root page
  server.on("/", handleRoot);

  // serve portal page
  server.on("/portal", []() {
    server.send(200, "text/html", responsePortal);
  });

  // all unknown pages are redirected to captive portal
  server.onNotFound(handleNotFound);
  server.begin();
}

void loop() {
  server.handleClient();
  delay(5);  // give CPU some idle time
}
