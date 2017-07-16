#include <Ethernet.h>
#include "WiFi.h"
#include <ESP32WebServer.h>

// declare the ethernet adapter
ESP32Ethernet ethernet;
// define a webserver on port 80
ESP32WebServer server(80);

void setup(){
  Serial.begin(115200);

  // start the ethernet adapter (DHCP)
  ethernet.begin();

  // attach handles
  server.on("/",handleRoot);   
  server.on("/test",handleTest);   

  // start the server
  server.begin();
}

void handleRoot()
{
  String html = "You just loaded the root of your ESP WebServer<br><br><a href=\"/test\">Goto /test</a>";
  server.setContentLength(html.length());
  server.send(200,"text/html",html);
}

void handleTest()
{
  String html = "You just entered /test ...";
  server.setContentLength(html.length());
  server.send(200,"text/html",html);
}

void loop(){
  // hanle clients
  server.handleClient();
}
