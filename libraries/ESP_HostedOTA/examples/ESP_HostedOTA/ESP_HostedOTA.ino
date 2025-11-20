#include "WiFi.h"
#include "ESP_HostedOTA.h"

const char *ssid = "your-ssid";          // Change this to your WiFi SSID
const char *password = "your-password";  // Change this to your WiFi password

void setup() {
  Serial.begin(115200);

  WiFi.STA.begin();

  Serial.println();
  Serial.println("******************************************************");
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.STA.connect(ssid, password);

  while (WiFi.STA.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.STA.localIP());

  if (updateEspHostedSlave()) {
  	// Currently it's required to restart the host
    ESP.restart();
  }
}

void loop() {
  delay(1000);
}
