#include <WiFi.h>
#include <NetworkClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

const char *ssid = "WiFi_SSID";
const char *password = "WiFi_Password";
const char *apssid = "ESP32";

WebServer *server0, *server1, *server2;

#ifdef LED_BUILTIN
const int led = LED_BUILTIN;
#else
const int led = 13;
#endif

void handleRoot(WebServer *server, const char *content) {
  digitalWrite(led, 1);
  server->send(200, "text/plain", content);
  digitalWrite(led, 0);
}

void handleRoot0() {
  handleRoot(server0, "Hello from server0 who listens on both WLAN and own Soft AP");
}

void handleRoot1() {
  handleRoot(server1, "Hello from server1 who listens only on WLAN");
}

void handleRoot2() {
  handleRoot(server2, "Hello from server2 who listens only on own Soft AP");
}

void handleNotFound(WebServer *server) {
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server->uri();
  message += "\nMethod: ";
  message += (server->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server->args();
  message += "\n";
  for (uint8_t i = 0; i < server->args(); i++) {
    message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
  }
  server->send(404, "text/plain", message);
  digitalWrite(led, 0);
}

void handleNotFound0() {
  handleNotFound(server0);
}

void handleNotFound1() {
  handleNotFound(server1);
}

void handleNotFound2() {
  handleNotFound(server2);
}

void setup(void) {
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);

  Serial.println("Multi-homed Servers example starting");
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting ");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to \"");
  Serial.print(ssid);
  Serial.print("\", IP address: \"");
  Serial.println(WiFi.localIP());
  if (!WiFi.softAP(apssid)) {
    Serial.println("failed to start softAP");
    for (;;) {
      digitalWrite(led, 1);
      delay(100);
      digitalWrite(led, 0);
      delay(100);
    }
  }
  Serial.print("Soft AP SSID: \"");
  Serial.print(apssid);
  Serial.print("\", IP address: ");
  Serial.println(WiFi.softAPIP());

  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  server0 = new WebServer(8080);
  server1 = new WebServer(WiFi.localIP(), 8081);
  server2 = new WebServer(WiFi.softAPIP(), 8081);

  server0->on("/", handleRoot0);
  server1->on("/", handleRoot1);
  server2->on("/", handleRoot2);

  server0->onNotFound(handleNotFound0);
  server1->onNotFound(handleNotFound1);
  server2->onNotFound(handleNotFound2);

  server0->begin();
  Serial.println("HTTP server0 started");
  server1->begin();
  Serial.println("HTTP server1 started");
  server2->begin();
  Serial.println("HTTP server2 started");

  Serial.printf("SSID: %s\n\thttp://", ssid);
  Serial.print(WiFi.localIP());
  Serial.print(":8080\n\thttp://");
  Serial.print(WiFi.localIP());
  Serial.println(":8081");
  Serial.printf("SSID: %s\n\thttp://", apssid);
  Serial.print(WiFi.softAPIP());
  Serial.print(":8080\n\thttp://");
  Serial.print(WiFi.softAPIP());
  Serial.println(":8081");
  Serial.printf("Any of the above SSIDs\n\thttp://esp32.local:8080\n\thttp://esp32.local:8081\n");
}

void loop(void) {
  server0->handleClient();
  server1->handleClient();
  server2->handleClient();
  delay(2);  //allow the cpu to switch to other tasks
}
