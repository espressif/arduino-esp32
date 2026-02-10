#include <WiFi.h>

String ssid = "";
String password = "";

void readWiFiCredentials() {
  Serial.println("[AP] Waiting for WiFi credentials...");
  Serial.println("[AP] Send SSID:");

  // Wait for SSID
  while (ssid.length() == 0) {
    if (Serial.available()) {
      ssid = Serial.readStringUntil('\n');
      ssid.trim();
    }
    delay(100);
  }

  Serial.println("[AP] Send Password:");

  // Wait for password (allow empty password)
  bool password_received = false;
  while (!password_received) {
    if (Serial.available()) {
      password = Serial.readStringUntil('\n');
      password.trim();
      password_received = true;  // Accept even empty password
    }
    delay(100);
  }

  Serial.print("[AP] SSID: ");
  Serial.println(ssid);
  Serial.print("[AP] Password: ");
  Serial.println(password);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  // Wait for test to be ready
  Serial.println("[AP] Device ready for WiFi credentials");

  // Read WiFi credentials from serial
  readWiFiCredentials();

  WiFi.AP.begin();
  bool ok = WiFi.AP.create(ssid, password);

  if (!ok) {
    Serial.println("[AP] Failed to start AP");
    return;
  }

  IPAddress ip = WiFi.AP.localIP();
  Serial.printf("[AP] Started SSID=%s Password=%s IP=%s\n", ssid.c_str(), password.c_str(), ip.toString().c_str());
}

void loop() {
  // periodically report stations
  static unsigned long last = 0;
  if (millis() - last > 3000) {
    last = millis();
    int count = WiFi.AP.stationCount();
    Serial.printf("[AP] Stations connected: %d\n", count);
  }
}
