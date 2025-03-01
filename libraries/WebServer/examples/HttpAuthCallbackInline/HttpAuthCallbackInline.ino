#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>

const char *ssid = "........";
const char *password = "........";

WebServer server(80);

typedef struct credentials_t {
  const char *username;
  const char *password;
} credentials_t;

credentials_t passwdfile[] = {{"admin", "esp32"}, {"fred", "41234123"}, {"charlie", "sdfsd"}, {"alice", "vambdnkuhj"}, {"bob", "svcdbjhws12"}, {NULL, NULL}};

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
    if (!server.authenticate([](HTTPAuthMethod mode, String username, String params[]) -> String * {
          // Scan the password list for the username and return the password if
          // we find the username.
          //
          for (credentials_t *entry = passwdfile; entry->username; entry++) {
            if (username == entry->username) {
              return new String(entry->password);
            };
          };
          // we've not found the user in the list.
          return NULL;
        })) {
      server.requestAuthentication();
      return;
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
  delay(2);  //allow the cpu to switch to other tasks
}
