#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WebServer.h>

const char* ssid = "........";
const char* password = "........";

WebServer server(80);

typedef struct credentials_t {
  String username;
  String password;
} credentials_t;

credentials_t passwdfile[] = {
  { "admin", "esp32" },
  { "fred", "41234123" },
  { "charlie", "sdfsd" },
  { "alice", "vambdnkuhj" },
  { "bob", "svcdbjhws12" },
};
const size_t N_CREDENTIALS = sizeof(passwdfile) / sizeof(credentials_t);

String* credentialsHandler(HTTPAuthMethod mode, String username, String params[]) {
  for (int i = 0; i < N_CREDENTIALS; i++) {
    if (username == passwdfile[i].username)
      return new String(passwdfile[i].password);
  }
  return NULL;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi Connect Failed! Rebooting...");
    delay(1000);
    ESP.restart();
  }
  ArduinoOTA.begin();

  server.on("/", []() {
    if (!server.authenticate(&credentialsHandler)) {
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
