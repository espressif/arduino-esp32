/*
  To upload through terminal you can use: curl -F "image=@firmware.bin" esp8266-webupdate.local/update
*/

#include <WiFi.h>
#include <NetworkClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

const char *host = "esp32-webupdate";
const char *ssid = "........";
const char *password = "........";

// Set the username and password for firmware upload
const char *authUser = "........";
const char *authPass = "........";

WebServer server(80);
const char *serverIndex =
  "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

const char *csrfHeaders[2] = {"Origin", "Host"};
static bool authenticated = false;

void setup(void) {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Booting Sketch...");
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() == WL_CONNECTED) {
    MDNS.begin(host);
    server.collectHeaders(csrfHeaders, 2);
    server.on("/", HTTP_GET, []() {
      if (!server.authenticate(authUser, authPass)) {
        return server.requestAuthentication();
      }
      server.sendHeader("Connection", "close");
      server.send(200, "text/html", serverIndex);
    });
    server.on(
      "/update", HTTP_POST,
      []() {
        if (!authenticated) {
          return server.requestAuthentication();
        }
        server.sendHeader("Connection", "close");
        if (Update.hasError()) {
          server.send(200, "text/plain", "FAIL");
        } else {
          server.send(200, "text/plain", "Success! Rebooting...");
          delay(500);
          ESP.restart();
        }
      },
      []() {
        HTTPUpload &upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
          Serial.setDebugOutput(true);
          authenticated = server.authenticate(authUser, authPass);
          if (!authenticated) {
            Serial.println("Authentication fail!");
            return;
          }
          String origin = server.header(String(csrfHeaders[0]));
          String host = server.header(String(csrfHeaders[1]));
          String expectedOrigin = String("http://") + host;
          if (origin != expectedOrigin) {
            Serial.printf("Wrong origin received! Expected: %s, Received: %s\n", expectedOrigin.c_str(), origin.c_str());
            authenticated = false;
            return;
          }

          Serial.printf("Update: %s\n", upload.filename.c_str());
          if (!Update.begin()) {  //start with max available size
            Update.printError(Serial);
          }
        } else if (authenticated && upload.status == UPLOAD_FILE_WRITE) {
          if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
          }
        } else if (authenticated && upload.status == UPLOAD_FILE_END) {
          if (Update.end(true)) {  //true to set the size to the current progress
            Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
          } else {
            Update.printError(Serial);
          }
          Serial.setDebugOutput(false);
        } else if (authenticated) {
          Serial.printf("Update Failed Unexpectedly (likely broken connection): status=%d\n", upload.status);
        }
      }
    );
    server.begin();
    MDNS.addService("http", "tcp", 80);

    Serial.printf("Ready! Open http://%s.local in your browser\n", host);
  } else {
    Serial.println("WiFi Failed");
  }
}

void loop(void) {
  server.handleClient();
  delay(2);  //allow the cpu to switch to other tasks
}
