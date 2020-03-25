/*
 * ClientHTTP example sketch
 * 
 * GET using Basic Authorization
 * 
 * This example sketch demonstrates how to use the ClientHTTP library
 * to fetch from a web server using GET and Basic Authorization. 
 * 
 * Created by Jeroen Döll on 25 March 2020
 * This example is in the public domain
 */

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#if defined(ESP32)
#include <WiFi.h>
#endif

#include <WiFiClient.h>

#include <ClientHTTP.h>

#ifndef STASSID
#define STASSID "your-ssid"
#define STAPSK  "your-password"
#endif

const char * ssid = STASSID;
const char * password = STAPSK;


void setup() {
  Serial.begin(115200);
  Serial.println();

  Serial.printf("Connecting to %s", ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Use WiFiClient class to create connection
  WiFiClient client;

  ClientHTTP http(client);
  http.setTimeout(10000);

  if (!http.connect("jigsaw.w3.org", 80)) {
    Serial.println("Connection failed");
    return;
  }

  if(!http.GET("/HTTP/connection.html")) {
    Serial.println("Failed to GET");
    return;
  }

  ClientHTTP::http_code_t status = http.status();

  if(status > 0) {
    Serial.printf("HTTP status code is %d\n", status);
    if(status == ClientHTTP::OK) {
      Serial.println("Page succesfully fetched!");
    }

    http.printTo(Serial);
    Serial.println();
  } else {
    Serial.printf("Request failed with error code %d\n", status);
  }
  
  Serial.println("Closing connection");
  http.stop();
}

void loop() {
}
