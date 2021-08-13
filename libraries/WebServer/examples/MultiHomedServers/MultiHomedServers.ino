#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

/*
 * MultiHomedServers
 * 
 * MultiHomedServers tests support for multi-homed servers, i.e. a distinct web servers on each IP interface. 
 * It only tests the case n=2 because on a basic ESP32 device, we only have two IP interfaces, namely
 * the WiFi station interfaces and the WiFi soft AP interface.
 *
 * For this to work, the WebServer and the WiFiServer classes must correctly handle the case where an 
 * IP address is passed to their relevant constructor. It also requires WebServer to work with multiple,
 * simultaneous instances.
 * 
 * Testing the WebServer and the WiFiServer constructors was the primary purpose of this script.
 * The part of WebServer used by this sketch does seem to work with multiple, simultaneous instances.
 * However there is much functionality in WebServer that is not tested here. It may all be well, but
 * that is not proven here.
 * 
 * This sketch starts the mDNS server, as did HelloServer, and it resolves esp32.local on both interfaces,
 * but was not otherwise tested.
 * 
 * This script also tests that a server not bound to a specific IP address still works.
 * 
 * We create three, simultaneous web servers, one specific to each interface and one that listens on both:
 * 
 *  name    IP Address      Port
 *  ----    ----------      ----
 *  server0 INADDR_ANY      8080
 *  server1 station address 8081
 *  server2 soft AP address 8081
 *  
 *  The expected responses to a brower's requests are as follows:
 *  
 *  1. when client connected to the same WLAN as the station:
 *      Request URL                 Response
 *      -----------                 --------
 *      http://stationaddress:8080  "hello from server0"
 *      http://stationaddress:8081  "hello from server1"
 *      
 *  2. when client is connected to the soft AP:
 *  
 *      Request URL                 Response
 *      -----------                 --------
 *      http://softAPaddress:8080   "hello from server0"
 *      http://softAPaddress:8081   "hello from server2"
 *
 *  3. Repeat 1 and 2 above with esp32.local in place of stationaddress and softAPaddress, respectively.
 *  
 * MultiHomedServers was originally based on HelloServer.
 */

const char* ssid = "........";
const char* password = "........";
const char *apssid = "ESP32";

WebServer *server0, *server1, *server2;

const int led = 13;

void handleRoot(WebServer *server, const char *content) {
  digitalWrite(led, 1);
  server->send(200, "text/plain", content);
  digitalWrite(led, 0);
}

void handleRoot0() {
  handleRoot(server0, "hello from server0");
}

void handleRoot1() {
  handleRoot(server1, "hello from server1");
}

void handleRoot2() {
  handleRoot(server2, "hello from server2");
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
  if (!WiFi.softAP(apssid)) {
    Serial.println("failed to start softAP");
    for (;;) {
        digitalWrite(led, 1);
        delay(100);
        digitalWrite(led, 0);
        delay(100);
    }
  }
  Serial.print("Soft AP: ");
  Serial.print(apssid);
  Serial.print(" IP address: ");
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
}

void loop(void) {
  server0->handleClient();
  server1->handleClient();
  server2->handleClient();
  delay(2);//allow the cpu to switch to other tasks
}
