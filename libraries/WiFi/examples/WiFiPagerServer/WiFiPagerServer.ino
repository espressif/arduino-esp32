/*
  WiFi Pager Server - server.available
  and print-to-all-clients example

  The example is a simple server that echoes any incoming
  messages to all connected clients. Connect two or more
  telnet sessions to see how server.available() and
  server.print() work.

  created in September 2020
  by Juraj Andrassy
 */

#include <WiFi.h>

WiFiServer server(2323);

#include "arduino_secrets.h"
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)

void setup() {

  Serial.begin(115200);
  while (!Serial);

  Serial.print("Attempting to connect to SSID \"");
  Serial.print(ssid);
  Serial.println("\" with DHCP ...");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println();

  server.begin();

  IPAddress ip = WiFi.localIP();
  Serial.println();
  Serial.println("Connected to WiFi network.");
  Serial.print("To access the server, connect with Telnet client to ");
  Serial.print(ip);
  Serial.println(" 2323");
}

void loop() {

  WiFiClient client = server.available();     // returns first client which has data to read or a 'false' client
  if (client) {                               // client is here true only if it is connected and has data to read
    String s = client.readStringUntil('\n');  // read the message incoming from one of the clients
    s.trim();                                 // trim eventual \r
    Serial.println(s);                        // print the message to Serial Monitor
    client.print("echo: ");                   // this is only for the sending client
    server.println(s);                        // send the message to all connected clients
    server.flush();                           // flush the buffers
  }
}
