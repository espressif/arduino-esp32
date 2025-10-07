/*
 * This example demonstrates how to send an HTTP response using chunks
 * It will create an HTTP Server (port 80) associated with an a MDNS service
 * Access the HTTP server using a Web Browser:
 * URL can be composed using the MDNS name "esp32_chunk_resp.local"
 * http://esp32_chunk_resp.local/
 * or the IP Address that will be printed out, such as for instance 192.168.1.10
 * http://192.168.1.10/
 *
 * ESP32 Server response can also be viewed using the curl command:
 * curl -i esp32_chunk_resp.local:80
 * curl -i --raw esp32_chunk_resp.local:80
 */

#include <WiFi.h>
#include <NetworkClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

const char *ssid = "........";
const char *password = "........";

WebServer server(80);

void handleChunks() {
  uint8_t countDown = 10;
  server.chunkResponseBegin();
  char countContent[8];
  while (countDown) {
    sprintf(countContent, "%d...\r\n", countDown--);
    server.chunkWrite(countContent, strlen(countContent));
    // count down shall show up in the browser only after about 5 seconds when finishing the whole transmission
    // using "curl -i esp32_chunk_resp.local:80", it will show the count down as it sends each chunk
    delay(500);
  }
  server.chunkWrite("DONE!", 5);
  server.chunkResponseEnd();
}

void setup(void) {
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

  // Use the URL: http://esp32_chunk_resp.local/
  if (MDNS.begin("esp32_chunk_resp")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleChunks);

  server.onNotFound([]() {
    server.send(404, "text/plain", "Page not found");
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop(void) {
  server.handleClient();
  delay(2);  //allow the cpu to switch to other tasks
}
