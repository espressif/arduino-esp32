#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

const char *ssid = "........";
const char *password = "........";

WebServer server(80);

void setup(void) {
  Serial.begin(9600);
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

  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", []() {
    server.send(200, "text/plain", "hello from esp32!");
  });

  server.on("/users/{}", []() {
    String user = server.pathArg(0);
    server.send(200, "text/plain", "User: '" + user + "'");
  });
  
  server.on("/users/{}/devices/{}", []() {
    String user = server.pathArg(0);
    String device = server.pathArg(1);
    server.send(200, "text/plain", "User: '" + user + "' and Device: '" + device + "'");
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
}
